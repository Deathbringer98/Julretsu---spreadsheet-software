#include "julretsu/SheetHealth.hpp"
#include "julretsu/I18n.hpp"
#include "julretsu/WorksheetOps.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <map>
namespace julretsu {
namespace {
std::string a1(CellCoord c) { auto r=to_a1(c); return std::get<std::string>(r); }
std::string column_name(std::uint32_t column) { auto name=a1({0,column}); return name.substr(0,name.size()-1); }
std::string amount(double n) {
    char buffer[64];
    if(std::abs(n)>=1e15||(n!=0&&std::abs(n)<1e-4)) std::snprintf(buffer,sizeof buffer,"%.4g",n);
    else if(std::floor(n)==n) std::snprintf(buffer,sizeof buffer,"%.0f",n);
    else std::snprintf(buffer,sizeof buffer,"%.4g",n);
    // Thousands separators make magnitude mistakes easier to see.
    std::string text=buffer; auto dot=text.find_first_of(".e"); if(dot==std::string::npos) dot=text.size();
    for(auto i=int(dot)-3;i>(text[0]=='-'?1:0);i-=3) text.insert(std::size_t(i),",");
    return text;
}
const double* number_input(const Cell& cell) { return std::get_if<double>(&cell.input); }
bool numeric_value(const Cell& cell) { return std::holds_alternative<double>(cell.cached); }
bool has_total_call(const Cell& cell) {
    if(!cell.formula) return false;
    for(const auto& node:cell.formula->nodes) if(node.kind==NodeKind::Call&&(node.op=="SUM"||node.op=="AVERAGE")) return true;
    return false;
}
// Column-major view of the populated cells.
using Columns=std::map<std::uint32_t,std::map<std::uint32_t,const Cell*>>;
Columns by_column(const Sheet& sheet) {
    Columns columns;
    for(const auto& [r,row]:sheet.populated_rows()) for(const auto& [c,cell]:row) columns[c][r]=&cell;
    return columns;
}
double median(std::vector<double> values) {
    std::nth_element(values.begin(),values.begin()+std::ptrdiff_t(values.size()/2),values.end());
    return values[values.size()/2];
}
// Parses text that is really a number: "1,200", "$50", " 12 ", "15%".
std::optional<double> text_number(std::string_view text) {
    while(!text.empty()&&text.front()==' ') text.remove_prefix(1);
    while(!text.empty()&&text.back()==' ') text.remove_suffix(1);
    if(text.empty()||text.size()>40) return std::nullopt;
    bool percent=false, negative=false;
    if(text.front()=='-') { negative=true; text.remove_prefix(1); }
    if(!text.empty()&&text.front()=='$') text.remove_prefix(1);
    if(!text.empty()&&text.back()=='%') { percent=true; text.remove_suffix(1); }
    if(text.empty()) return std::nullopt;
    // Leading zeros usually mean an identifier (ZIP code, account number), not an amount.
    if(text.size()>1&&text[0]=='0'&&text[1]!='.') return std::nullopt;
    std::string digits; const auto dot=text.find('.'); const auto whole=text.substr(0,dot);
    if(whole.find(',')!=std::string_view::npos) {
        // Commas must be real thousands separators: 1,200 or 12,345,678.
        std::size_t group=0; bool first=true;
        for(std::size_t i=whole.size();i-->0;) {
            if(whole[i]==',') { if(group!=3) return std::nullopt; group=0; first=false; continue; }
            ++group;
        }
        if(first||group==0||group>3) return std::nullopt;
    }
    for(char c:text) if(c!=',') digits+=c;
    double n{}; auto [end,e]=parse_double(digits.data(),digits.data()+digits.size(),n);
    if(e!=std::errc{}||end!=digits.data()+digits.size()||!std::isfinite(n)) return std::nullopt;
    if(negative) n=-n; if(percent) n/=100;
    return n;
}
}
std::string formula_signature(const ParsedFormula& formula,CellCoord at) {
    std::string signature;
    auto offset=[&](CellCoord c) { signature+='('+std::to_string(std::int64_t(c.row)-at.row)+','+std::to_string(std::int64_t(c.column)-at.column)+')'; };
    for(const auto& node:formula.nodes) {
        signature+=char('a'+int(node.kind)); signature+=node.op;
        if(node.kind==NodeKind::Reference) offset(node.first);
        else if(node.kind==NodeKind::Range) { offset(node.first); offset(node.last); }
        else if(node.kind==NodeKind::Literal) signature+=display(node.literal);
        for(auto child:node.children) signature+=':'+std::to_string(child);
        signature+=';';
    }
    return signature;
}
std::optional<std::string> outlier_note(const Sheet& sheet,CellCoord coord) {
    const auto* cell=sheet.cell(coord); if(!cell) return std::nullopt;
    const auto* n=number_input(*cell); if(!n||*n==0) return std::nullopt;
    std::vector<double> others;
    for(const auto& [r,row]:sheet.populated_rows()) {
        auto it=row.find(coord.column);
        if(r!=coord.row&&it!=row.end()) if(auto v=number_input(it->second);v&&*v!=0) others.push_back(std::abs(*v));
    }
    if(others.size()<5) return std::nullopt;
    const double typical=median(others), ratio=std::abs(*n)/typical;
    if(ratio<50&&ratio>1.0/50) return std::nullopt;
    const auto scale=ratio>=1000?std::string(tr("over 1,000x")):trf("about %.0fx",ratio);
    if(ratio>=50) return trf("%s is %s, %s the typical value in column %s (%s). Check for extra digits.",
        a1(coord).c_str(),amount(*n).c_str(),scale.c_str(),column_name(coord.column).c_str(),amount(typical).c_str());
    return trf("%s is %s, far smaller than the typical value in column %s (%s). Check for a missing digit or decimal point.",
        a1(coord).c_str(),amount(*n).c_str(),column_name(coord.column).c_str(),amount(typical).c_str());
}
std::vector<HealthIssue> check_sheet_health(const Sheet& sheet,std::size_t max_issues) {
    std::vector<HealthIssue> issues;
    auto add=[&](HealthIssue issue) { if(issues.size()<max_issues) issues.push_back(std::move(issue)); };
    const auto columns=by_column(sheet);

    // 1. A cell that breaks the formula pattern shared by the cells directly above and below it.
    for(const auto& [c,cells]:columns) {
        for(auto it=cells.begin();it!=cells.end();++it) {
            const auto r=it->first; if(r==0) continue;
            auto above=cells.find(r-1), below=cells.find(r+1);
            if(above==cells.end()||below==cells.end()) continue;
            const Cell& up=*above->second; const Cell& down=*below->second; const Cell& here=*it->second;
            if(!up.formula||!down.formula||up.formula->error||down.formula->error) continue;
            const auto pattern=formula_signature(*up.formula,{r-1,c});
            if(pattern!=formula_signature(*down.formula,{r+1,c})) continue;
            HealthIssue issue; issue.kind=HealthKind::InconsistentFormula; issue.cell={r,c}; issue.serious=true;
            const auto source=std::get<FormulaInput>(up.input).source;
            if(here.formula&&!here.formula->error) {
                if(formula_signature(*here.formula,{r,c})==pattern) continue;
                issue.title=trf("%s's formula differs from its neighbours",a1({r,c}).c_str());
                issue.detail=trf("The cells above and below follow one pattern (%s in %s), but %s uses %s.",source.c_str(),a1({r-1,c}).c_str(),a1({r,c}).c_str(),std::get<FormulaInput>(here.input).source.c_str());
            } else if(number_input(here)) {
                issue.title=trf("%s is a typed number inside a column of formulas",a1({r,c}).c_str());
                issue.detail=trf("The cells above and below are calculated, but %s holds a fixed %s. It will not update when its inputs change.",a1({r,c}).c_str(),amount(*number_input(here)).c_str());
            } else continue;
            const auto repaired=adjust_references(source,1,0);
            if(!parse_formula(repaired,{}).error) {
                issue.fix_label=trf("Use %s",repaired.c_str());
                issue.fix.cells.push_back({{r,c},FormulaInput{repaired}});
            }
            add(std::move(issue));
        }
    }
    // 2. SUM/AVERAGE ranges that stop just short of adjoining numbers.
    for(const auto& [r,row]:sheet.populated_rows()) for(const auto& [c,cell]:row) {
        if(!cell.formula||cell.formula->error) continue;
        for(const auto& node:cell.formula->nodes) {
            if(node.kind!=NodeKind::Call||(node.op!="SUM"&&node.op!="AVERAGE")) continue;
            for(auto child:node.children) {
                const auto& range=cell.formula->nodes[child];
                if(range.kind!=NodeKind::Range) continue;
                const bool vertical=range.first.column==range.last.column, horizontal=range.first.row==range.last.row;
                if(vertical==horizontal) continue; // single cell or 2-D block
                auto numeric_at=[&](CellCoord p) {
                    if(p==CellCoord{r,c}) return false;
                    const auto* other=sheet.cell(p); return other&&numeric_value(*other)&&!has_total_call(*other);
                };
                CellCoord end=range.last; std::uint32_t missed=0;
                while(missed<100000) {
                    CellCoord next=vertical?CellCoord{end.row+1,end.column}:CellCoord{end.row,end.column+1};
                    if(!next.valid()||!numeric_at(next)) break;
                    end=next; ++missed;
                }
                if(!missed) continue;
                HealthIssue issue; issue.kind=HealthKind::TotalMissesRows; issue.cell={r,c}; issue.serious=true;
                const auto old_range=a1(range.first)+":"+a1(range.last), new_range=a1(range.first)+":"+a1(end);
                issue.title=vertical?trf("%s's %s leaves out %u row%s",a1({r,c}).c_str(),node.op.c_str(),missed,missed==1?"":"s"):trf("%s's %s leaves out %u column%s",a1({r,c}).c_str(),node.op.c_str(),missed,missed==1?"":"s");
                issue.detail=missed==1?trf("It covers %s, but %s is a number right next to that range and is not included.",old_range.c_str(),a1(end).c_str()):trf("It covers %s, but the numbers continue to %s and are not included.",old_range.c_str(),a1(end).c_str());
                auto source=std::get<FormulaInput>(cell.input).source, upper=source;
                std::transform(upper.begin(),upper.end(),upper.begin(),[](unsigned char ch){ return char(std::toupper(ch)); });
                if(const auto at=upper.find(old_range);at!=std::string::npos&&upper.find(old_range,at+1)==std::string::npos) {
                    source.replace(at,old_range.size(),new_range);
                    issue.fix_label=trf("Extend to %s",new_range.c_str()); issue.fix.cells.push_back({{r,c},FormulaInput{source}});
                }
                add(std::move(issue));
            }
        }
    }
    // 3. Numbers wildly out of scale with the rest of their column (extra or missing digits).
    for(const auto& [c,cells]:columns) {
        std::vector<double> values;
        for(const auto& [r,cell]:cells) { (void)r; if(auto n=number_input(*cell);n&&*n!=0) values.push_back(std::abs(*n)); }
        if(values.size()<6) continue;
        const double typical=median(values); int reported=0;
        for(const auto& [r,cell]:cells) {
            auto n=number_input(*cell); if(!n||*n==0) continue;
            const double ratio=std::abs(*n)/typical; if(ratio<50&&ratio>1.0/50) continue;
            if(++reported>3) break;
            HealthIssue issue; issue.kind=HealthKind::Outlier; issue.cell={r,c};
            issue.title=trf("%s looks out of scale (%s)",a1({r,c}).c_str(),amount(*n).c_str());
            issue.detail=*outlier_note(sheet,{r,c});
            add(std::move(issue));
        }
    }
    // 4. Numbers stored as text, which SUM, AVERAGE and sorting treat as words.
    for(const auto& [r,row]:sheet.populated_rows()) for(const auto& [c,cell]:row) {
        const auto* text=std::get_if<std::string>(&cell.input); if(!text) continue;
        auto value=text_number(*text); if(!value) continue;
        HealthIssue issue; issue.kind=HealthKind::NumberAsText; issue.cell={r,c};
        issue.title=trf("%s is a number stored as text (\"%s\")",a1({r,c}).c_str(),text->c_str());
        issue.detail=tr("Totals, averages and sorting skip text, so this value is silently left out of calculations.");
        issue.fix_label=trf("Convert to %s",amount(*value).c_str()); issue.fix.cells.push_back({{r,c},*value});
        add(std::move(issue));
    }
    // 5. A single blank row splitting a table: sorting and filtering stop at the gap.
    const auto& rows=sheet.populated_rows();
    for(auto it=rows.begin();it!=rows.end();++it) {
        auto next=std::next(it); if(next==rows.end()) break;
        if(next->first!=it->first+2||it->second.size()<2||next->second.size()<2) continue;
        bool totals_row=false; for(const auto& [c,cell]:next->second) { (void)c; if(has_total_call(cell)) totals_row=true; }
        if(totals_row) continue; // a spacer above a totals row is intentional
        std::size_t shared=0, shared_numbers=0;
        for(const auto& [c,cell]:it->second) if(auto other=next->second.find(c);other!=next->second.end()) {
            ++shared; if(numeric_value(cell)&&numeric_value(other->second)) ++shared_numbers;
        }
        if(shared<2||!shared_numbers) continue; // different columns or a new heading: not the same table
        const auto blank=it->first+1;
        HealthIssue issue; issue.kind=HealthKind::BlankRowInTable; issue.cell={blank,it->second.begin()->first};
        issue.title=trf("Row %u is blank inside a table",unsigned(blank+1));
        issue.detail=trf("Rows %u and %u hold the same kind of data. Sorting and filtering stop at the gap, which can split or scramble the table.",unsigned(it->first+1),unsigned(next->first+1));
        issue.fix_label=trf("Delete blank row %u",unsigned(blank+1)); issue.delete_row=true; issue.row=blank;
        add(std::move(issue));
    }
    std::stable_sort(issues.begin(),issues.end(),[](const HealthIssue& a,const HealthIssue& b){ return a.serious>b.serious; });
    return issues;
}
}
