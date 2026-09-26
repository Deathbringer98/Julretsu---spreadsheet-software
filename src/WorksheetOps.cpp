#include "julretsu/WorksheetOps.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <numeric>
#include "julretsu/I18n.hpp"
namespace julretsu {
std::string adjust_references(std::string_view source,int dr,int dc,int axis,unsigned position,bool deleting) {
    std::string out; bool quoted=false;
    for(std::size_t i=0;i<source.size();) {
        if(source[i]=='"') { out+=source[i++]; if(quoted&&i<source.size()&&source[i]=='"') {out+=source[i++];continue;} quoted=!quoted;continue; }
        if(quoted|| (i&& (std::isalnum(static_cast<unsigned char>(source[i-1]))||source[i-1]=='_'))) {out+=source[i++];continue;}
        auto begin=i,j=i; bool ac=false,ar=false;
        if(j<source.size()&&source[j]=='$') {ac=true;++j;}
        auto letters=j; while(j<source.size()&&std::isalpha(static_cast<unsigned char>(source[j]))) ++j;
        auto endletters=j; if(j<source.size()&&source[j]=='$') {ar=true;++j;}
        auto digits=j; while(j<source.size()&&std::isdigit(static_cast<unsigned char>(source[j]))) ++j;
        if(letters==endletters||digits==j||(j<source.size()&&(std::isalnum(static_cast<unsigned char>(source[j]))||source[j]=='_'||source[j]=='('))) {out+=source[i++];continue;}
        std::string plain(source.substr(letters,endletters-letters)); plain+=source.substr(digits,j-digits);
        auto parsed=from_a1(plain); auto coord=std::get_if<CellCoord>(&parsed);
        if(!coord) {out+=source[i++];continue;}
        long long r=coord->row,c=coord->column;
        if(axis<0) {if(!ar) r+=dr;if(!ac) c+=dc;}
        else {
            auto& n=axis==0?r:c;
            if(deleting&&n==position) throw std::runtime_error("A formula references the row or column being deleted. Remove that reference first.");
            if(n>=position) n+=deleting?-1:1;
        }
        if(r<0||r>=max_rows||c<0||c>=max_columns) throw std::runtime_error("This operation would move a formula reference outside the sheet.");
        auto address=std::get<std::string>(to_a1({unsigned(r),unsigned(c)})); auto split=address.find_first_of("0123456789");
        if(ac) out+='$'; out+=address.substr(0,split); if(ar) out+='$';out+=address.substr(split); i=j;(void)begin;
    }
    return out;
}
Batch structural_edit(const Sheet& sheet,bool rows,unsigned position,bool deleting) {
    if(!sheet.tables().empty())throw std::runtime_error(tr("Remove the table definition before structural edits. Use Add table row to extend it."));
    if(!sheet.validation_rules().empty()) throw std::runtime_error("Remove validation rules before inserting or deleting rows or columns.");
    Batch b; auto move=[&](CellCoord c)->std::optional<CellCoord> {
        auto& n=rows?c.row:c.column; auto limit=rows?max_rows:max_columns;
        if(deleting&&n==position) return {};
        if(n>=position) { if(!deleting&&n==limit-1) throw std::runtime_error("Insertion would discard cells at the sheet boundary."); n=deleting?n-1:n+1; }
        return c;
    };
    for(const auto& [r,row]:sheet.populated_rows()) for(const auto& [c,cell]:row) b.cells.push_back({{r,c},std::monostate{}});
    for(const auto& [r,row]:sheet.populated_rows()) for(const auto& [c,cell]:row) if(auto target=move({r,c})) {
        Input input=cell.input;
        if(auto f=std::get_if<FormulaInput>(&input)) {
            if(f->source.find("LUA")!=std::string::npos||f->source.find("lua")!=std::string::npos) throw std::runtime_error("Structural edits cannot safely rewrite Lua cell references.");
            f->source=adjust_references(f->source,0,0,rows?0:1,position,deleting);
        }
        b.cells.push_back({*target,std::move(input)});
    }
    for(const auto& [c,style]:sheet.cell_styles()) b.formats.push_back({c,{}});
    for(const auto& [c,style]:sheet.cell_styles()) if(auto target=move(c)) b.formats.push_back({*target,style});
    if(rows) {
        for(const auto& [r,style]:sheet.row_styles()) b.rows.push_back({r,{}});
        for(const auto& [r,style]:sheet.row_styles()) if(auto target=move({r,0})) b.rows.push_back({target->row,style});
    }
    return b;
}
Batch sort_range(const Sheet& sheet,CellCoord a,CellCoord z,unsigned key,bool descending) {
    if(a.row>z.row||a.column>z.column||key<a.column||key>z.column||std::uint64_t(z.row-a.row+1)*(z.column-a.column+1)>10000) throw std::runtime_error("Select at most 10,000 cells to sort.");
    std::vector<unsigned> order(z.row-a.row+1);std::iota(order.begin(),order.end(),a.row);
    std::stable_sort(order.begin(),order.end(),[&](unsigned x,unsigned y) {
        auto vx=sheet.read({x,key}),vy=sheet.read({y,key});
        bool ex=std::holds_alternative<std::monostate>(vx),ey=std::holds_alternative<std::monostate>(vy);
        if(ex!=ey)return !ex;
        auto nx=std::get_if<double>(&vx),ny=std::get_if<double>(&vy);
        if(nx&&ny)return descending?*nx>*ny:*nx<*ny;
        return descending?display(vx)>display(vy):display(vx)<display(vy);
    });
    Batch b;
    for(unsigned i=0;i<order.size();++i) for(unsigned c=a.column;c<=z.column;++c) {
        auto cell=sheet.cell({order[i],c}); Input input=cell?cell->input:Input{};
        if(auto f=std::get_if<FormulaInput>(&input)) f->source=adjust_references(f->source,int(a.row+i)-int(order[i]),0);
        b.cells.push_back({{a.row+i,c},std::move(input)}); b.formats.push_back({{a.row+i,c},sheet.cell_style({order[i],c})});
    }
    return b;
}
std::string quote_clipboard(std::string_view text) {
    if(text.find_first_of("\t\r\n\"")==std::string_view::npos)return std::string(text);
    std::string result="\""; for(char c:text) {result+=c;if(c=='"')result+='"';}return result+'"';
}
std::vector<std::vector<std::string>> parse_clipboard(std::string_view text) {
    if(text.size()>8*1024*1024) throw std::runtime_error("Clipboard exceeds 8 MiB.");
    std::vector<std::vector<std::string>> rows(1); std::string field; bool quoted=false,closed=false; std::size_t count=0;
    auto emit=[&] {if(++count>10000)throw std::runtime_error("Paste at most 10,000 cells.");rows.back().push_back(std::move(field));field.clear();closed=false;};
    for(std::size_t i=0;i<text.size();++i) {char c=text[i];
        if(quoted) {if(c=='"') {if(i+1<text.size()&&text[i+1]=='"'){field+='"';++i;}else {quoted=false;closed=true;}} else field+=c;continue;}
        if(c=='\t')emit();
        else if(c=='\r'||c=='\n') {emit();if(c=='\r'&&i+1<text.size()&&text[i+1]=='\n')++i;if(i+1<text.size())rows.emplace_back();}
        else if(c=='"'&&field.empty()&&!closed)quoted=true;
        else {if(closed)throw std::runtime_error("Malformed quoted clipboard text.");field+=c;}
    }
    if(quoted)throw std::runtime_error("Unclosed clipboard quotation.");
    if(!text.empty()&&text.back()!='\n'&&text.back()!='\r')emit();
    return rows;
}
}

