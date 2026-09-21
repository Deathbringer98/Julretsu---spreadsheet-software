#include "julretsu/WorksheetOps.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <numeric>
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
