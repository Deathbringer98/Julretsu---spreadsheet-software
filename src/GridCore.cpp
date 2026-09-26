#include "julretsu/GridCore.hpp"
#include "julretsu/I18n.hpp"
#include "julretsu/WorksheetOps.hpp"
#include <algorithm>
#include <atomic>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace julretsu {
RowSelection::RowSelection(std::vector<RowInterval> intervals) {
    for(const auto& i:intervals) if(i.first>i.last||i.last>=max_rows) throw std::invalid_argument("Invalid row selection");
    std::sort(intervals.begin(),intervals.end(),[](auto a,auto b){return a.first<b.first;});
    for(auto i:intervals) {
        if(!intervals_.empty()&&i.first<=intervals_.back().last+1) intervals_.back().last=std::max(intervals_.back().last,i.last);
        else intervals_.push_back(i);
    }
}
bool RowSelection::contains(std::uint32_t row) const noexcept {
    auto it=std::upper_bound(intervals_.begin(),intervals_.end(),row,[](auto r,const auto& i){return r<i.first;});
    return it!=intervals_.begin()&&row<=std::prev(it)->last;
}
Sheet::Sheet(Limits limits,const FormulaExtension* extension):limits_(limits),extension_(extension) {
    // Hard caps also bound native stack use and history amplification.
    if(limits.parse_depth==0||limits.parse_depth>128||limits.ast_nodes==0||limits.ast_nodes>8192||
       limits.formula_bytes==0||limits.formula_bytes>65536||limits.range_cells>100'000||
       limits.dependencies>100'000||limits.evaluation_work>1'000'000||
       limits.batch_edits>1'000'000||limits.populated_cells>1'000'000||
       limits.graph_edges>2'000'000||limits.undo_depth>32||
       limits.text_bytes>16*1024*1024||limits.sheet_input_bytes>256*1024*1024)
        throw std::invalid_argument("Limits exceed supported hard caps");
}
const Cell* Sheet::find_cell(const State& s,CellCoord c) {
    auto row=s.rows.find(c.row); if(row==s.rows.end()) return nullptr;
    auto cell=row->second.find(c.column); return cell==row->second.end()?nullptr:&cell->second;
}
Cell* Sheet::mutable_cell(State& s,CellCoord c) {
    auto row=s.rows.find(c.row); if(row==s.rows.end()) return nullptr;
    auto cell=row->second.find(c.column); return cell==row->second.end()?nullptr:&cell->second;
}
Value Sheet::read_state(const State& s,CellCoord c) {
    if(!c.valid()) return CellError{ErrorCode::Ref,"Coordinate out of bounds",c};
    const auto* cell=find_cell(s,c); return cell?cell->cached:Value{};
}
Value Sheet::read(CellCoord c) const { return read_state(state_,c); }
const Cell* Sheet::cell(CellCoord c) const { return c.valid()?find_cell(state_,c):nullptr; }
const Row& Sheet::row_view(std::uint32_t row) const {
    if(row>=max_rows) throw std::out_of_range("Row out of bounds");
    static const Row empty;
    auto it=state_.rows.find(row); return it==state_.rows.end()?empty:it->second;
}
Style Sheet::row_style(std::uint32_t row) const {
    if(row>=max_rows) throw std::out_of_range("Row out of bounds");
    auto it=state_.styles.find(row); return it==state_.styles.end()?Style{}:it->second;
}
std::size_t Sheet::populated_cells() const noexcept {
    std::size_t count=0; for(const auto& [row,cells]:state_.rows) { (void)row; count+=cells.size(); } return count;
}
std::size_t Sheet::edge_count() const noexcept {
    std::size_t count=0; for(const auto& [coord,edges]:state_.precedents) { (void)coord; count+=edges.size(); } return count;
}
std::size_t Sheet::recalculate(State& s,const std::set<CellCoord>& dirty,std::uint64_t revision,bool& topology_changed) {
    std::set<CellCoord> formulas;
    for(auto c:dirty) if(auto* cell=mutable_cell(s,c)) {
        cell->dirty=true; cell->revision=revision;
        if(cell->formula) formulas.insert(c); else cell->dirty=false;
    }
    // Iterative Kosaraju on the affected formula subgraph.
    // Outgoing reference edges: formula -> precedent.
    std::set<CellCoord> visited;
    std::vector<CellCoord> finish;
    for(auto start:formulas) {
        if(visited.contains(start)) continue;
        std::vector<std::pair<CellCoord,bool>> stack{{start,false}};
        while(!stack.empty()) {
            auto [c,exit]=stack.back(); stack.pop_back();
            if(exit) { finish.push_back(c); continue; }
            if(!visited.insert(c).second) continue;
            stack.emplace_back(c,true);
            auto edges=s.precedents.find(c);
            if(edges!=s.precedents.end()) for(auto it=edges->second.rbegin();it!=edges->second.rend();++it)
                if(formulas.contains(*it)&&!visited.contains(*it)) stack.emplace_back(*it,false);
        }
    }
    visited.clear(); std::set<CellCoord> cycles;
    for(auto it=finish.rbegin();it!=finish.rend();++it) {
        if(visited.contains(*it)) continue;
        std::vector<CellCoord> component, stack{*it}; visited.insert(*it);
        while(!stack.empty()) {
            auto c=stack.back(); stack.pop_back(); component.push_back(c);
            auto reverse=s.dependents.find(c);
            if(reverse!=s.dependents.end()) for(auto n:reverse->second)
                if(formulas.contains(n)&&visited.insert(n).second) stack.push_back(n);
        }
        const auto edges=s.precedents.find(component.front());
        const bool self=edges!=s.precedents.end()&&edges->second.contains(component.front());
        if(component.size()>1||self) cycles.insert(component.begin(),component.end());
    }
    for(auto c:cycles) {
        auto* cell=mutable_cell(s,c); cell->cached=CellError{ErrorCode::Cycle,"Cell belongs to a circular reference component",c}; cell->dirty=false;
    }
    // Kahn order excluding diagnosed cycle members. Their typed cached errors
    // are now available to every downstream formula.
    std::map<CellCoord,std::size_t> pending;
    std::set<CellCoord> ready;
    for(auto c:formulas) if(!cycles.contains(c)) {
        std::size_t count=0;
        auto edges=s.precedents.find(c);
        if(edges!=s.precedents.end()) for(auto p:edges->second) if(formulas.contains(p)&&!cycles.contains(p)) ++count;
        pending[c]=count; if(!count) ready.insert(c);
    }
    std::size_t evaluated=0;
    Edges discovered;
    while(!ready.empty()) {
        auto c=*ready.begin(); ready.erase(ready.begin());
        auto* cell=mutable_cell(s,c);
        ExtensionContext context{[&](CellCoord p)->Value {
            if(!p.valid()) return CellError{ErrorCode::Ref,"Script coordinate out of bounds",p};
            discovered[c].insert(p);
            const auto edges=s.precedents.find(c);
            if(edges==s.precedents.end()||!edges->second.contains(p))
                return CellError{ErrorCode::Limit,"Dependency discovery: evaluation suspended",p};
            auto value=read_state(s,p);
            if(auto e=std::get_if<CellError>(&value); e&&e->code==ErrorCode::Cycle) {
                e->code=ErrorCode::CycleDependency; e->context="Script depends on a cycle";
            }
            return value;
        }};
        cell->cached=evaluate_formula(*cell->formula,[&s](CellCoord p){return read_state(s,p);},limits_,extension_,&context);
        if(auto e=std::get_if<CellError>(&cell->cached);e&&!e->origin) e->origin=c;
        cell->dirty=false; ++evaluated;
        auto reverse=s.dependents.find(c);
        if(reverse!=s.dependents.end()) for(auto d:reverse->second) {
            auto p=pending.find(d);
            if(p!=pending.end()&&p->second && --p->second==0) ready.insert(d);
        }
    }
    if(evaluated+cycles.size()!=formulas.size()) throw std::logic_error("Incomplete calculation order");
    for(const auto& [c,reads]:discovered) {
        auto* cell=mutable_cell(s,c);
        for(auto p:reads) {
            cell->runtime_precedents.insert(p);
            if(s.precedents[c].insert(p).second) { s.dependents[p].insert(c); topology_changed=true; }
        }
        if(s.precedents[c].size()>limits_.dependencies)
            throw CellError{ErrorCode::Limit,"Runtime dependency budget exceeded",c};
    }
    return evaluated;
}
CommitResult Sheet::apply(const Batch& requested) {
    Batch expanded;
    try {expanded=expand_tables(*this,requested);} catch(const std::exception& e){CommitResult failed;failed.revision=revision_;failed.error=CellError{ErrorCode::Value,e.what(),{}};return failed;}
    const Batch& batch=expanded;
    CommitResult result; result.revision=revision_;
    auto reject=[&](ErrorCode code,std::string context) {
        result.error=CellError{code,std::move(context),{}}; return result;
    };
    if(batch.cells.size()>limits_.batch_edits||batch.rows.size()>limits_.batch_edits||batch.formats.size()>limits_.batch_edits)
        return reject(ErrorCode::Limit,"Batch edit limit");
    if(batch.cells.empty()&&batch.rows.empty()&&batch.formats.empty()&&!batch.rules&&!batch.tables) { result.accepted=true; return result; }
    if(revision_==std::numeric_limits<std::uint64_t>::max()) return reject(ErrorCode::Limit,"Revision exhausted");
    try {
        std::size_t batch_bytes=0;
        for(const auto& edit:batch.cells) {
            if(auto p=std::get_if<std::string>(&edit.input)) batch_bytes+=p->size();
            if(auto p=std::get_if<FormulaInput>(&edit.input)) batch_bytes+=p->source.size();
            if(batch_bytes>limits_.sheet_input_bytes) return reject(ErrorCode::Limit,"Batch input byte limit");
        }
        // Validate ALL operations before duplicate resolution: last write wins.
        std::map<CellCoord,Cell> candidates;
        for(const auto& edit:batch.cells) {
            if(!edit.coord.valid()) return reject(ErrorCode::Ref,"Batch contains an invalid coordinate");
            if(auto p=std::get_if<double>(&edit.input);p&&!std::isfinite(*p))
                return reject(ErrorCode::Num,"Literal number must be finite");
            if(auto p=std::get_if<std::string>(&edit.input);p&&p->size()>limits_.text_bytes)
                return reject(ErrorCode::Limit,"Text byte limit");
            if(auto p=std::get_if<FormulaInput>(&edit.input);p&&p->source.size()>limits_.text_bytes)
                return reject(ErrorCode::Limit,"Stored input byte limit");
            Cell cell; cell.input=edit.input;
            if(auto p=std::get_if<FormulaInput>(&cell.input)) cell.formula=parse_formula(p->source,limits_);
            else std::visit([&](const auto& v){
                using T=std::decay_t<decltype(v)>;
                if constexpr(!std::is_same_v<T,FormulaInput>) cell.cached=v;
            },cell.input);
            candidates.insert_or_assign(edit.coord,std::move(cell));
        }
        for(const auto& edit:batch.rows)
            if(edit.row>=max_rows || (edit.style&&edit.style->decimals>15))
                return reject(ErrorCode::Value,"Invalid row style");
        for(const auto& edit:batch.formats)
            if(!edit.coord.valid()||(edit.style&&(edit.style->decimals>12||edit.style->alignment>3||edit.style->number_format>5||edit.style->font_size<8||edit.style->font_size>36||edit.style->font_family>2)))
                return reject(ErrorCode::Value,"Invalid cell formatting");
        // Reject the whole edit before changing any protected input or formatting.
        for(const auto& rule:state_.rules) if(rule.locked) {
            for(const auto& e:batch.cells) if(rule.contains(e.coord)) {
                const auto* old=cell(e.coord);
                if((old?old->input:Input{})!=e.input) return reject(ErrorCode::Value,trf("%s: This cell is protected.",std::get<std::string>(to_a1(e.coord)).c_str()));
            }
            for(const auto& e:batch.formats) if(rule.contains(e.coord)) return reject(ErrorCode::Value,tr("Protected cell formatting cannot be changed."));
            for(const auto& e:batch.rows) if(e.row>=rule.first.row&&e.row<=rule.last.row) return reject(ErrorCode::Value,tr("Protected cell formatting cannot be changed."));
        }
        if(batch.rules) {
            std::size_t area=0,rule_bytes=0;
            if(batch.rules->size()>1000) return reject(ErrorCode::Limit,tr("Use at most 1,000 rules and 100,000 rule cells."));
            for(const auto& rule:*batch.rules) {
                if(!valid_rule(rule)) return reject(ErrorCode::Value,tr("Invalid validation rule. Check the range, limits and choices."));
                rule_bytes+=rule.date_min.size()+rule.date_max.size();for(const auto& choice:rule.choices)rule_bytes+=choice.size();
                if(rule_bytes>4*1024*1024) return reject(ErrorCode::Limit,tr("Invalid validation rule. Check the range, limits and choices."));
                area+=std::size_t(rule.last.row-rule.first.row+1)*(rule.last.column-rule.first.column+1);
                if(area>100000) return reject(ErrorCode::Limit,tr("Use at most 1,000 rules and 100,000 rule cells."));
            }
        }
        if(batch.tables){
            if(batch.tables->size()>64)return reject(ErrorCode::Limit,tr("Invalid table range or name."));
            std::set<std::string> names;
            for(const auto& t:*batch.tables){
                if(t.name.empty()||t.name.size()>64||!names.insert(t.name).second||!t.first.valid()||!t.last.valid()||t.first.row>=t.last.row||t.first.column>t.last.column||t.last.row+unsigned(t.totals)>=max_rows||std::uint64_t(t.last.row-t.first.row+1)*(t.last.column-t.first.column+1)>100000)return reject(ErrorCode::Value,tr("Invalid table range or name."));
                for(const auto& other:*batch.tables)if(&t!=&other&&t.first.row<=other.last.row+unsigned(other.totals)&&t.last.row+unsigned(t.totals)>=other.first.row&&t.first.column<=other.last.column&&t.last.column>=other.first.column)return reject(ErrorCode::Value,tr("Tables cannot overlap."));
            }
        }
        State staged=state_; // Strong transaction boundary; see complexity contract.
        std::set<CellCoord> dirty;
        if(batch.rules) staged.rules=*batch.rules;
        if(batch.tables) staged.tables=*batch.tables;
        for(auto& [coord,cell]:candidates) {
            auto old=staged.precedents.find(coord);
            if(old!=staged.precedents.end()) {
                for(auto p:old->second) {
                    auto reverse=staged.dependents.find(p);
                    if(reverse!=staged.dependents.end()) {
                        reverse->second.erase(coord);
                        if(reverse->second.empty()) staged.dependents.erase(reverse);
                    }
                }
                staged.precedents.erase(old);
            }
            if(cell.formula && !cell.formula->precedents.empty()) {
                auto& forward=staged.precedents[coord];
                for(auto p:cell.formula->precedents) { forward.insert(p); staged.dependents[p].insert(coord); }
            }
            if(std::holds_alternative<std::monostate>(cell.input)) {
                auto row=staged.rows.find(coord.row);
                if(row!=staged.rows.end()) { row->second.erase(coord.column); if(row->second.empty()) staged.rows.erase(row); }
            } else staged.rows[coord.row].insert_or_assign(coord.column,std::move(cell));
            dirty.insert(coord);
        }
        for(const auto& edit:batch.rows) {
            if(edit.style&&*edit.style!=Style{}) staged.styles.insert_or_assign(edit.row,*edit.style);
            else staged.styles.erase(edit.row);
        }
        for(const auto& edit:batch.formats) {
            if(edit.style) staged.formats.insert_or_assign(edit.coord,*edit.style);
            else staged.formats.erase(edit.coord);
        }
        if(staged.formats.size()>limits_.populated_cells) return reject(ErrorCode::Limit,"Cell format limit");
        std::size_t cells=0, bytes=0, edges=0;
        for(const auto& [r,row]:staged.rows) {
            (void)r; cells+=row.size();
            for(const auto& [c,cell]:row) {
                (void)c;
                if(auto p=std::get_if<std::string>(&cell.input)) bytes+=p->size();
                if(auto p=std::get_if<FormulaInput>(&cell.input)) bytes+=p->source.size();
            }
        }
        for(const auto& [c,e]:staged.precedents) { (void)c; edges+=e.size(); }
        if(cells>limits_.populated_cells||bytes>limits_.sheet_input_bytes||edges>limits_.graph_edges||staged.styles.size()>limits_.populated_cells)
            return reject(ErrorCode::Limit,"Sheet storage or graph budget exceeded");
        std::vector<CellCoord> queue(dirty.begin(),dirty.end());
        for(std::size_t i=0;i<queue.size();++i) {
            auto reverse=staged.dependents.find(queue[i]);
            if(reverse!=staged.dependents.end()) for(auto d:reverse->second) if(dirty.insert(d).second) queue.push_back(d);
        }
        // Determine dirty closure using old runtime subscriptions first; then
        // discard them so changed branches can recover from former cycles.
        for(auto c:dirty) if(auto* cell=mutable_cell(staged,c);cell&&cell->formula) {
            std::set<CellCoord> native(cell->formula->precedents.begin(),cell->formula->precedents.end());
            for(auto p:cell->runtime_precedents) if(!native.contains(p)) {
                staged.precedents[c].erase(p);
                auto reverse=staged.dependents.find(p);
                if(reverse!=staged.dependents.end()) {
                    reverse->second.erase(c);
                    if(reverse->second.empty()) staged.dependents.erase(reverse);
                }
            }
            cell->runtime_precedents.clear();
        }
        bool settled=false;
        for(unsigned retry=0;retry<64;++retry) {
            bool topology_changed=false;
            result.formulas_evaluated+=recalculate(staged,dirty,revision_+1,topology_changed);
            std::size_t total_edges=0;
            for(const auto& [c,e]:staged.precedents) { (void)c; total_edges+=e.size(); }
            if(total_edges>limits_.graph_edges) return reject(ErrorCode::Limit,"Runtime graph budget exceeded");
            if(!topology_changed) { settled=true; break; }
        }
        if(!settled) return reject(ErrorCode::Limit,"Dynamic dependency discovery exceeded 64 passes; batch rolled back");
        // Check final recalculated values, including duplicates introduced in one paste.
        // Existing invalid cells are reported by the rule panel; unrelated edits remain possible.
        if(!batch.cells.empty()&&!state_.rules.empty()) {
            auto issues=check_rules(staged.rules,[&](CellCoord c){return read_state(staged,c);});
            for(const auto& issue:issues) if(!issue.warning&&dirty.contains(issue.cell)) {
                bool scaffold=false;if(batch.table_scaffold&&issue.reason=="A value is required."&&std::holds_alternative<std::monostate>(read_state(staged,issue.cell)))for(const auto& t:staged.tables)for(const auto& old:state_.tables)if(t.name==old.name&&t.last.row==old.last.row+1&&issue.cell.row==t.last.row)scaffold=true;
                if(scaffold)continue;
                return reject(ErrorCode::Value,trf("%s: %s",std::get<std::string>(to_a1(issue.cell)).c_str(),tr(issue.reason.c_str())));
            }
        }
        result.changed.assign(dirty.begin(),dirty.end());
        std::set<std::uint32_t> styled;
        for(const auto& edit:batch.rows) styled.insert(edit.row);
        result.styled_rows.assign(styled.begin(),styled.end());
        if(limits_.undo_depth) {
            undo_.push_back(state_);
            if(undo_.size()>limits_.undo_depth) undo_.erase(undo_.begin());
        }
        // No throwing work beyond this point.
        state_=std::move(staged); redo_.clear(); ++revision_;
        if(!undo_.empty()) record_change(undo_.back(),{});
        result.accepted=true; result.revision=revision_; return result;
    } catch(const CellError& error) {
        return reject(error.code,error.context);
    } catch(const std::bad_alloc&) {
        result.changed.clear(); result.styled_rows.clear(); result.formulas_evaluated=0;
        return reject(ErrorCode::Limit,"Allocation failed; transaction was not committed");
    }
}
CommitResult Sheet::set(CellCoord c,Input input) { return apply(Batch{{Edit{c,std::move(input)}},{}}); }
CommitResult Sheet::clear(CellCoord c) { return set(c,std::monostate{}); }
CommitResult Sheet::clear_rows(const RowSelection& selection) {
    Batch batch;
    for(auto interval:selection.intervals())
        for(auto row=state_.rows.lower_bound(interval.first);row!=state_.rows.end()&&row->first<=interval.last;++row)
            for(const auto& [col,cell]:row->second) { (void)cell; batch.cells.push_back({{row->first,col},std::monostate{}}); }
    return apply(batch);
}
bool Sheet::undo() {
    if(undo_.empty()||revision_==std::numeric_limits<std::uint64_t>::max()) return false;
    redo_.push_back(state_); state_=std::move(undo_.back()); undo_.pop_back(); ++revision_;
    for(auto& [r,row]:state_.rows) { (void)r; for(auto& [c,cell]:row) { (void)c; cell.revision=revision_; } }
    record_change(redo_.back(),"Undo");
    return true;
}
bool Sheet::redo() {
    if(redo_.empty()||revision_==std::numeric_limits<std::uint64_t>::max()) return false;
    undo_.push_back(state_); state_=std::move(redo_.back()); redo_.pop_back(); ++revision_;
    for(auto& [r,row]:state_.rows) { (void)r; for(auto& [c,cell]:row) { (void)c; cell.revision=revision_; } }
    record_change(undo_.back(),"Redo");
    return true;
}
namespace {
std::string history_text(const Input& input) {
    std::string text;
    if(auto f=std::get_if<FormulaInput>(&input)) text=f->source;
    else if(auto t=std::get_if<std::string>(&input)) text=*t;
    else if(auto b=std::get_if<bool>(&input)) text=*b?"TRUE":"FALSE";
    else if(auto n=std::get_if<double>(&input)) { std::array<char,64> buffer{}; auto [end,e]=std::to_chars(buffer.data(),buffer.data()+buffer.size(),*n); if(e==std::errc{}) text.assign(buffer.data(),end); }
    if(text.size()>journal_text) {
        std::size_t cut=journal_text; while(cut>0&&(static_cast<unsigned char>(text[cut])&0xc0)==0x80) --cut;
        text.resize(cut); text+="...";
    }
    return text;
}
// Visits every key present in either ordered map, pairing the values (nullptr when absent).
template<class Map,class F> void merge_maps(const Map& a,const Map& b,F&& visit) {
    auto i=a.begin(); auto j=b.begin();
    while(i!=a.end()||j!=b.end()) {
        if(j==b.end()||(i!=a.end()&&i->first<j->first)) { visit(i->first,&i->second,nullptr); ++i; }
        else if(i==a.end()||j->first<i->first) { visit(j->first,nullptr,&j->second); ++j; }
        else { visit(i->first,&i->second,&j->second); ++i; ++j; }
    }
}
}
void Sheet::record_change(const State& before,std::string label) {
    if(suppress_journal_) return;
    try {
        ChangeRecord record;
        const Input empty;
        merge_maps(before.rows,state_.rows,[&](std::uint32_t r,const Row* a,const Row* b) {
            const Row none;
            merge_maps(a?*a:none,b?*b:none,[&](std::uint32_t c,const Cell* x,const Cell* y) {
                const Input& old_input=x?x->input:empty; const Input& new_input=y?y->input:empty;
                if(old_input==new_input) return;
                ++record.total_cells;
                if(record.cells.size()<journal_cells_per_record)
                    record.cells.push_back({{r,c},history_text(old_input),history_text(new_input),std::uint8_t(old_input.index()),std::uint8_t(new_input.index())});
            });
        });
        merge_maps(before.formats,state_.formats,[&](const CellCoord&,const Style* x,const Style* y) { if(!x||!y||!(*x==*y)) ++record.formats; });
        merge_maps(before.styles,state_.styles,[&](std::uint32_t,const Style* x,const Style* y) { if(!x||!y||!(*x==*y)) ++record.formats; });
        if(before.rules!=state_.rules||before.tables!=state_.tables) ++record.formats;
        if(!record.total_cells&&!record.formats) return; // recalculation only; nothing a person changed
        const bool explicit_label=!label.empty();
        record.label=explicit_label?std::move(label):next_label_.empty()?std::string("Edit"):next_label_;
        if(!explicit_label) next_label_.clear();
        record.time=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        journal_.push_back(std::move(record)); ++journal_serial_;
        if(journal_.size()>journal_records) journal_.erase(journal_.begin(),journal_.begin()+std::ptrdiff_t(journal_.size()-journal_records));
    } catch(...) {} // history is best effort; the edit itself is already committed
}
std::uint64_t Sheet::next_instance() { static std::atomic<std::uint64_t> next{1}; return next++; }
void Sheet::set_journal(std::vector<ChangeRecord> journal) {
    if(journal.size()>journal_records) journal.erase(journal.begin(),journal.begin()+std::ptrdiff_t(journal.size()-journal_records));
    for(auto& record:journal) if(record.cells.size()>journal_cells_per_record) record.cells.resize(journal_cells_per_record);
    journal_=std::move(journal);
}
bool Sheet::revert_last_change() {
    const auto serial=journal_serial_;
    suppress_journal_=true; const bool undone=undo(); suppress_journal_=false;
    if(undone&&!journal_.empty()&&serial==journal_serial_) journal_.pop_back();
    return undone;
}
}