namespace julretsu {
namespace {
std::string table_address(CellCoord c){return std::get<std::string>(to_a1(c));}
void table_totals(const Sheet& sheet,const StructuredTable& t,Batch& b){
    if(!t.totals)return;
    for(unsigned c=t.first.column;c<=t.last.column;++c){
        bool numbers=false;for(unsigned r=t.first.row+1;r<=t.last.row;++r)if(std::holds_alternative<double>(sheet.read({r,c})))numbers=true;
        Input input=numbers?Input{FormulaInput{"=SUM("+table_address({t.first.row+1,c})+":"+table_address({t.last.row,c})+")"}}:Input{};
        b.cells.push_back({{t.last.row+1,c},input});auto style=sheet.cell_style({t.last.row,c});style.bold=true;style.border=true;b.formats.push_back({{t.last.row+1,c},style});
    }
}
}
Batch create_table(const Sheet& sheet,StructuredTable t){
    if(!t.first.valid()||!t.last.valid()||t.first.row>=t.last.row||t.first.column>t.last.column||t.last.row+unsigned(t.totals)>=max_rows||std::uint64_t(t.last.row-t.first.row+1)*(t.last.column-t.first.column+1)>100000)throw std::runtime_error(tr("Select a header and at least one data row, up to 100,000 cells."));
    std::set<std::string> headers;for(unsigned c=t.first.column;c<=t.last.column;++c){auto text=display(sheet.read({t.first.row,c}));if(text.empty()||!headers.insert(text).second)throw std::runtime_error(tr("Table headers must be nonempty and unique."));
        if(t.totals&&sheet.cell({t.last.row+1,c}))throw std::runtime_error(tr("The space below this table must be empty."));}
    Batch b;b.tables=sheet.tables();b.tables->push_back(t);
    for(unsigned c=t.first.column;c<=t.last.column;++c){auto style=sheet.cell_style({t.first.row,c});style.bold=true;style.background=0x1F7A5CFF;style.foreground=0xFFFFFFFF;b.formats.push_back({{t.first.row,c},style});}
    table_totals(sheet,t,b);return b;
}
Batch append_table_row(const Sheet& sheet,std::size_t index){
    if(index>=sheet.tables().size())throw std::runtime_error(tr("Select a table first."));
    auto t=sheet.tables()[index];if(t.last.row+1+unsigned(t.totals)>=max_rows)throw std::runtime_error(tr("Invalid table range or name."));
    for(unsigned c=t.first.column;c<=t.last.column;++c)if(sheet.cell({t.last.row+1+unsigned(t.totals),c}))throw std::runtime_error(tr("The space below this table must be empty."));
    if(t.totals)for(const auto& [r,row]:sheet.populated_rows())for(const auto& [c,cell]:row)if(cell.formula&&r!=t.last.row+1)for(auto dep:cell.formula->precedents)if(dep.row==t.last.row+1&&dep.column>=t.first.column&&dep.column<=t.last.column)throw std::runtime_error(tr("A formula references the total row. Remove that reference before extending the table."));
    Batch b;b.table_scaffold=true;b.tables=sheet.tables();++(*b.tables)[index].last.row;b.rules=sheet.validation_rules();
    for(auto& rule:*b.rules)if(rule.first.row>=t.first.row+1&&rule.first.row<=t.last.row&&rule.last.row==t.last.row&&rule.first.column>=t.first.column&&rule.last.column<=t.last.column)++rule.last.row;
    for(unsigned c=t.first.column;c<=t.last.column;++c){
        auto old=sheet.cell({t.last.row,c});Input input{};if(old)if(auto f=std::get_if<FormulaInput>(&old->input))input=FormulaInput{adjust_references(f->source,1,0)};
        if(t.totals||!std::holds_alternative<std::monostate>(input))b.cells.push_back({{t.last.row+1,c},input});
        b.formats.push_back({{t.last.row+1,c},sheet.cell_style({t.last.row,c})});
    }
    table_totals(sheet,(*b.tables)[index],b);return b;
}
Batch expand_tables(const Sheet& sheet,const Batch& requested){
    if(requested.tables||sheet.tables().empty()||requested.cells.empty())return requested;
    Batch result=requested;std::optional<std::size_t> grow;
    for(std::size_t i=0;i<sheet.tables().size();++i){const auto& t=sheet.tables()[i];
        for(const auto& e:requested.cells)if(e.coord.column>=t.first.column&&e.coord.column<=t.last.column&&e.coord.row==t.last.row+1){
            if(t.totals)throw std::runtime_error(tr("Use Add table row instead of editing the managed total row."));
            if(!std::holds_alternative<std::monostate>(e.input)){if(grow&&*grow!=i)throw std::runtime_error(tr("Extend one table at a time."));grow=i;}
        }
    }
    if(!grow)return result;
    const auto& growing=sheet.tables()[*grow];for(const auto& e:requested.cells)if(e.coord.column>=growing.first.column&&e.coord.column<=growing.last.column&&e.coord.row>growing.last.row+1)throw std::runtime_error(tr("Extend one table row at a time."));
    auto addition=append_table_row(sheet,*grow);auto t=sheet.tables()[*grow];
    for(const auto& e:requested.cells)if(e.coord.row==t.last.row+1&&e.coord.column>=t.first.column&&e.coord.column<=t.last.column)for(const auto& rule:sheet.validation_rules())if(rule.locked&&rule.contains({t.last.row,e.coord.column}))throw std::runtime_error(tr("This table column is protected."));
    // User input wins over copied formulas; normal validation still checks the resulting values.
    addition.cells.insert(addition.cells.end(),result.cells.begin(),result.cells.end());addition.formats.insert(addition.formats.end(),result.formats.begin(),result.formats.end());addition.rows=result.rows;return addition;
}
}
