#include "julretsu/GridUI.hpp"
#include "julretsu/WorkbookFile.hpp"
#include "julretsu/Csv.hpp"
#include "julretsu/Xlsx.hpp"
#include "julretsu/SheetHealth.hpp"
#include <ctime>
#include "julretsu/WorksheetOps.hpp"
#include <chrono>
#include <regex>
#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#else
#include <sys/wait.h>
#endif
#include <imgui.h>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <filesystem>
#include <fstream>
#include <cstdlib>

namespace julretsu {
static std::string to_address(CellCoord c) { return std::get<std::string>(to_a1(c)); }
namespace {
std::filesystem::path appearance_path() {
    if(const char* override_path=std::getenv("JULRETSU_SETTINGS_PATH")) return std::filesystem::path(override_path);
#ifdef _WIN32
    if(const char* base=std::getenv("LOCALAPPDATA")) return std::filesystem::path(base)/"Julretsu"/"appearance.txt";
#else
    if(const char* base=std::getenv("XDG_CONFIG_HOME")) return std::filesystem::path(base)/"julretsu"/"appearance.txt";
    if(const char* base=std::getenv("HOME")) return std::filesystem::path(base)/".config"/"julretsu"/"appearance.txt";
#endif
    return {};
}
enum class Glyph { Sheet, Undo, Redo, Bold, Fill, Reset, Clear, Function, Code, Settings, Book, Sun, Moon, Plus, Ai,
    Paste, Cut, Copy, AlignLeft, AlignCenter, AlignRight, Border, Sigma, Filter, Find, Down, ChevronUp, ChevronDown, Help,
    WinMin, WinMax, WinRestore, WinClose, Save, Styles, Table, Insert, Delete, TextColor, Currency, Percent, Comma, DecLess, DecMore, FontUp, FontDown };
void glyph(ImDrawList* d,ImVec2 p,float size,ImU32 color,Glyph kind) {
    auto point=[&](float x,float y){return ImVec2{p.x+x*size,p.y+y*size};};
    auto line=[&](float x,float y,float xx,float yy){ d->AddLine(point(x,y),point(xx,yy),color,1.6f); };
    if(kind==Glyph::Undo||kind==Glyph::Redo) {
        const bool redo=kind==Glyph::Redo;
        auto pt=[&](float x,float y){return point(redo?1-x:x,y);};
        d->AddBezierCubic(pt(.2f,.3f),pt(.9f,.04f),pt(1,.7f),pt(.4f,.8f),color,1.7f);
        d->AddLine(pt(.2f,.3f),pt(.2f,.03f),color,1.7f);d->AddLine(pt(.2f,.3f),pt(.47f,.34f),color,1.7f);
    } else if(kind==Glyph::Sheet||kind==Glyph::Book) {
        d->AddRect(point(.1f,.1f),point(.9f,.9f),color,1,0,1.5f); line(.1f,.35f,.9f,.35f);
        line(.35f,.35f,.35f,.9f); line(.1f,.62f,.9f,.62f);
    } else if(kind==Glyph::Fill) {
        d->AddQuad(point(.5f,.08f),point(.85f,.43f),point(.45f,.83f),point(.1f,.48f),color,1.6f);
        line(.45f,.03f,.65f,.4f); d->AddRectFilled(point(.08f,.92f),point(.92f,1),IM_COL32(232,189,42,255));
    } else if(kind==Glyph::Clear||kind==Glyph::Reset) {
        d->AddQuad(point(.55f,.1f),point(.9f,.4f),point(.45f,.88f),point(.1f,.55f),color,1.6f);
        line(.32f,.33f,.67f,.65f); line(.4f,.9f,.97f,.9f);
    } else if(kind==Glyph::Code) {
        line(.3f,.2f,.05f,.5f);line(.05f,.5f,.3f,.8f);line(.7f,.2f,.95f,.5f);line(.95f,.5f,.7f,.8f);line(.6f,.08f,.4f,.92f);
    } else if(kind==Glyph::Sun||kind==Glyph::Settings) {
        d->AddCircle(point(.5f,.5f),size*.23f,color,16,1.5f);
        for(int i=0;i<8;++i) { const float a=float(i)*.785398f;line(.5f+.34f*std::cos(a),.5f+.34f*std::sin(a),.5f+.47f*std::cos(a),.5f+.47f*std::sin(a)); }
    } else if(kind==Glyph::Plus) {line(.5f,.15f,.5f,.85f);line(.15f,.5f,.85f,.5f);}
    else if(kind==Glyph::Paste) {
        d->AddRect(point(.15f,.15f),point(.85f,.95f),color,2,0,1.5f); d->AddRectFilled(point(.33f,.04f),point(.67f,.24f),color,2);
        line(.32f,.5f,.68f,.5f); line(.32f,.7f,.6f,.7f);
    } else if(kind==Glyph::Cut) {
        d->AddCircle(point(.28f,.8f),size*.14f,color,12,1.5f); d->AddCircle(point(.72f,.8f),size*.14f,color,12,1.5f);
        line(.35f,.68f,.78f,.05f); line(.65f,.68f,.22f,.05f);
    } else if(kind==Glyph::Copy) {
        d->AddRect(point(.08f,.3f),point(.62f,.95f),color,1,0,1.5f); d->AddRect(point(.38f,.05f),point(.92f,.7f),color,1,0,1.5f);
    } else if(kind==Glyph::AlignLeft||kind==Glyph::AlignCenter||kind==Glyph::AlignRight) {
        const float shift=kind==Glyph::AlignLeft?0:kind==Glyph::AlignCenter?.15f:.3f;
        for(int i=0;i<4;++i) { const float y=.18f+.22f*float(i); if(i%2==0) line(.08f,y,.92f,y); else line(.08f+shift,y,.62f+shift,y); }
    } else if(kind==Glyph::Border) {
        d->AddRect(point(.1f,.1f),point(.9f,.9f),color,0,0,1.8f);
        d->AddLine(point(.5f,.1f),point(.5f,.9f),color,.8f); d->AddLine(point(.1f,.5f),point(.9f,.5f),color,.8f);
    } else if(kind==Glyph::Sigma) {
        line(.82f,.12f,.2f,.12f); line(.2f,.12f,.55f,.5f); line(.55f,.5f,.2f,.88f); line(.2f,.88f,.82f,.88f);
    } else if(kind==Glyph::Filter) {
        line(.08f,.12f,.92f,.12f); line(.08f,.12f,.42f,.52f); line(.92f,.12f,.58f,.52f);
        line(.42f,.52f,.42f,.92f); line(.58f,.52f,.58f,.8f); line(.42f,.92f,.58f,.8f);
    } else if(kind==Glyph::Find) {
        d->AddCircle(point(.42f,.42f),size*.28f,color,16,1.6f); d->AddLine(point(.62f,.62f),point(.93f,.93f),color,2.2f);
    } else if(kind==Glyph::Down) { line(.5f,.08f,.5f,.88f); line(.22f,.6f,.5f,.9f); line(.78f,.6f,.5f,.9f); }
    else if(kind==Glyph::ChevronUp) { line(.18f,.66f,.5f,.34f); line(.5f,.34f,.82f,.66f); }
    else if(kind==Glyph::ChevronDown) { line(.18f,.34f,.5f,.66f); line(.5f,.66f,.82f,.34f); }
    else if(kind==Glyph::WinMin) d->AddLine(point(0,.5f),point(1,.5f),color,1);
    else if(kind==Glyph::WinMax) d->AddRect(point(0,0),point(1,1),color,0,0,1);
    else if(kind==Glyph::WinRestore) {
        d->AddRect(point(0,.25f),point(.75f,1),color,0,0,1);
        d->AddLine(point(.25f,.25f),point(.25f,0),color,1); d->AddLine(point(.25f,0),point(1,0),color,1);
        d->AddLine(point(1,0),point(1,.75f),color,1); d->AddLine(point(1,.75f),point(.75f,.75f),color,1);
    } else if(kind==Glyph::WinClose) { d->AddLine(point(0,0),point(1,1),color,1.2f); d->AddLine(point(1,0),point(0,1),color,1.2f); }
    else if(kind==Glyph::Save) {
        d->AddRect(point(.1f,.1f),point(.9f,.9f),color,1.5f,0,1.5f);
        d->AddRect(point(.28f,.1f),point(.7f,.38f),color,0,0,1.3f); d->AddRect(point(.25f,.58f),point(.75f,.9f),color,0,0,1.3f);
    } else if(kind==Glyph::Styles) {
        d->AddRectFilled(point(.05f,.05f),point(.47f,.47f),IM_COL32(198,239,206,255),2); d->AddRectFilled(point(.53f,.05f),point(.95f,.47f),IM_COL32(255,199,206,255),2);
        d->AddRectFilled(point(.05f,.53f),point(.47f,.95f),IM_COL32(255,235,156,255),2); d->AddRectFilled(point(.53f,.53f),point(.95f,.95f),IM_COL32(31,122,92,255),2);
        d->AddRect(point(.05f,.05f),point(.95f,.95f),color,2,0,1);
    } else if(kind==Glyph::Table) {
        d->AddRectFilled(point(.08f,.1f),point(.92f,.34f),IM_COL32(31,122,92,255));
        d->AddRect(point(.08f,.1f),point(.92f,.9f),color,0,0,1.4f); line(.08f,.62f,.92f,.62f); line(.5f,.1f,.5f,.9f);
    } else if(kind==Glyph::Insert) {
        d->AddRect(point(.08f,.4f),point(.92f,.8f),color,0,0,1.4f); line(.5f,.02f,.5f,.3f); line(.36f,.16f,.64f,.16f);
    } else if(kind==Glyph::Delete) {
        d->AddRect(point(.08f,.3f),point(.92f,.7f),color,0,0,1.4f);
        d->AddLine(point(.3f,.05f),point(.7f,.95f),IM_COL32(196,43,28,255),1.8f); d->AddLine(point(.7f,.05f),point(.3f,.95f),IM_COL32(196,43,28,255),1.8f);
    } else if(kind==Glyph::TextColor) {
        d->AddText(ImGui::GetFont(),size*.95f,point(.22f,-.12f),color,"A"); d->AddRectFilled(point(.06f,.84f),point(.94f,1),IM_COL32(196,43,28,255));
    }
    else {
        const char* text=kind==Glyph::Bold?"B":kind==Glyph::Function?"fx":kind==Glyph::Ai?"AI":kind==Glyph::Help?"?":kind==Glyph::Currency?"$":
            kind==Glyph::Percent?"%":kind==Glyph::Comma?",":kind==Glyph::DecLess?".0":kind==Glyph::DecMore?".00":kind==Glyph::FontUp?"A+":kind==Glyph::FontDown?"A-":"J";
        const float text_size=size*1.2f; const auto extent=ImGui::GetFont()->CalcTextSizeA(text_size,FLT_MAX,0,text);
        const ImVec2 at{p.x+(size-extent.x)/2,p.y+(size-extent.y)/2};
        d->AddText(ImGui::GetFont(),text_size,at,color,text);
        if(kind==Glyph::Bold) d->AddText(ImGui::GetFont(),text_size,{at.x+1,at.y},color,text); // faux bold
    }
}
bool ribbon_button(const char* text,Glyph icon,float scale,float width=76,bool arrow=false) {
    ImGui::PushID(text); const auto p=ImGui::GetCursorScreenPos();
    const bool clicked=ImGui::InvisibleButton("button",{width*scale,65*scale});
    auto* d=ImGui::GetWindowDrawList();
    if(ImGui::IsItemHovered()) d->AddRectFilled(p,{p.x+width*scale,p.y+65*scale},ImGui::GetColorU32(ImGuiCol_ButtonHovered),5*scale);
    glyph(d,{p.x+(width-25)*scale/2,p.y+5*scale},25*scale,ImGui::GetColorU32(ImGuiCol_Text),icon);
    const auto size=ImGui::CalcTextSize(text);
    d->AddText({p.x+(width*scale-size.x)/2,p.y+40*scale},ImGui::GetColorU32(ImGuiCol_Text),text);
    if(arrow) { const float ax=p.x+(width*scale+size.x)/2+7*scale, ay=p.y+40*scale+size.y/2; d->AddTriangleFilled({ax-4*scale,ay-2*scale},{ax+4*scale,ay-2*scale},{ax,ay+2*scale},ImGui::GetColorU32(ImGuiCol_Text)); }
    ImGui::PopID(); return clicked;
}
// Compact icon button used by ribbon groups, the title bar and the workbook window controls.
bool icon_button(const char* id,Glyph icon,float scale,const char* tip,bool active=false,float width=30) {
    ImGui::PushID(id); const auto p=ImGui::GetCursorScreenPos(); const ImVec2 size{width*scale,28*scale};
    const bool clicked=ImGui::InvisibleButton("icon",size);
    auto* d=ImGui::GetWindowDrawList();
    if(active||ImGui::IsItemHovered()) d->AddRectFilled(p,{p.x+size.x,p.y+size.y},ImGui::GetColorU32(active?ImGuiCol_Header:ImGuiCol_ButtonHovered),4*scale);
    const float g=16*scale; glyph(d,{p.x+(size.x-g)/2,p.y+(size.y-g)/2},g,ImGui::GetColorU32(ImGuiCol_Text),icon);
    if(tip&&ImGui::IsItemHovered()) ImGui::SetTooltip("%s",tip);
    ImGui::PopID(); return clicked;
}
// Small icon-and-label button, optionally with a drop-down arrow, like Excel's Insert / Delete / Format.
bool labeled_button(const char* label,Glyph icon,float scale,float width,bool arrow=false,float height=28) {
    ImGui::PushID(label); const auto p=ImGui::GetCursorScreenPos(); const ImVec2 size{width*scale,height*scale};
    const bool clicked=ImGui::InvisibleButton("labeled",size);
    auto* d=ImGui::GetWindowDrawList(); const auto text=ImGui::GetColorU32(ImGuiCol_Text);
    if(ImGui::IsItemHovered()) d->AddRectFilled(p,{p.x+size.x,p.y+size.y},ImGui::GetColorU32(ImGuiCol_ButtonHovered),4*scale);
    const float g=std::min(16*scale,size.y-4*scale); glyph(d,{p.x+5*scale,p.y+(size.y-g)/2},g,text,icon);
    const float fs=ImGui::GetFontSize()*.85f;
    d->AddText(ImGui::GetFont(),fs,{p.x+g+11*scale,p.y+(size.y-fs)/2},text,label);
    if(arrow) { const float ax=p.x+size.x-10*scale, ay=p.y+size.y/2; d->AddTriangleFilled({ax-4*scale,ay-2*scale},{ax+4*scale,ay-2*scale},{ax,ay+2*scale},text); }
    ImGui::PopID(); return clicked;
}
// Formats with thousands separators: 1234.5 with 2 decimals and "$" gives "$1,234.50".
void grouped(char* out,std::size_t capacity,double n,int decimals,const char* prefix) {
    if(!(std::abs(n)<1e15)) { std::snprintf(out,capacity,"%s%.6g",prefix,n); return; }
    char digits[64]; std::snprintf(digits,sizeof digits,"%.*f",decimals,std::abs(n));
    const char* dot=std::strchr(digits,'.'); const std::size_t whole=dot?std::size_t(dot-digits):std::strlen(digits);
    bool nonzero=false; for(const char* c=digits;*c;++c) if(*c>='1'&&*c<='9') nonzero=true;
    char result[96]; std::size_t k=0;
    auto put=[&](char c){ if(k+1<sizeof result) result[k++]=c; };
    if(n<0&&nonzero) put('-');
    for(const char* c=prefix;*c;++c) put(*c);
    for(std::size_t i=0;i<whole;++i) { if(i&&(whole-i)%3==0) put(','); put(digits[i]); }
    if(dot) for(const char* c=dot;*c;++c) put(*c);
    result[k]=0; std::snprintf(out,capacity,"%s",result);
}

Input interpret(std::string_view text) {
    if(text.empty()) return std::monostate{};
    if(text.front()=='\'') return std::string(text.substr(1));
    if(text.front()=='=') return FormulaInput{std::string(text)};
    if(text=="TRUE"||text=="true") return true;
    if(text=="FALSE"||text=="false") return false;
    double value{};
    auto [end,error]=parse_double(text.data(),text.data()+text.size(),value);
    if(error==std::errc{}&&end==text.data()+text.size()&&std::isfinite(value)) return value;
    return std::string(text);
}
void label(CellCoord c,char* out,std::size_t capacity,bool row=true) {
    char letters[16]{}; unsigned n=c.column+1; int count=0;
    while(n) { letters[count++]=char('A'+(n-1)%26); n=(n-1)/26; }
    char normal[16]{}; for(int i=0;i<count;++i) normal[i]=letters[count-i-1];
    if(row) std::snprintf(out,capacity,"%s%u",normal,c.row+1); else std::snprintf(out,capacity,"%s",normal);
}
ImU32 rgba(std::uint32_t c) { return IM_COL32((c>>24)&255,(c>>16)&255,(c>>8)&255,c&255); }
const char* format_value(const Value& value,char* buffer,std::size_t size,int decimals) {
    if(auto d=std::get_if<double>(&value)) {
        if(std::abs(*d)>=1e12) std::snprintf(buffer,size,"%.6g",*d);
        else if(std::floor(*d)==*d) std::snprintf(buffer,size,"%.0f",*d);
        else std::snprintf(buffer,size,"%.*f",decimals,*d);
        return buffer;
    }
    if(auto b=std::get_if<bool>(&value)) return *b?"TRUE":"FALSE";
    if(auto t=std::get_if<std::string>(&value)) {
        auto count=std::min(t->size(),size-1);
        while(count<t->size()&&count>0&&(static_cast<unsigned char>((*t)[count])&0xc0)==0x80) --count;
        std::memcpy(buffer,t->data(),count); buffer[count]=0;
        for(std::size_t i=0;i<count;++i) if(buffer[i]=='\n'||buffer[i]=='\r'||buffer[i]=='\t') buffer[i]=' ';
        return buffer;
    }
    if(auto e=std::get_if<CellError>(&value)) return error_display(e->code).data();
    return "";
}
}
GridUI::GridUI():sheet_({},&lua_) {
    std::ifstream preference(appearance_path()); std::string saved; preference >> saved; dark_=saved=="dark";
    std::snprintf(script_.data(),script_.size(),"-- Macros commit once. Undo restores every edit.\n-- cell() reads the sheet before the macro.\nfor row = 4, 9 do\n  local quantity = cell('C' .. row)\n  set('C' .. row, quantity + 1)\nend");
    load_demo();
    ai_use_settings(load_ai_settings());
    sheets_.push_back({"Sheet 1",Sheet({},&lua_),""});
    lua_.sheet_reader=[this](std::string_view name,CellCoord c)->Value {
        for(std::size_t i=0;i<sheets_.size();++i) if(sheets_[i].name==name) {std::set<std::pair<std::size_t,CellCoord>> visiting;unsigned work=0;return read_sheet_cell(i,c,visiting,work);}
        return CellError{ErrorCode::Ref,"Unknown worksheet: "+std::string(name),{}};
    };
    recovery_open_=std::filesystem::exists(appearance_path().parent_path()/"recovery.julretsu");
}
void GridUI::load_demo() {
    if(!sheets_.empty()){sheets_.clear();sheets_.push_back({"Sheet 1",Sheet({},&lua_),""});current_sheet_=0;}
    file_path_.clear(); workbook_name_="Untitled workbook"; file_label_.clear(); editor_dirty_=false;
    sheet_=Sheet({},&lua_); Batch b;
    auto put=[&](unsigned row,unsigned col,Input value){b.cells.push_back({{row,col},std::move(value)});};
    Style title_style; title_style.bold=true; b.rows.push_back({0,title_style});
    put(0,0,std::string("PROJECT BUDGET")); put(1,0,std::string("A small plan. Room to grow."));
    const char* headers[]{"Category","Item","Quantity","Unit cost","Total","Status","Notes"};
    for(unsigned c=0;c<7;++c) put(2,c,std::string(headers[c]));
    const char* category[]{"Workspace","Equipment","Software","Marketing","Operations","Contingency"};
    const char* item[]{"Studio rental","Display + peripherals","Design tools","Launch campaign","Supplies","Reserve"};
    const double quantity[]{3,2,6,1,4,1},price[]{450,279,24,1200,65,500};
    for(unsigned i=0;i<6;++i) {
        put(i+3,0,std::string(category[i])); put(i+3,1,std::string(item[i]));
        put(i+3,2,quantity[i]); put(i+3,3,price[i]);
        put(i+3,4,FormulaInput{"=C"+std::to_string(i+4)+"*D"+std::to_string(i+4)});
        put(i+3,5,std::string(i==3?"Review":"Planned"));
    }
    put(10,3,std::string("TOTAL")); put(10,4,FormulaInput{"=SUM(E4:E9)"});
    put(12,0,std::string("Try a formula")); put(12,1,std::string("=SUM(E4:E9)"));
    put(13,0,std::string("Try Lua")); put(13,1,std::string("=LUA(\"return cell('E11') * 1.05\")"));
    Style heading; heading.bold=true; heading.background=0xE2F1EBFF; heading.foreground=0x155E4BFF;
    b.rows={{0,title_style},{2,heading},{10,heading}};
    auto result=sheet_.apply(b);
    message_=result.accepted?"Example workbook. Edit any cell to make it yours.":result.error->context;
    sheet_.clear_journal();
    populated_=sheet_.populated_cells(); modified_=false; select({3,2}); viewport_.first_row=0; viewport_.first_column=0;
}
void GridUI::remember_target(int index) {
    auto a=ImGui::GetItemRectMin(), b=ImGui::GetItemRectMax(); targets[std::size_t(index)]={(a.x+b.x)/2,(a.y+b.y)/2};
}
void GridUI::set_dark(bool dark,bool remember) {
    dark_=dark; remember_theme_=remember;
    if(!remember) return;
    try {
        const auto path=appearance_path();
        if(path.empty()) { message_="Appearance changed for this session; no settings folder is available."; return; }
        if(!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path); out<<(dark?"dark":"light")<<'\n'; out.close();
        if(!out) message_="Appearance changed, but the preference could not be saved.";
    } catch(const std::exception&) { message_="Appearance changed, but the preference could not be saved."; }
}
void GridUI::set_scale(float scale) {
    scale_=scale; viewport_.row_height=37*scale*zoom_; viewport_.column_width=144*scale*zoom_;
    viewport_.row_header=44*scale; viewport_.column_header=39*scale;
}
void GridUI::load_performance_fixture() {
    sheet_=Sheet({},&lua_); Batch data;
    for(std::uint32_t i=0;i<10'000;++i) data.cells.push_back({{i*100,i%16},double(i)});
    const auto result=sheet_.apply(data);
    message_=result.accepted?"Sparse benchmark fixture: 10,000 populated cells.":result.error->context;
    select({0,0}); counted_revision_=~std::uint64_t{}; modified_=false;
}
void GridUI::select(CellCoord c,bool extend) {
    active_=c; if(!extend) anchor_=c;
    selection_changed_=true; editing_=false;
}
void GridUI::refresh_editor() {
    label(active_,address_.data(),address_.size());
    const auto* cell=sheet_.cell(active_); oversized_=false;
    if(!cell) editor_[0]=0;
    else {
        std::string value;
        if(auto formula=std::get_if<FormulaInput>(&cell->input)) value=formula->source;
        else if(auto text=std::get_if<std::string>(&cell->input)) value="'"+*text;
        else value=display(cell->cached);
        oversized_=value.size()>=editor_.size();
        if(oversized_) { editor_[0]=0; message_="Cell exceeds the editor limit; value preserved. Clear it explicitly to replace."; }
        else std::snprintf(editor_.data(),editor_.size(),"%s",value.c_str());
    }
    selection_changed_=false; editor_dirty_=false;
}
void GridUI::queue_edit() {
    if(oversized_) return;
    queued_.cells.push_back({active_,interpret(editor_.data())});
    editing_=false; editor_dirty_=false; grid_focused_=true;
}
void GridUI::smoke_queue_edit(CellCoord c,const char* text) {
    select(c); std::snprintf(editor_.data(),editor_.size(),"%s",text); queue_edit();
}
void GridUI::clear_selection() {
    if(viewport_.filtered){message_="Clear the filter before deleting a range.";return;}
    if(!row_selection_.empty()) {
        auto result=sheet_.clear_rows(RowSelection(row_selection_));
        message_=result.accepted?"Selected row contents cleared.":result.error->context;
        modified_|=result.accepted; return;
    }
    const auto top=std::min(active_.row,anchor_.row), bottom=std::max(active_.row,anchor_.row);
    const auto left=std::min(active_.column,anchor_.column),right=std::max(active_.column,anchor_.column);
    // Clear only existing cells; large blank selections never materialize.
    Batch b;
    for(auto r=sheet_.populated_rows().lower_bound(top);r!=sheet_.populated_rows().end()&&r->first<=bottom;++r)
        for(auto c=r->second.lower_bound(left);c!=r->second.end()&&c->first<=right;++c) b.cells.push_back({{r->first,c->first},std::monostate{}});
    auto result=sheet_.apply(b); modified_|=result.accepted&&!b.cells.empty();
    message_=result.accepted?"Contents cleared.":result.error->context;
}
void GridUI::style_selection(Action action) {
    if(viewport_.filtered){message_="Clear the filter before formatting rows.";return;}
    const auto intervals=row_selection_.empty()?std::vector<RowInterval>{{std::min(active_.row,anchor_.row),std::max(active_.row,anchor_.row)}}:row_selection_;
    std::size_t count=0; for(auto interval:intervals) count+=interval.last-interval.first+1;
    if(count>10'000) { message_="Format at most 10,000 rows per batch."; return; }
    Batch b; const bool bold=!sheet_.row_style(active_.row).bold;
    for(auto interval:intervals) for(auto row=interval.first;row<=interval.last;++row) {
        auto style=sheet_.row_style(row);
        if(action==Action::Bold) style.bold=bold;
        if(action==Action::Tint) style.background=0xE2F1EBFF;
        if(action==Action::ResetStyle) style=Style{};
        if(action==Action::DecimalsLess) style.decimals=std::max(0,style.decimals-1);
        if(action==Action::DecimalsMore) style.decimals=std::min(12,style.decimals+1);
        b.rows.push_back({row,style});
    }
    auto result=sheet_.apply(b); modified_|=result.accepted;
    message_=result.accepted?"Row formatting applied.":result.error->context;
}
namespace {
#ifndef _WIN32
// macOS and Linux have no dialog API in GLFW; use the desktop's own dialogs (AppleScript, zenity or kdialog).
std::string shell_quote(std::string_view text) {
    std::string out="'"; for(char c:text) { if(c=='\'') out+="'\\''"; else out+=c; } out+="'"; return out;
}
struct CommandResult { int status=-1; std::string output; };
CommandResult run_command(const std::string& command) {
    CommandResult result; FILE* pipe=popen(command.c_str(),"r"); if(!pipe) return result;
    char buffer[4096]; while(std::fgets(buffer,sizeof buffer,pipe)) result.output+=buffer;
    const int status=pclose(pipe); result.status=WIFEXITED(status)?WEXITSTATUS(status):-1;
    while(!result.output.empty()&&(result.output.back()=='\n'||result.output.back()=='\r')) result.output.pop_back();
    return result;
}
[[maybe_unused]] bool has_command(const char* name) { return std::system((std::string("command -v ")+name+" >/dev/null 2>&1").c_str())==0; }
std::string applescript_quote(std::string_view text) {
    std::string out="\""; for(char c:text) { if(c=='"'||c=='\\') out+='\\'; out+=c; } out+="\""; return out;
}
#endif
std::optional<std::filesystem::path> workbook_dialog(bool save,const std::filesystem::path& current,void* owner,const wchar_t* extension=L"julretsu") {
#ifdef _WIN32
    std::array<wchar_t,32768> name{};
    const auto initial=current.empty()?(std::wstring(L"Untitled workbook.")+extension):current.wstring();
    std::copy_n(initial.c_str(),std::min(initial.size(),name.size()-1),name.data());
    OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=static_cast<HWND>(owner);
    dialog.lpstrFilter=std::wstring_view(extension)==L"html"?L"Chart report (*.html)\0*.html\0\0":std::wstring_view(extension)==L"xlsx"?L"Excel Workbook (*.xlsx)\0*.xlsx\0\0":std::wstring_view(extension)==L"csv"?L"CSV (UTF-8) (*.csv)\0*.csv\0\0":L"Julretsu Workbook (*.julretsu)\0*.julretsu\0\0";
    dialog.lpstrFile=name.data(); dialog.nMaxFile=DWORD(name.size()); dialog.lpstrDefExt=extension;
    dialog.lpstrTitle=save?L"Save workbook locally":L"Open a Julretsu workbook";
    dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    if(save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog)) return std::filesystem::path(name.data());
    if(CommDlgExtendedError()) throw std::runtime_error("Windows could not open the file dialog. Please try again.");
    return std::nullopt;
#else
    (void)owner;
    std::string ext; for(const wchar_t* c=extension;*c;++c) ext+=char(*c);
    const auto name=current.empty()?std::string("Untitled workbook.")+ext:current.filename().string();
    const auto folder=current.empty()?std::string():current.parent_path().string();
    const std::string title=save?"Save workbook":"Open a file";
    CommandResult chosen;
#ifdef __APPLE__
    std::string script=save?"POSIX path of (choose file name with prompt "+applescript_quote(title)+" default name "+applescript_quote(name)+")"
                           :"POSIX path of (choose file with prompt "+applescript_quote(title)+" of type {"+applescript_quote(ext)+"})";
    chosen=run_command("osascript -e "+shell_quote(script)+" 2>/dev/null");
#else
    const auto start=folder.empty()?name:(std::filesystem::path(folder)/name).string();
    if(has_command("zenity"))
        chosen=run_command("zenity --file-selection "+std::string(save?"--save --confirm-overwrite ":"")+"--title="+shell_quote(title)+" --filename="+shell_quote(start)+" --file-filter="+shell_quote("*."+ext)+" 2>/dev/null");
    else if(has_command("kdialog"))
        chosen=run_command(std::string(save?"kdialog --getsavefilename ":"kdialog --getopenfilename ")+shell_quote(start)+" "+shell_quote("*."+ext)+" 2>/dev/null");
    else throw std::runtime_error("Julretsu needs zenity or kdialog to show file dialogs. Install one, for example: sudo apt install zenity");
#endif
    if(chosen.status!=0||chosen.output.empty()) return std::nullopt; // cancelled
    std::filesystem::path path(chosen.output);
    if(save&&path.extension()!="."+ext) path+="."+ext;
    return path;
#endif
}
std::string path_label(const std::filesystem::path& path) {
    const auto utf8=path.u8string(); return std::string(reinterpret_cast<const char*>(utf8.data()),utf8.size());
}
}
bool GridUI::save_to(const std::filesystem::path& path) {
    try {
        if(editor_dirty_) queue_edit();
        if(!queued_.cells.empty()) {
            auto result=sheet_.apply(queued_); queued_={};
            if(!result.accepted) {editor_dirty_=true;throw std::runtime_error(result.error->context);}
            modified_=true; selection_changed_=true;
        }
        if(!save_document_to(path)) return false;
        file_path_=path; workbook_name_=path_label(path.filename()); file_label_=path_label(path);
        modified_=false; editor_dirty_=false; message_="Saved to this device."; discard_recovery(); return true;
    } catch(const std::exception& error) { message_=error.what(); file_error_=message_; return false; }
}
bool GridUI::save(bool save_as) {
    try {
        auto path=file_path_;
        if(save_as||path.empty()) { auto chosen=workbook_dialog(true,path,native_window); if(!chosen) {message_="Save cancelled.";return false;} path=*chosen; }
        if(path.extension()!=L".julretsu") { message_="Save uses the .julretsu format. Use File > Export for CSV or Excel."; file_error_=message_; return false; }
        return save_to(path);
    } catch(const std::exception& error) {message_=error.what(); file_error_=message_;return false;}
}
bool GridUI::open_from(const std::filesystem::path& path) {
    try {
        auto document=read_document(path);std::vector<StoredSheet> loaded;
        for(const auto& item:document) {auto data=deserialize_sheet(item.bytes);Sheet sheet({},&lua_);auto r=sheet.apply(data.data);if(!r.accepted)throw std::runtime_error(r.error->context);sheet.clear_history();sheet.set_journal(std::move(data.journal));loaded.push_back({item.name,std::move(sheet),data.script});}
        auto workbook=deserialize_sheet(document[0].bytes);
        Limits limits; limits.batch_edits=max_rows;
        Sheet replacement(limits,&lua_); auto result=replacement.apply(workbook.data);
        if(!result.accepted) throw std::runtime_error(result.error->context);
        // Install only after complete parsing and validation; an invalid file cannot erase work.
        sheets_=std::move(loaded);current_sheet_=0;
        replacement.clear_history(); replacement.set_journal(std::move(workbook.journal)); sheet_=std::move(replacement); queued_={}; row_selection_.clear();
        std::snprintf(script_.data(),script_.size(),"%s",workbook.script.c_str());
        file_path_=path; workbook_name_=path_label(path.filename()); file_label_=path_label(path);
        viewport_.filtered=false;viewport_.filtered_rows.clear();recalculate_links();
        modified_=false; editor_dirty_=false; counted_revision_=~std::uint64_t{};
        select({0,0}); viewport_.first_row=0;viewport_.first_column=0;message_="Workbook opened.";return true;
    } catch(const std::exception& error) {message_=error.what();file_error_=message_;return false;}
}
bool GridUI::confirm_close() {
    if(!modified()) return true;
#ifdef _WIN32
    const int answer=MessageBoxW(static_cast<HWND>(native_window),L"Do you want to save your changes?\n\nSave keeps your workbook on this device. Choose No to discard your changes, or Cancel to keep editing.",L"Julretsu - Save changes?",MB_YESNOCANCEL|MB_ICONQUESTION);
    if(answer==IDYES) return save(false);
    return answer==IDNO;
#else
    const char* question="Do you want to save your changes? Save keeps your workbook on this device. Don't Save discards your changes.";
#ifdef __APPLE__
    const auto answer=run_command("osascript -e "+shell_quote(std::string("button returned of (display dialog ")+applescript_quote(question)+
        " buttons {\"Cancel\", \"Don't Save\", \"Save\"} default button \"Save\" cancel button \"Cancel\" with title \"Julretsu\")")+" 2>/dev/null");
    if(answer.status==0&&answer.output=="Save") return save(false);
    return answer.status==0&&answer.output=="Don't Save";
#else
    if(has_command("zenity")) {
        const auto answer=run_command(std::string("zenity --question --title=Julretsu --ok-label=Save --cancel-label=Cancel --extra-button=")+shell_quote("Don't Save")+" --text="+shell_quote(question)+" 2>/dev/null");
        if(answer.output=="Don't Save") return true;
        return answer.status==0&&save(false);
    }
    if(has_command("kdialog")) {
        const auto answer=run_command(std::string("kdialog --title Julretsu --yes-label Save --no-label ")+shell_quote("Don't Save")+" --yesnocancel "+shell_quote(question)+" 2>/dev/null");
        if(answer.status==0) return save(false);
        return answer.status==1;
    }
    message_="Save the workbook before replacing or closing it.";return false;
#endif
#endif
}
void GridUI::open() {
    try {
        auto path=workbook_dialog(false,file_path_,native_window);
        if(path&&confirm_close()) open_from(*path);
    } catch(const std::exception& error) {message_=error.what();file_error_=message_;}
}

void GridUI::csv_file(bool exporting) {
    try {
        auto chosen=workbook_dialog(exporting,{},native_window,L"csv");if(!chosen)return;
        if(exporting) {
            if(editor_dirty_) {queue_edit();auto result=sheet_.apply(queued_);queued_={};if(!result.accepted)throw std::runtime_error(result.error->context);modified_=true;selection_changed_=true;}
            std::ostringstream out;CsvAdapter adapter;StreamOptions options;
            auto result=adapter.export_sheet(out,sheet_,options);if(result.error)throw std::runtime_error(result.error->context);
            write_file_atomic(*chosen,out.str());message_="CSV exported: values only; formula-like text has a protective apostrophe. Save .julretsu to keep formulas and formatting.";
        } else {
            if(!confirm_close())return;
            std::ifstream in(*chosen,std::ios::binary);if(!in)throw std::runtime_error("Cannot open CSV file.");
            Sheet replacement({},&lua_);replacement.label_next_change("Import CSV");CsvAdapter adapter;StreamOptions options;options.interpretation=ImportInterpretation::Values;
            auto result=adapter.import_sheet(in,replacement,options);if(result.error)throw std::runtime_error(result.error->context);
            replacement.clear_history();sheet_=std::move(replacement);sheets_.clear();sheets_.push_back({"Sheet 1",Sheet({},&lua_),""});current_sheet_=0;viewport_.filtered=false;viewport_.filtered_rows.clear();file_path_.clear();workbook_name_="Imported CSV";file_label_.clear();script_[0]=0;
            modified_=true;editor_dirty_=false;counted_revision_=~std::uint64_t{};row_selection_.clear();select({0,0});viewport_.first_row=viewport_.first_column=0;
            message_="CSV imported. Numbers and Booleans detected; formulas remain text. Save as .julretsu to keep your work.";
        }
    }catch(const std::exception& e){message_=e.what();file_error_=message_;}
}

void GridUI::xlsx_file(bool exporting) {
    try {
        auto chosen=workbook_dialog(exporting,{},native_window,L"xlsx");if(!chosen)return;
        if(exporting) {
#ifdef _WIN32
            if(MessageBoxW(static_cast<HWND>(native_window),L"Export one worksheet with supported arithmetic, SUM, AVERAGE and IF formulas and basic row formatting. Lua and unsupported formulas become values. Decimal styles use 0 or 2 places. Lua scripts are omitted. Keep a .julretsu copy for full fidelity. Continue?",L"Export Excel workbook",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)return;
#endif
            if(editor_dirty_){queue_edit();auto r=sheet_.apply(queued_);queued_={};if(!r.accepted)throw std::runtime_error(r.error->context);modified_=true;selection_changed_=true;}
            message_=write_xlsx(*chosen,sheet_);
        } else {
            auto imported=read_xlsx(*chosen,xlsx_formulas_);
#ifdef _WIN32
            const int length=MultiByteToWideChar(CP_UTF8,0,imported.summary.c_str(),-1,nullptr,0);std::wstring summary(std::size_t(length),L'\0');MultiByteToWideChar(CP_UTF8,0,imported.summary.c_str(),-1,summary.data(),length);
            if(MessageBoxW(static_cast<HWND>(native_window),summary.c_str(),L"Excel import preview",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)return;
#endif
            if(!confirm_close())return;Sheet replacement({},&lua_);replacement.label_next_change("Import Excel");auto result=replacement.apply(imported.data);if(!result.accepted)throw std::runtime_error(result.error->context);
            replacement.clear_history();sheet_=std::move(replacement);sheets_.clear();sheets_.push_back({"Sheet 1",Sheet({},&lua_),""});current_sheet_=0;viewport_.filtered=false;viewport_.filtered_rows.clear();file_path_.clear();workbook_name_="Imported Excel workbook";file_label_.clear();script_[0]=0;
            modified_=true;editor_dirty_=false;counted_revision_=~std::uint64_t{};row_selection_.clear();select({0,0});viewport_.first_row=viewport_.first_column=0;
            message_="Excel worksheet imported. Save .julretsu to keep changes. Import compatibility details were shown before loading.";
        }
    }catch(const std::exception& e){message_=e.what();file_error_=message_;}
}

void GridUI::ai_use_settings(const AiSettings& settings) {
    ai_settings_=settings;
    std::snprintf(ai_model_.data(),ai_model_.size(),"%s",settings.model.c_str());
    std::snprintf(ai_endpoint_.data(),ai_endpoint_.size(),"%s",settings.endpoint.c_str());
    ai_key_saved_=load_ai_key(settings.provider).has_value();
}
void GridUI::ai_send() {
    try {
        AiSettings settings=ai_settings_; settings.model=ai_model_.data(); settings.endpoint=ai_endpoint_.data();
        std::string key=load_ai_key(settings.provider).value_or(std::string());
        if(settings.provider==AiProvider::Anthropic&&key.empty()) { ai_connection_open_=true; throw std::runtime_error("Add your Anthropic API key under Connection first."); }
        if(editor_dirty_) queue_edit();
        if(!queued_.cells.empty()) { auto r=sheet_.apply(queued_); queued_={}; modified_|=r.accepted; }
        auto request=build_ai_request(settings,key,ai_prompt_.data(),describe_sheet(sheet_,active_,anchor_));
        std::fill(key.begin(),key.end(),'\0');
        ai_session_->start(std::move(request)); ai_request_settings_=settings; ai_proposal_.reset();
        ai_status_="Waiting for "+endpoint_host(settings.endpoint)+"...";
    } catch(const std::exception& error) { ai_status_=error.what(); }
}
void GridUI::ai_poll() {
    if(auto result=ai_session_->poll()) {
        if(!result->second.empty()) { ai_status_=result->second; return; }
        try {
            ai_proposal_=parse_ai_proposal(extract_ai_text(ai_request_settings_,result->first),sheet_); ai_revision_=sheet_.revision(); ai_connection_collapse_=true;
            ai_status_=ai_proposal_->edits.empty()?"No edits were proposed.":"Review the proposed edits, then apply the ones you want.";
        } catch(const std::exception& error) { ai_status_=error.what(); }
    }
    if(ai_proposal_&&ai_revision_!=sheet_.revision()) { refresh_proposal(*ai_proposal_,sheet_); ai_revision_=sheet_.revision(); }
}
void GridUI::ai_apply() {
    if(!ai_proposal_) return;
    const auto batch=proposal_batch(*ai_proposal_);
    if(batch.cells.empty()) { message_="Tick at least one proposed edit to apply."; return; }
    auto result=sheet_.apply(batch);
    if(!result.accepted) { message_=result.error->context; ai_status_="The edits could not be applied: "+message_; return; }
    modified_=true; ai_proposal_.reset(); ai_status_="Applied. Ctrl+Z undoes all of these edits together.";
    message_="Applied "+std::to_string(batch.cells.size())+" AI-proposed edits in one step.";
}
void GridUI::smoke_ai_proposal(std::string_view reply) {
    ai_proposal_=parse_ai_proposal(reply,sheet_); ai_revision_=sheet_.revision(); ai_connection_collapse_=true; ai_status_="Smoke proposal.";
}

Value GridUI::read_sheet_cell(std::size_t index,CellCoord c,std::set<std::pair<std::size_t,CellCoord>>& visiting,unsigned& work) {
    if(++work>10000||visiting.size()>64)return CellError{ErrorCode::Limit,"Cross-sheet evaluation limit",{}};
    auto key=std::make_pair(index,c);if(!visiting.insert(key).second)return CellError{ErrorCode::Cycle,"Circular worksheet reference",c};
    const auto& sheet=index==current_sheet_?sheet_:sheets_[index].sheet;auto cell=sheet.cell(c);Value value;
    if(cell&&cell->formula) {
        struct LinkReader:FormulaExtension {
            std::function<Value(std::string_view,CellCoord)> link;
            Value evaluate(std::span<const Value>,std::size_t)const override{return CellError{ErrorCode::Unsupported,"Lua execution through sheet links is unsupported",{}};}
            Value sheet_reference(std::string_view name,CellCoord c)const override{return link(name,c);}
        } extension;
        extension.link=[&](std::string_view name,CellCoord target)->Value {for(std::size_t i=0;i<sheets_.size();++i)if(sheets_[i].name==name)return read_sheet_cell(i,target,visiting,work);return CellError{ErrorCode::Ref,"Unknown worksheet",{}};};
        value=evaluate_formula(*cell->formula,[&](CellCoord target){return read_sheet_cell(index,target,visiting,work);},{},&extension);
    } else value=sheet.read(c);
    visiting.erase(key);return value;
}
void GridUI::recalculate_links() {
    Batch batch;for(const auto& [r,row]:sheet_.populated_rows())for(const auto& [c,cell]:row)if(auto f=std::get_if<FormulaInput>(&cell.input);f&&cell.formula&&std::any_of(cell.formula->nodes.begin(),cell.formula->nodes.end(),[](const AstNode& n){return n.kind==NodeKind::Call&&n.op=="SHEET";}))batch.cells.push_back({{r,c},cell.input});
    if(!batch.cells.empty()){auto result=sheet_.refresh_values(batch);if(!result.accepted)throw std::runtime_error(result.error->context);}
}
bool GridUI::save_document_to(const std::filesystem::path& path) {
    Sheet snapshot=sheet_;Batch pending=queued_;
    if(editor_dirty_)pending.cells.push_back({active_,interpret(editor_.data())});
    if(!pending.cells.empty()){auto result=snapshot.apply(pending);if(!result.accepted)throw std::runtime_error(result.error->context);}
    std::vector<DocumentSheet> document;
    if(sheets_.size()<=1&&sheets_[0].name=="Sheet 1") {write_workbook(path,snapshot,script_.data());return true;}
    for(std::size_t i=0;i<sheets_.size();++i)document.push_back({sheets_[i].name,serialize_sheet(i==current_sheet_?snapshot:sheets_[i].sheet,i==current_sheet_?script_.data():sheets_[i].script)});
    write_document(path,document);return true;
}
void GridUI::change_sheet(std::size_t index) {
    if(index>=sheets_.size()||index==current_sheet_)return;
    if(editor_dirty_)queue_edit();if(!queued_.cells.empty()){auto r=sheet_.apply(queued_);queued_={};if(!r.accepted)throw std::runtime_error(r.error->context);modified_=true;}
    sheets_[current_sheet_].sheet=std::move(sheet_);sheets_[current_sheet_].script=script_.data();
    current_sheet_=index;sheet_=std::move(sheets_[index].sheet);std::snprintf(script_.data(),script_.size(),"%s",sheets_[index].script.c_str());
    viewport_.filtered=false;viewport_.filtered_rows.clear();
    recalculate_links();
    clipboard_cells_.clear();clipboard_cut_=false;row_selection_.clear();select({0,0});viewport_.first_row=viewport_.first_column=0;counted_revision_=~std::uint64_t{};ai_proposal_.reset();
}
void GridUI::sheet_action(int action) {
    if(editor_dirty_)queue_edit();if(!queued_.cells.empty()){auto result=sheet_.apply(queued_);queued_={};if(!result.accepted)throw std::runtime_error(result.error->context);modified_=true;}
    if(action==0||action==1) {
        if(sheets_.size()>=64)throw std::runtime_error("At most 64 worksheets per workbook.");
        unsigned n=1;std::string name;do{name="Sheet "+std::to_string(n++);}while(std::any_of(sheets_.begin(),sheets_.end(),[&](const auto& s){return s.name==name;}));
        sheets_.push_back({name,action==1?sheet_:Sheet({},&lua_),action==1?script_.data():""});change_sheet(sheets_.size()-1);
    } else if(action==2) {
        if(sheets_.size()==1)throw std::runtime_error("Keep at least one worksheet.");
        auto old=current_sheet_;change_sheet(old?old-1:1);sheets_.erase(sheets_.begin()+old);if(current_sheet_>old)--current_sheet_;
    } else if(action==3) {
        std::string name=sheet_name_.data();if(name.empty()||name.find_first_of("[]:*?/\\!")!=std::string::npos)throw std::runtime_error("Choose a nonempty sheet name without []:*?/\\!.");
        for(std::size_t i=0;i<sheets_.size();++i)if(i!=current_sheet_&&sheets_[i].name==name)throw std::runtime_error("That name is already used.");
        if(name.find('"')!=std::string::npos)throw std::runtime_error("Sheet names cannot contain double quotes.");
        const auto old_name=sheets_[current_sheet_].name;
        std::vector<Batch> updates(sheets_.size());
        const std::regex pattern(R"LINK((SHEET\s*\(\s*")((?:[^"]|"")*)("\s*,))LINK",std::regex::icase);
        for(std::size_t i=0;i<sheets_.size();++i){const auto& sheet=i==current_sheet_?sheet_:sheets_[i].sheet;
            for(const auto& [r,row]:sheet.populated_rows())for(const auto& [c,cell]:row)if(auto formula=std::get_if<FormulaInput>(&cell.input)){
                std::string source;std::size_t end=0;bool changed=false;
                for(std::sregex_iterator it(formula->source.begin(),formula->source.end(),pattern),last;it!=last;++it){const auto& m=*it;source+=formula->source.substr(end,std::size_t(m.position())-end);source+=m[1].str();bool match=m[2].str()==old_name;source+=match?name:m[2].str();source+=m[3].str();changed|=match;end=std::size_t(m.position()+m.length());}
                if(changed){source+=formula->source.substr(end);updates[i].cells.push_back({{r,c},FormulaInput{source}});}
            }
        }
        // Validate on copies before changing any worksheet or its name.
        std::vector<Sheet> replacements;for(std::size_t i=0;i<sheets_.size();++i){replacements.push_back(i==current_sheet_?sheet_:sheets_[i].sheet);auto r=replacements.back().apply(updates[i]);if(!r.accepted)throw std::runtime_error(r.error->context);}
        sheets_[current_sheet_].name=name;for(std::size_t i=0;i<sheets_.size();++i){if(i==current_sheet_)sheet_=std::move(replacements[i]);else sheets_[i].sheet=std::move(replacements[i]);}recalculate_links();
    } else {
        auto target=action==4?(current_sheet_?current_sheet_-1:current_sheet_):std::min(current_sheet_+1,sheets_.size()-1);
        std::swap(sheets_[target],sheets_[current_sheet_]);current_sheet_=target;
    }
    modified_=true;
}
void GridUI::discard_recovery() {
    if(recovery_disabled_) return;
    std::error_code ignored; std::filesystem::remove(appearance_path().parent_path()/"recovery.julretsu",ignored);
}
void GridUI::recovery_tick() {
    auto now=std::chrono::steady_clock::now();if(recovery_disabled_||sheets_.empty()||recovery_open_||!autosave_||!modified()||now-autosave_time_<std::chrono::minutes(1))return;autosave_time_=now;
    try {auto path=appearance_path().parent_path()/"recovery.julretsu";std::filesystem::create_directories(path.parent_path());save_document_to(path);recovery_error_.clear();}
    catch(const std::exception& e){recovery_error_=e.what();message_="Recovery copy failed: "+recovery_error_;}
}
void GridUI::draw_sheets() {
    if(sheets_open_)ImGui::OpenPopup("Manage worksheets");
    if(ImGui::BeginPopupModal("Manage worksheets",&sheets_open_,ImGuiWindowFlags_AlwaysAutoResize)) {
        for(std::size_t i=0;i<sheets_.size();++i)if(ImGui::Selectable(sheets_[i].name.c_str(),i==current_sheet_))switch_sheet_=int(i);
        if(ImGui::Button("Add"))action_=Action::AddSheet;ImGui::SameLine();if(ImGui::Button("Duplicate"))action_=Action::DuplicateSheet;
        if(ImGui::Button("Move left"))action_=Action::MoveSheetLeft;ImGui::SameLine();if(ImGui::Button("Move right"))action_=Action::MoveSheetRight;
        ImGui::InputText("New name",sheet_name_.data(),sheet_name_.size());if(ImGui::Button("Rename"))action_=Action::RenameSheet;
        if(ImGui::Button("Delete current sheet..."))ImGui::OpenPopup("Delete worksheet?");
        if(ImGui::BeginPopupModal("Delete worksheet?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {ImGui::TextUnformatted("Delete this worksheet and its contents? This cannot be undone.");if(ImGui::Button("Delete")){action_=Action::DeleteSheet;ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();ImGui::EndPopup();}
        ImGui::TextUnformatted("Reference a sheet: =SHEET(\"Sheet 1\",\"A1\")");
        if(ImGui::Button("Close")){sheets_open_=false;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
    }
    if(recovery_open_)ImGui::OpenPopup("Workbook recovery");
    if(ImGui::BeginPopupModal("Workbook recovery",&recovery_open_,ImGuiWindowFlags_AlwaysAutoResize)) {
        auto path=appearance_path().parent_path()/"recovery.julretsu";bool available=std::filesystem::exists(path);
        ImGui::TextUnformatted(available?"A recovery copy is available. Save current work before restoring.":"No recovery copy is available yet.");
        ImGui::BeginDisabled(!available);if(ImGui::Button("Restore")){if(confirm_close()&&open_from(path)){discard_recovery();file_path_.clear();file_label_.clear();modified_=true;recovery_open_=false;ImGui::CloseCurrentPopup();}}ImGui::EndDisabled();ImGui::SameLine();
        if(ImGui::Button("Keep current workbook")){discard_recovery();recovery_open_=false;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
    }
}
void GridUI::prepare_report() {
    const auto chart_column=active_.column;
    // A single selected cell means the user wants a report of the worksheet.
    if(active_==anchor_) {
        report_first_={max_rows-1,max_columns-1}; report_last_={};
        for(const auto& [r,row]:sheet_.populated_rows()) for(const auto& [c,cell]:row) {
            (void)cell; report_first_.row=std::min(report_first_.row,r); report_first_.column=std::min(report_first_.column,c);
            report_last_.row=std::max(report_last_.row,r); report_last_.column=std::max(report_last_.column,c);
        }
        if(sheet_.populated_cells()==0) throw std::runtime_error("The worksheet is empty.");
    } else {
    report_first_={std::min(active_.row,anchor_.row),std::min(active_.column,anchor_.column)};
    report_last_={std::max(active_.row,anchor_.row),std::max(active_.column,anchor_.column)};
    }
    if(std::uint64_t(report_last_.row-report_first_.row+1)*(report_last_.column-report_first_.column+1)>10000||report_last_.column-report_first_.column>=12)throw std::runtime_error("Select at most 10,000 cells and 12 columns for a report.");
    report_rows_.clear();report_values_.clear();chart_values_.clear();
    for(unsigned r=report_first_.row;r<=report_last_.row;++r){
        if(viewport_.filtered&&std::find(viewport_.filtered_rows.begin(),viewport_.filtered_rows.end(),r)==viewport_.filtered_rows.end())continue;
        std::vector<std::string> row; std::vector<Value> values;
        for(unsigned c=report_first_.column;c<=report_last_.column;++c){auto value=sheet_.read({r,c});row.push_back(display(value));values.push_back(value);}
        report_rows_.push_back(std::move(row));report_values_.push_back(std::move(values));
    }
    report_column_=int(std::clamp(chart_column,report_first_.column,report_last_.column)-report_first_.column);
    rebuild_chart();
    if(chart_values_.empty()) for(unsigned c=0;c<=report_last_.column-report_first_.column;++c) {report_column_=int(c);rebuild_chart();if(!chart_values_.empty())break;}
    std::snprintf(report_title_.data(),report_title_.size(),"%s",workbook_name_.c_str());report_open_=true;
}
void GridUI::rebuild_chart() {
    chart_values_.clear();
    for(const auto& row:report_values_) if(auto n=std::get_if<double>(&row.at(report_column_));n&&std::isfinite(*n)&&std::abs(*n)<=1e30&&chart_values_.size()<1000)chart_values_.push_back(float(*n));
}
void GridUI::draw_report() {
    if(report_open_)ImGui::OpenPopup("Chart and print preview");
    ImGui::SetNextWindowSize({760*scale_,650*scale_},ImGuiCond_Appearing);
    if(ImGui::BeginPopupModal("Chart and print preview",&report_open_)){
        ImGui::InputText("Report title",report_title_.data(),report_title_.size());
        ImGui::Text("Report range: %s : %s",to_address(report_first_).c_str(),to_address(report_last_).c_str());
        ImGui::TextWrapped("Select a range before opening to report only those cells. Charts include up to 1,000 numeric values.");
        std::string selected_column=to_address({0,report_first_.column+unsigned(report_column_)});selected_column.pop_back();
        if(ImGui::BeginCombo("Chart column",selected_column.c_str())) {
            for(unsigned c=0;c<=report_last_.column-report_first_.column;++c){auto label=to_address({0,report_first_.column+c});label.pop_back();if(ImGui::Selectable(label.c_str(),report_column_==int(c))){report_column_=int(c);rebuild_chart();}}
            ImGui::EndCombo();
        }
        ImGui::Text("Chart: %zu values",chart_values_.size());
        ImGui::Checkbox("Line chart",&chart_line_);ImGui::SameLine();ImGui::Checkbox("Landscape print",&print_landscape_);ImGui::SameLine();ImGui::Checkbox("Repeat first row",&print_header_);
        if(!chart_values_.empty()){
            // Match the printed chart colour.
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram,ImVec4{0.12f,0.57f,0.43f,1});ImGui::PushStyleColor(ImGuiCol_PlotLines,ImVec4{0.12f,0.57f,0.43f,1});
            auto [lo,hi]=std::minmax_element(chart_values_.begin(),chart_values_.end());
            float low=std::min(0.0f,*lo),high=std::max(0.0f,*hi);if(low==high)high=low+1;float pad=(high-low)*0.05f;low-=pad;high+=pad;
            if(chart_line_&&chart_values_.size()>1)ImGui::PlotLines("##chart",chart_values_.data(),int(chart_values_.size()),0,nullptr,low,high,{-1,150*scale_});
            else ImGui::PlotHistogram("##chart",chart_values_.data(),int(chart_values_.size()),0,nullptr,low,high,{-1,150*scale_});
            ImGui::PopStyleColor(2);
            if(chart_line_&&chart_values_.size()==1)ImGui::TextUnformatted("One value: shown as a bar so it remains visible.");
        }else ImGui::TextDisabled("Select a numeric column to draw a chart.");
        if(!report_rows_.empty()&&ImGui::BeginTable("Report table",int(report_rows_[0].size()),ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY,{-1,std::max(90*scale_,ImGui::GetContentRegionAvail().y-100*scale_)})){
            ImGuiListClipper clipper;clipper.Begin(int(report_rows_.size()));while(clipper.Step())for(int r=clipper.DisplayStart;r<clipper.DisplayEnd;++r){ImGui::TableNextRow();for(const auto& value:report_rows_[r]){ImGui::TableNextColumn();ImGui::TextUnformatted(value.c_str());}}ImGui::EndTable();
        }
        ImGui::TextWrapped("Export a report with its chart and table. Open the HTML file in a browser and use Print / Save as PDF on any platform.");
        if(ImGui::Button("Export chart + table (HTML)..."))action_=Action::ExportReport;
#ifdef _WIN32
        ImGui::SameLine();if(ImGui::Button("Print / Save as PDF..."))action_=Action::PrintReport;
#endif
        ImGui::SameLine();if(ImGui::Button("Close")){report_open_=false;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
    }
}
void GridUI::print_report(const std::filesystem::path& output) {
#ifdef _WIN32
    if(report_rows_.empty())throw std::runtime_error("Select some cells before printing.");
    PRINTDLGW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=static_cast<HWND>(native_window);dialog.Flags=PD_RETURNDC|PD_NOPAGENUMS|PD_NOSELECTION;
    if(!output.empty()){dialog.hDC=CreateDCW(L"WINSPOOL",L"Microsoft Print to PDF",nullptr,nullptr);if(!dialog.hDC)throw std::runtime_error("Microsoft Print to PDF is unavailable.");}
    else if(!PrintDlgW(&dialog)){if(dialog.hDevMode)GlobalFree(dialog.hDevMode);if(dialog.hDevNames)GlobalFree(dialog.hDevNames);return;}
    HDC dc=dialog.hDC;
    if(dialog.hDevMode){auto mode=static_cast<DEVMODEW*>(GlobalLock(dialog.hDevMode));if(mode){mode->dmFields|=DM_ORIENTATION;mode->dmOrientation=print_landscape_?DMORIENT_LANDSCAPE:DMORIENT_PORTRAIT;ResetDCW(dc,mode);GlobalUnlock(dialog.hDevMode);}}
    auto wide=[](std::string_view text){int n=MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),nullptr,0);std::wstring out(n,L' ');MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),out.data(),n);return out;};
    auto title=wide(report_title_.data());DOCINFOW doc{};doc.cbSize=sizeof(doc);doc.lpszDocName=title.c_str();const auto output_name=output.wstring();if(!output.empty())doc.lpszOutput=output_name.c_str();bool started=StartDocW(dc,&doc)>0,ok=started;
    const int dpi=GetDeviceCaps(dc,LOGPIXELSY),margin=dpi/3,width=GetDeviceCaps(dc,HORZRES)-2*margin,height=GetDeviceCaps(dc,VERTRES)-2*margin,row_height=dpi/4;
    if(width<=0||height<row_height*5||row_height<=0){if(started)AbortDoc(dc);DeleteDC(dc);if(dialog.hDevMode)GlobalFree(dialog.hDevMode);if(dialog.hDevNames)GlobalFree(dialog.hDevNames);throw std::runtime_error("Printer page size is invalid.");}
    HFONT font=CreateFontW(-MulDiv(10,dpi,72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"Segoe UI");auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);
    unsigned page=0;std::size_t offset=0;int cols=int(report_rows_[0].size()),cw=width/cols;
    const auto draw_row=[&](std::size_t r,int y){for(int c=0;c<cols;++c){RECT rect{margin+c*cw,y,margin+(c+1)*cw,y+row_height};Rectangle(dc,rect.left,rect.top,rect.right,rect.bottom);rect.left+=dpi/24;rect.right-=dpi/24;auto text=wide(report_rows_[r][c]);DrawTextW(dc,text.c_str(),int(text.size()),&rect,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);}};
    while(ok&&offset<report_rows_.size()){
        ok=StartPage(dc)>0;if(!ok)break;++page;int y=margin;RECT heading{margin,y,margin+width,y+row_height*2};DrawTextW(dc,title.c_str(),int(title.size()),&heading,DT_SINGLELINE|DT_NOPREFIX);y+=row_height*2;
        if(page==1&&!chart_values_.empty()) {
            const int chart_height=std::min(dpi*2,height/3),chart_width=width;
            auto [min_it,max_it]=std::minmax_element(chart_values_.begin(),chart_values_.end());double low=std::min(0.0,double(*min_it)),high=std::max(0.0,double(*max_it));if(high==low)high=low+1;
            auto py=[&](float value){return y+chart_height-int((double(value)-low)/(high-low)*chart_height);};
            int zero=py(0);MoveToEx(dc,margin,zero,nullptr);LineTo(dc,margin+chart_width,zero);
            auto brush=CreateSolidBrush(RGB(30,145,110));auto previous=SelectObject(dc,brush);
            auto pen=CreatePen(PS_SOLID,std::max(1,dpi/72),RGB(30,145,110));auto old_pen=SelectObject(dc,pen);
            for(std::size_t i=0;i<chart_values_.size();++i){int x=margin+int(double(i)*chart_width/chart_values_.size()),xx=margin+int(double(i+1)*chart_width/chart_values_.size());int value_y=py(chart_values_[i]);if(chart_line_&&chart_values_.size()>1){if(i==0)MoveToEx(dc,x,value_y,nullptr);else LineTo(dc,x,value_y);}else Rectangle(dc,x,std::min(zero,value_y),std::max(x+1,xx-2),std::max(zero,value_y)+1);}
            SelectObject(dc,old_pen);DeleteObject(pen);SelectObject(dc,previous);DeleteObject(brush);y+=chart_height+row_height;
        }
        if(offset&&print_header_){draw_row(0,y);y+=row_height;}
        while(offset<report_rows_.size()&&y+row_height<margin+height-row_height){draw_row(offset++,y);y+=row_height;}
        auto label=std::wstring(L"Page ")+std::to_wstring(page);TextOutW(dc,margin,margin+height-row_height,label.c_str(),int(label.size()));ok=EndPage(dc)>0;
    }
    if(started){if(ok)ok=EndDoc(dc)>0;else AbortDoc(dc);}SelectObject(dc,old);DeleteObject(font);DeleteDC(dc);if(dialog.hDevMode)GlobalFree(dialog.hDevMode);if(dialog.hDevNames)GlobalFree(dialog.hDevNames);
    if(!ok)throw std::runtime_error("The printer could not complete the report.");message_="Report sent to the selected printer.";
#else
    (void)output;throw std::runtime_error("Native printing is currently available on Windows.");
#endif
}
bool GridUI::smoke_report_export(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir);
    load_demo();select({3,2});prepare_report();
    if(report_rows_.size()!=14||chart_values_.size()!=6)return false;
    chart_line_=true;export_report(dir/"budget.html");
#ifdef _WIN32
    print_report(dir/"budget.pdf");
#endif
    for(int choice=1;choice<=4;++choice){load_template(choice);prepare_report();if(chart_values_.size()<10)return false;
        for(const auto& row:report_values_)for(const auto& value:row)if(std::holds_alternative<CellError>(value))return false;
        export_report(dir/("template-"+std::to_string(choice)+".html"));
        if(!save_document_to(dir/("template-"+std::to_string(choice)+".julretsu")))return false;
    }
    sheet_=Sheet({},&lua_);Batch b;b.cells={{{0,0},3.0}};if(!sheet_.apply(b).accepted)return false;select({0,0});prepare_report();chart_line_=true;
    if(chart_values_.size()!=1)return false;export_report(dir/"single-value.html");
#ifdef _WIN32
    print_report(dir/"single-value.pdf");
#endif
    b.cells={{{0,0},-5.0},{{1,0},0.0},{{2,0},10.0},{{3,0},std::string("<script>alert(1)</script>")}};if(!sheet_.apply(b).accepted)return false;select({0,0});prepare_report();chart_line_=false;
    if(chart_values_.size()!=3)return false;export_report(dir/"mixed-values.html");
    std::ifstream in(dir/"mixed-values.html");std::string text((std::istreambuf_iterator<char>(in)),{});
    if(text.find("<script>")!=std::string::npos||text.find("&lt;script&gt;")==std::string::npos)return false;
    viewport_.filtered=true;viewport_.filtered_rows={0,2};prepare_report();if(chart_values_.size()!=2||report_rows_.size()!=2)return false;
    viewport_.filtered=false;report_open_=false;load_demo();return true;
}
void GridUI::export_report(const std::filesystem::path& path) {
    if(report_rows_.empty())throw std::runtime_error("No report data to export.");
    auto escape=[](std::string_view text){std::string out;for(char c:text){switch(c){case '&':out+="&amp;";break;case '<':out+="&lt;";break;case '>':out+="&gt;";break;case '"':out+="&quot;";break;default:out+=c;}}return out;};
    std::ostringstream html;html.imbue(std::locale::classic());
    html<<"<!doctype html><html lang=\"en\"><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width\"><title>"<<escape(report_title_.data())<<"</title><style>body{font:14px Arial,sans-serif;color:#18352c;margin:32px}h1{font-size:24px}svg{width:100%;height:auto;max-height:320px;break-inside:avoid}table{width:100%;border-collapse:collapse;table-layout:fixed}td{border:1px solid #aac4b7;padding:7px;overflow-wrap:anywhere;white-space:pre-wrap}thead{font-weight:bold;background:#e5f3ec}tr{break-inside:avoid}.hint{color:#456359}@media print{.hint{display:none}body{margin:0}}@page{size:A4 "<<(print_landscape_?"landscape":"portrait")<<";margin:14mm}</style><h1>"<<escape(report_title_.data())<<"</h1><p class=\"hint\">Julretsu report - use your browser's Print / Save as PDF. Chart and data are embedded in this file.</p>";
    if(!chart_values_.empty()) {
        auto [lo,hi]=std::minmax_element(chart_values_.begin(),chart_values_.end());double low=std::min(0.0,double(*lo)),high=std::max(0.0,double(*hi));if(low==high)high=low+1;
        double pad=(high-low)*.05;low-=pad;high+=pad;
        auto py=[&](double v){return 250-(v-low)/(high-low)*210;};
        auto px=[&](std::size_t i){return 85+(i+.5)*680/chart_values_.size();};
        auto column=to_address({0,report_first_.column+unsigned(report_column_)});column.pop_back();
        html<<"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 800 300\" role=\"img\" aria-label=\"Chart of column "<<column<<"\"><title>Column "<<column<<" - "<<chart_values_.size()<<" numeric values in row order</title><rect width=\"800\" height=\"300\" fill=\"white\"/>";
        for(int tick=0;tick<=4;++tick){double value=low+(high-low)*tick/4;html<<"<path d=\"M85 "<<py(value)<<" H765\" stroke=\"#d5e2db\"/><text x=\"78\" y=\""<<py(value)+4<<"\" text-anchor=\"end\" font-size=\"11\">"<<value<<"</text>";}
        html<<"<path d=\"M85 "<<py(0)<<" H765\" stroke=\"#63776d\"/>";
        if(chart_line_&&chart_values_.size()>1){html<<"<polyline fill=\"none\" stroke=\"#1e916e\" stroke-width=\"2\" points=\"";for(std::size_t i=0;i<chart_values_.size();++i)html<<px(i)<<","<<py(chart_values_[i])<<" ";html<<"\"/>";}
        for(std::size_t i=0;i<chart_values_.size();++i){double y=py(chart_values_[i]);
            if(chart_line_)html<<"<circle cx=\""<<px(i)<<"\" cy=\""<<y<<"\" r=\"3\" fill=\"#1e916e\"><title>"<<chart_values_[i]<<"</title></circle>";
            else{double w=std::max(.5,680.0/chart_values_.size()*.8);html<<"<rect x=\""<<px(i)-w/2<<"\" y=\""<<std::min(y,py(0))<<"\" width=\""<<w<<"\" height=\""<<std::max(1.0,std::abs(y-py(0)))<<"\" fill=\"#1e916e\"><title>"<<chart_values_[i]<<"</title></rect>";}
            if(i%std::max(std::size_t(1),chart_values_.size()/12)==0)html<<"<text x=\""<<px(i)<<"\" y=\"270\" text-anchor=\"middle\" font-size=\"11\">"<<i+1<<"</text>";
        }
        html<<"<text x=\"400\" y=\"294\" text-anchor=\"middle\" font-size=\"12\">Column "<<column<<" - numeric entries in row order (maximum 1,000)</text></svg>";
    } else html<<"<p>No numeric values in the chosen chart column.</p>";
    html<<"<table>";for(std::size_t r=0;r<report_rows_.size();++r){if(r==0&&print_header_)html<<"<thead>";html<<"<tr>";for(const auto& value:report_rows_[r])html<<"<td>"<<escape(value)<<"</td>";html<<"</tr>";if(r==0&&print_header_)html<<"</thead><tbody>";}if(print_header_)html<<"</tbody>";html<<"</table></html>";
    std::ofstream out(path,std::ios::binary);out<<html.str();out.close();if(!out)throw std::runtime_error("Could not save report. Check the destination and available space.");
}

void GridUI::load_template(int choice) {
    if(choice==0){load_demo();return;}
    // Template replacement has already passed the existing save/discard prompt.
    load_demo();sheet_=Sheet({},&lua_);sheets_.clear();sheets_.push_back({"Sheet 1",Sheet({},&lua_),""});current_sheet_=0;
    script_.fill(0);viewport_.filtered=false;viewport_.filtered_rows.clear();row_selection_.clear();
    Batch b;auto put=[&](unsigned r,unsigned c,Input value){b.cells.push_back({{r,c},std::move(value)});};
    const char* months[]={"January","February","March","April","May","June","July","August","September","October","November","December"};
    std::vector<std::string> headers;
    if(choice==1){workbook_name_="Monthly sales (sample)";headers={"Month","Online sales","Store sales","Total sales","Target","Difference"};for(unsigned i=0;i<12;++i){unsigned r=i+1;auto n=std::to_string(r+1);put(r,0,std::string(months[i]));put(r,1,2400.0+i*175+(i%3)*210);put(r,2,1800.0+i*95);put(r,3,FormulaInput{"=B"+n+"+C"+n});put(r,4,5000.0);put(r,5,FormulaInput{"=D"+n+"-E"+n});}}
    else if(choice==2){workbook_name_="Household expenses (sample)";headers={"Category","Planned","Actual","Remaining"};const char* labels[]={"Rent","Groceries","Utilities","Transport","Internet","Insurance","Dining","Fitness","Entertainment","Savings"};for(unsigned i=0;i<10;++i){unsigned r=i+1;auto n=std::to_string(r+1);put(r,0,std::string(labels[i]));put(r,1,150.0+(10-i)*95);put(r,2,125.0+(10-i)*92+(i%3)*40);put(r,3,FormulaInput{"=B"+n+"-C"+n});}}
    else if(choice==3){workbook_name_="Inventory planning (sample)";headers={"Product","In stock","Reorder level","Unit cost","Stock value"};const char* labels[]={"Notebook","Pen set","Desk lamp","Cable kit","USB hub","Mouse pad","Monitor stand","Keyboard","Storage box","Whiteboard","Headset","Webcam"};for(unsigned i=0;i<12;++i){unsigned r=i+1;auto n=std::to_string(r+1);put(r,0,std::string(labels[i]));put(r,1,double(12+(i*17)%60));put(r,2,25.0);put(r,3,8.0+i*7.5);put(r,4,FormulaInput{"=B"+n+"*D"+n});}}
    else{workbook_name_="Weekly project hours (sample)";headers={"Week","Design","Development","Testing","Total hours"};for(unsigned i=0;i<12;++i){unsigned r=i+1;auto n=std::to_string(r+1);put(r,0,std::string("Week ")+std::to_string(r));put(r,1,8.0+i%4);put(r,2,16.0+i%6);put(r,3,4.0+i%5);put(r,4,FormulaInput{"=SUM(B"+n+":D"+n+")"});}}
    for(unsigned c=0;c<headers.size();++c)put(0,c,headers[c]);
    Style heading;heading.bold=true;heading.background=0xE2F1EBFF;heading.foreground=0x155E4BFF;b.rows.push_back({0,heading});
    auto result=sheet_.apply(b);if(!result.accepted)throw std::runtime_error(result.error->context);
    sheet_.clear_journal();populated_=sheet_.populated_cells();modified_=true;select({1,1});viewport_.first_row=0;viewport_.first_column=0;message_="Fictional sample data. Edit and save as your own workbook.";
}


void GridUI::rebuild_filter() {
    viewport_.filtered_rows={0};for(const auto& [r,row]:sheet_.populated_rows())if(r&&display(sheet_.read({r,filter_column_})).find(filter_text_.data())!=std::string::npos)viewport_.filtered_rows.push_back(r);
    viewport_.first_row=0;filter_revision_=sheet_.revision();viewport_.clamp();
}
void GridUI::apply_drag_fill() {
    if(viewport_.filtered)throw std::runtime_error("Clear the filter before dragging to fill.");
    unsigned top=fill_first_.row,left=fill_first_.column,bottom=std::max(fill_last_.row,fill_target_.row),right=std::max(fill_last_.column,fill_target_.column);
    if(std::uint64_t(bottom-top+1)*(right-left+1)>10000)throw std::runtime_error("Fill at most 10,000 cells.");
    bool down=fill_target_.row>fill_last_.row;if(down)right=fill_last_.column;else bottom=fill_last_.row;
    Batch batch;
    for(unsigned r=top;r<=bottom;++r)for(unsigned c=left;c<=right;++c){
        if(r<=fill_last_.row&&c<=fill_last_.column)continue;
        CellCoord source{top+(r-top)%(fill_last_.row-top+1),left+(c-left)%(fill_last_.column-left+1)};
        auto cell=sheet_.cell(source);Input input=cell?cell->input:Input{};
        if(auto formula=std::get_if<FormulaInput>(&input))formula->source=adjust_references(formula->source,int(r)-int(source.row),int(c)-int(source.column));
        else if((down&&fill_last_.row==top+1)||(!down&&fill_last_.column==left+1)){
            auto a=sheet_.read({top,down?c:left}),b=sheet_.read({down?top+1:r,down?c:left+1});
            if(auto x=std::get_if<double>(&a))if(auto y=std::get_if<double>(&b))input=*x+(*y-*x)*double(down?r-top:c-left);
        }
        batch.cells.push_back({{r,c},input});batch.formats.push_back({{r,c},sheet_.cell_style(source)});
    }
    auto result=sheet_.apply(batch);if(!result.accepted)throw std::runtime_error(result.error->context);modified_=true;message_="Filled selection. Relative references adjusted; two-number seeds extend a series.";
}
void GridUI::clipboard_command(Action action) {
    if(viewport_.filtered)throw std::runtime_error("Clear the filter before clipboard operations to avoid changing hidden rows.");
    if(!row_selection_.empty()) throw std::runtime_error("Select a rectangular cell range for clipboard operations.");
    if(action==Action::Copy||action==Action::Cut) {
        const unsigned top=std::min(active_.row,anchor_.row),left=std::min(active_.column,anchor_.column);
        const unsigned rows=std::max(active_.row,anchor_.row)-top+1,columns=std::max(active_.column,anchor_.column)-left+1;
        if(std::uint64_t(rows)*columns>10000)throw std::runtime_error("Copy at most 10,000 cells.");
        std::string text;std::vector<Edit> cells;std::vector<CellStyleEdit> styles;std::vector<Value> values;
        for(unsigned r=0;r<rows;++r) {if(r)text+='\n';for(unsigned c=0;c<columns;++c) {
            if(c)text+='\t';auto cell=sheet_.cell({top+r,left+c});Input input=cell?cell->input:Input{};
            auto value=sheet_.read({top+r,left+c});text+=quote_clipboard(display(value));
            cells.push_back({{r,c},input});styles.push_back({{r,c},sheet_.cell_style({top+r,left+c})});values.push_back(value);
            if(text.size()>8*1024*1024)throw std::runtime_error("Copy exceeds 8 MiB.");
        }}
        ImGui::SetClipboardText(text.c_str());clipboard_text_=std::move(text);clipboard_cells_=std::move(cells);clipboard_styles_=std::move(styles);clipboard_values_=std::move(values);
        clipboard_origin_={top,left};clipboard_rows_=rows;clipboard_columns_=columns;clipboard_revision_=sheet_.revision();clipboard_cut_=action==Action::Cut;
        message_=clipboard_cut_?"Cut prepared. Source cells clear only after a successful paste in this sheet.":"Copied selection.";return;
    }
    const char* raw=ImGui::GetClipboardText();if(!raw)throw std::runtime_error("Clipboard is empty.");
    std::string text(raw);Batch b;
    const bool internal=!clipboard_cells_.empty()&&text==clipboard_text_;
    if(internal) {
        if(std::uint64_t(active_.row)+clipboard_rows_>max_rows||std::uint64_t(active_.column)+clipboard_columns_>max_columns)throw std::runtime_error("Paste would exceed sheet bounds.");
        if(clipboard_cut_&&sheet_.revision()!=clipboard_revision_)throw std::runtime_error("The sheet changed after Cut. Cut the selection again.");
        if(clipboard_cut_)for(auto edit:clipboard_cells_) {edit.coord.row+=clipboard_origin_.row;edit.coord.column+=clipboard_origin_.column;edit.input={};b.cells.push_back(edit);b.formats.push_back({edit.coord,{}});}
        for(std::size_t i=0;i<clipboard_cells_.size();++i) {
            auto edit=clipboard_cells_[i];edit.coord.row+=active_.row;edit.coord.column+=active_.column;
            if(action==Action::PasteValues) std::visit([&](const auto& v) {using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,CellError>)edit.input=display(clipboard_values_[i]);else edit.input=v;},clipboard_values_[i]);
            else if(auto formula=std::get_if<FormulaInput>(&edit.input);formula&&!clipboard_cut_)formula->source=adjust_references(formula->source,int(active_.row)-int(clipboard_origin_.row),int(active_.column)-int(clipboard_origin_.column));
            b.cells.push_back(edit);if(action!=Action::PasteValues)b.formats.push_back({edit.coord,clipboard_styles_[i].style});
        }
    } else {
        auto rows=parse_clipboard(text);
        for(unsigned r=0;r<rows.size();++r)for(unsigned c=0;c<rows[r].size();++c) {
            if(std::uint64_t(active_.row)+r>=max_rows||std::uint64_t(active_.column)+c>=max_columns)throw std::runtime_error("Paste would exceed sheet bounds.");
            auto input=interpret(rows[r][c]);
            if(std::holds_alternative<FormulaInput>(input)) input=rows[r][c]; // external formulas remain literal until explicitly edited
            b.cells.push_back({{active_.row+r,active_.column+c},std::move(input)});
        }
    }
    auto result=sheet_.apply(b);if(!result.accepted)throw std::runtime_error(result.error->context);
    modified_=true;if(clipboard_cut_){clipboard_cut_=false;clipboard_cells_.clear();}
    message_="Pasted. Ctrl+Z restores the previous cells. External formulas are pasted as text.";
}
void GridUI::apply_cell_format() {
    if(viewport_.filtered)throw std::runtime_error("Clear the filter before formatting a range.");
    const auto top=std::min(active_.row,anchor_.row),bottom=std::max(active_.row,anchor_.row),left=std::min(active_.column,anchor_.column),right=std::max(active_.column,anchor_.column);
    if(std::uint64_t(bottom-top+1)*(right-left+1)>10000||!row_selection_.empty())throw std::runtime_error("Select up to 10,000 individual cells to format.");
    Batch b;for(auto r=top;r<=bottom;++r)for(auto c=left;c<=right;++c)b.formats.push_back({{r,c},format_style_});
    auto result=sheet_.apply(b);if(!result.accepted)throw std::runtime_error(result.error->context);modified_=true;message_="Cell formatting applied.";
}
void GridUI::draw_format_dialog() {
    if(format_open_)ImGui::OpenPopup("Format selected cells");
    if(ImGui::BeginPopupModal("Format selected cells",&format_open_,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Applies only to the selected cells");ImGui::Checkbox("Bold",&format_style_.bold);ImGui::SameLine();ImGui::Checkbox("Borders",&format_style_.border);
        int family=format_style_.font_family;const char* fonts[]{"Sans serif (Segoe UI)","Serif (Georgia)","Monospace (Consolas)"};ImGui::Combo("Font",&family,fonts,3);format_style_.font_family=std::uint8_t(family);
        int size=format_style_.font_size;ImGui::SliderInt("Font size",&size,8,36);format_style_.font_size=std::uint8_t(size);
        int align=format_style_.alignment;const char* alignments[]{"Automatic","Left","Center","Right"};ImGui::Combo("Alignment",&align,alignments,4);format_style_.alignment=std::uint8_t(align);
        int format=format_style_.number_format;const char* formats[]{"General","Number","Currency ($1,234.00)","Percentage","Date (Excel serial)","Comma (1,234.00)"};ImGui::Combo("Number format",&format,formats,6);format_style_.number_format=std::uint8_t(format);
        int decimals=format_style_.decimals;ImGui::SliderInt("Decimal places",&decimals,0,12);format_style_.decimals=std::uint8_t(decimals);
        auto color=[&](const char* label,std::uint32_t& packed) {float rgb[]{float(packed>>24)/255,float((packed>>16)&255)/255,float((packed>>8)&255)/255};if(ImGui::ColorEdit3(label,rgb))packed=(std::uint32_t(rgb[0]*255)<<24)|(std::uint32_t(rgb[1]*255)<<16)|(std::uint32_t(rgb[2]*255)<<8)|255;};
        color("Text color",format_style_.foreground);color("Fill color",format_style_.background);
        if(ImGui::Button("Apply")){action_=Action::CellFormat;format_open_=false;ImGui::CloseCurrentPopup();}ImGui::SameLine();
        if(ImGui::Button("Cancel")){format_open_=false;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
    }
}
void GridUI::update_selection_stats() {
    std::size_t count=0,numbers=0; double sum=0;
    const auto top=std::min(active_.row,anchor_.row), bottom=std::max(active_.row,anchor_.row);
    const auto left=std::min(active_.column,anchor_.column), right=std::max(active_.column,anchor_.column);
    for(const auto& [r,row]:sheet_.populated_rows()) {
        bool selected=r>=top&&r<=bottom;
        if(!row_selection_.empty()) { selected=false; for(auto interval:row_selection_) if(r>=interval.first&&r<=interval.last) selected=true; }
        if(!selected) continue;
        for(const auto& [c,cell]:row) if(!row_selection_.empty()||(c>=left&&c<=right)) {
            ++count; if(auto n=std::get_if<double>(&cell.cached)) { sum+=*n; ++numbers; }
        }
    }
    if(numbers) std::snprintf(selection_stats_.data(),selection_stats_.size(),"Count %zu   Sum %.8g   Average %.8g",count,sum,sum/double(numbers));
    else std::snprintf(selection_stats_.data(),selection_stats_.size(),"Count %zu",count);
}
bool GridUI::smoke_workbook_features(const std::filesystem::path& path) {
    sheet_=Sheet({},&lua_);sheets_.clear();sheets_.push_back({"Sheet 1",Sheet({},&lua_),""});current_sheet_=0;viewport_.filtered=false;
    bool ok=sheet_.set({0,0},2.0).accepted&&sheet_.set({1,0},4.0).accepted;
    select({0,0});select({1,0},true);format_style_=Style{};format_style_.border=true;format_style_.number_format=2;apply_cell_format();
    ok&=sheet_.cell_style({1,0}).border&&!sheet_.cell_style({1,1}).border;
    fill_first_={0,0};fill_last_={1,0};fill_target_={4,0};apply_drag_fill();ok&=sheet_.read({4,0})==Value{10.0};
    auto original_clipboard=std::string(ImGui::GetClipboardText()?ImGui::GetClipboardText():"");
    select({0,0});select({1,0},true);clipboard_command(Action::Copy);select({0,2});clipboard_command(Action::Paste);
    ok&=sheet_.read({1,2})==Value{4.0}&&sheet_.cell_style({1,2}).border;
    ok&=sheet_.set({0,3},FormulaInput{"=A1*3"}).accepted;select({0,3});clipboard_command(Action::Copy);select({0,4});clipboard_command(Action::PasteValues);ok&=sheet_.read({0,4})==Value{6.0}&&!std::holds_alternative<FormulaInput>(sheet_.cell({0,4})->input);
    select({0,2});clipboard_command(Action::Cut);select({2,2});clipboard_command(Action::Paste);ok&=!sheet_.cell({0,2})&&sheet_.read({2,2})==Value{2.0};ok&=sheet_.undo();ok&=sheet_.read({0,2})==Value{2.0};ImGui::SetClipboardText(original_clipboard.c_str());
    sheet_action(0);ok&=current_sheet_==1;ok&=sheet_.set({0,0},FormulaInput{"=SHEET(\"Sheet 1\",\"A1\")*5"}).accepted;ok&=sheet_.read({0,0})==Value{10.0};
    change_sheet(0);ok&=sheet_.set({0,0},3.0).accepted;change_sheet(1);ok&=sheet_.read({0,0})==Value{15.0};
    change_sheet(0);std::snprintf(sheet_name_.data(),sheet_name_.size(),"Budget");sheet_action(3);change_sheet(1);ok&=sheet_.read({0,0})==Value{15.0};
    ok&=save_document_to(path);GridUI reopened;ok&=reopened.open_from(path)&&reopened.sheets_.size()==2;reopened.change_sheet(1);ok&=reopened.sheet_.read({0,0})==Value{15.0};
    change_sheet(0);filter_column_=0;std::snprintf(filter_text_.data(),filter_text_.size(),"4");viewport_.filtered=true;rebuild_filter();ok&=viewport_.filtered_rows==std::vector<unsigned>{0,1};viewport_.filtered=false;
    select({0,0});select({4,0},true);prepare_report();ok&=chart_values_.size()==5&&report_rows_.size()==5;report_open_=false;
#ifdef _WIN32
    auto pdf=path;pdf.replace_extension(".pdf");print_report(pdf);ok&=std::filesystem::exists(pdf)&&std::filesystem::file_size(pdf)>100;
#endif
    select({8,0});std::snprintf(editor_.data(),editor_.size(),"77");editor_dirty_=true;ok&=save_document_to(path);GridUI recovered;ok&=recovered.open_from(path)&&recovered.sheet_.read({8,0})==Value{77.0};editor_dirty_=false;
    return ok;
}
bool GridUI::smoke_selection_tools() {
    sheet_=Sheet({},&lua_); (void)sheet_.set({0,0},2.0); (void)sheet_.set({1,0},4.0);
    select({0,0}); select({1,0},true); selection_command(Action::QuickSum);
    bool ok=sheet_.read({2,0})==Value{6.0};
    selection_command(Action::QuickAverage); ok&=sheet_.read({2,0})==Value{6.0}; // preserve occupied destination
    (void)sheet_.undo(); selection_command(Action::QuickAverage); ok&=sheet_.read({2,0})==Value{3.0};
    (void)sheet_.undo(); selection_command(Action::FillDown); ok&=sheet_.read({1,0})==Value{2.0};
    (void)sheet_.undo(); ok&=sheet_.read({1,0})==Value{4.0};
    select({0,0}); select({0,2},true); selection_command(Action::FillRight);
    ok&=sheet_.read({0,2})==Value{2.0}; (void)sheet_.undo();
    (void)sheet_.set({4,2},std::string("needle")); std::snprintf(find_text_.data(),find_text_.size(),"needle");
    find_next(); ok&=active_==CellCoord{4,2}; find_next(); ok&=active_==CellCoord{4,2};
    select_used(); ok&=active_==CellCoord{4,2}&&anchor_==CellCoord{0,0};
    update_selection_stats(); ok&=std::string(selection_stats_.data()).find("Sum 6")!=std::string::npos;
    return ok;
}
void GridUI::select_used() {
    CellCoord last{};
    for(const auto& [r,row]:sheet_.populated_rows()) if(!row.empty()) {
        last.row=std::max(last.row,r); last.column=std::max(last.column,row.rbegin()->first);
    }
    row_selection_.clear(); select({0,0}); select(last,true); viewport_.reveal(active_);
}
void GridUI::find_next() {
    if(!find_text_[0]) return;
    std::optional<CellCoord> first, next;
    for(const auto& [r,row]:sheet_.populated_rows()) for(const auto& [c,cell]:row) {
        auto text=display(cell.cached);
        if(auto formula=std::get_if<FormulaInput>(&cell.input)) text+=" " + formula->source;
        if(text.find(find_text_.data())!=std::string::npos) {
            CellCoord coord{r,c}; if(!first) first=coord; if(!next&&coord>active_) next=coord;
        }
    }
    if(!next) next=first;
    if(next) { row_selection_.clear(); jump(*next); message_="Match found (search wraps at the end)."; }
    else message_="No matching cell. Search is case-sensitive.";
}
void GridUI::selection_command(Action action) {
    if(viewport_.filtered)throw std::runtime_error("Clear the filter before filling or totaling a range.");
    const auto top=std::min(active_.row,anchor_.row), bottom=std::max(active_.row,anchor_.row);
    const auto left=std::min(active_.column,anchor_.column), right=std::max(active_.column,anchor_.column);
    if(!row_selection_.empty()) { message_="Select a rectangular cell range for this command."; return; }
    Batch batch;
    if(action==Action::QuickSum||action==Action::QuickAverage) {
        if(bottom+1>=max_rows) { message_="No room below this selection for a total."; return; }
        for(auto c=left;c<=right;++c) {
            if(sheet_.cell({bottom+1,c})) { message_="The row below must be empty; existing cells were preserved."; return; }
            auto start=std::get<std::string>(to_a1({top,c})), end=std::get<std::string>(to_a1({bottom,c}));
            batch.cells.push_back({{bottom+1,c},FormulaInput{std::string(action==Action::QuickSum?"=SUM(":"=AVERAGE(")+start+":"+end+")"}});
        }
    } else {
        if(std::uint64_t(bottom-top+1)*(right-left+1)>10000) { message_="Fill at most 10,000 cells at a time."; return; }
        for(auto r=top;r<=bottom;++r) for(auto c=left;c<=right;++c) {
            if((action==Action::FillDown&&r==top)||(action==Action::FillRight&&c==left)) continue;
            auto source=sheet_.cell(action==Action::FillDown?CellCoord{top,c}:CellCoord{r,left});
            Input input=source?source->input:Input{};
            if(auto formula=std::get_if<FormulaInput>(&input))formula->source=adjust_references(formula->source,action==Action::FillDown?int(r-top):0,action==Action::FillRight?int(c-left):0);
            batch.cells.push_back({{r,c},std::move(input)});
        }
    }
    if(batch.cells.empty()) { message_="Select more than one cell to fill."; return; }
    auto result=sheet_.apply(batch); modified_|=result.accepted;
    message_=result.accepted?"Selection updated. Ctrl+Z undoes this operation.":result.error->context;
}
void GridUI::draw_tools() {
    draw_report();
    draw_sheets();
    draw_format_dialog();
    draw_safety_dialogs();
    if(filter_open_)ImGui::OpenPopup("Filter rows");
    if(ImGui::BeginPopupModal("Filter rows",&filter_open_,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Show populated rows whose active-column value contains:");ImGui::InputText("Contains (case-sensitive)",filter_text_.data(),filter_text_.size());
        ImGui::TextUnformatted("Row 1 stays visible as the header. Filtering does not delete data.");
        if(ImGui::Button("Apply filter")){viewport_.filtered=true;rebuild_filter();filter_open_=false;ImGui::CloseCurrentPopup();}ImGui::SameLine();if(ImGui::Button("Cancel")){filter_open_=false;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
    }
    auto commands=[&] {
        if(ImGui::MenuItem("Copy","Ctrl+C")) action_=Action::Copy;
        if(ImGui::MenuItem("Cut","Ctrl+X")) action_=Action::Cut;
        if(ImGui::MenuItem("Paste","Ctrl+V")) action_=Action::Paste;
        if(ImGui::MenuItem("Paste values","Ctrl+Shift+V")) action_=Action::PasteValues;
        ImGui::Separator();
        if(ImGui::MenuItem("Undo","Ctrl+Z")) action_=Action::Undo;
        if(ImGui::MenuItem("Redo","Ctrl+Y")) action_=Action::Redo;
        ImGui::Separator();
        if(ImGui::MenuItem("Find...","Ctrl+F")) find_open_=true;
        if(ImGui::MenuItem("Select used range")) select_used();
        if(ImGui::MenuItem("Cell history...")) { history_cell_=active_; history_open_=true; }
        if(ImGui::MenuItem("Change log...")) changelog_open_=true;
        if(ImGui::MenuItem("Check sheet...")) show_health(true);
        if(ImGui::MenuItem("Clear contents","Delete")) action_=Action::Clear;
    };
    if(ImGui::BeginMenuBar()) {
        if(ImGui::BeginMenu("Edit")) { commands(); ImGui::EndMenu(); }
        if(ImGui::BeginMenu("Data")) {
            if(ImGui::MenuItem("Filter active column...")){filter_column_=active_.column;filter_open_=true;}
            if(ImGui::MenuItem("Clear filter",nullptr,false,viewport_.filtered)){viewport_.filtered=false;viewport_.filtered_rows.clear();viewport_.first_row=0;}
            ImGui::Separator();
            if(ImGui::MenuItem("Sort selection ascending"))action_=Action::SortAscending;
            if(ImGui::MenuItem("Sort selection descending"))action_=Action::SortDescending;
            ImGui::TextDisabled("Active column is the key; exclude your header.");
            ImGui::Separator();
            if(ImGui::MenuItem("Insert row above"))action_=Action::InsertRow;
            if(ImGui::MenuItem("Delete active row"))action_=Action::DeleteRow;
            if(ImGui::MenuItem("Insert column before"))action_=Action::InsertColumn;
            if(ImGui::MenuItem("Delete active column"))action_=Action::DeleteColumn;
            ImGui::Separator();
            if(ImGui::MenuItem("Fill down")) action_=Action::FillDown;
            if(ImGui::MenuItem("Fill right")) action_=Action::FillRight;
            ImGui::TextDisabled("Relative formula references adjust; $ references stay fixed.");
            ImGui::Separator();
            if(ImGui::MenuItem("Sum below selection")) action_=Action::QuickSum;
            if(ImGui::MenuItem("Average below selection")) action_=Action::QuickAverage;
            ImGui::Separator();
            if(ImGui::MenuItem("Import CSV...")) action_=Action::ImportCsv;
            if(ImGui::MenuItem("Export CSV...")) action_=Action::ExportCsv;
            if(ImGui::MenuItem("Import Excel values...")) { xlsx_formulas_=false; action_=Action::ImportXlsx; }
            if(ImGui::MenuItem("Export Excel...")) action_=Action::ExportXlsx;
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Format")) {
            if(ImGui::MenuItem("Format selected cells...")) {format_style_=sheet_.cell_style(active_);format_open_=true;}
            ImGui::Separator();
            ImGui::TextDisabled("Applies to the selected rows");
            if(ImGui::MenuItem("Bold")) action_=Action::Bold;
            if(ImGui::MenuItem("Green fill")) action_=Action::Tint;
            if(ImGui::MenuItem("More decimal places")) action_=Action::DecimalsMore;
            if(ImGui::MenuItem("Fewer decimal places")) action_=Action::DecimalsLess;
            if(ImGui::MenuItem("Reset row formatting")) action_=Action::ResetStyle;
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Reports")) {
            if(ImGui::MenuItem("Chart / print selected range...")){try{prepare_report();}catch(const std::exception& e){message_=e.what();}}
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Workbook")) {
            if(ImGui::MenuItem("Manage sheets..."))sheets_open_=true;
            if(ImGui::MenuItem("Add worksheet"))action_=Action::AddSheet;
            ImGui::MenuItem("Autosave recovery every minute",nullptr,&autosave_);
            if(ImGui::MenuItem("Open recovery..."))recovery_open_=true;
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Freeze first row",nullptr,&viewport_.freeze_row);
            ImGui::MenuItem("Freeze first column",nullptr,&viewport_.freeze_column);
            ImGui::MenuItem("Gridlines",nullptr,&gridlines_);
            ImGui::MenuItem("Show formulas",nullptr,&show_formulas_);
            ImGui::MenuItem("Let long text spill into empty cells",nullptr,&text_overflow_);
            if(ImGui::MenuItem("Dark appearance",nullptr,dark_)) set_dark(!dark_,remember_theme_);
            if(ImGui::MenuItem("Reset zoom to 100%")) { zoom_=1; set_scale(scale_); }
            if(ImGui::MenuItem("Go to first cell")) jump({0,0});
            if(ImGui::MenuItem("Select used range")) select_used();
            ImGui::EndMenu();
        }
        // Quick access toolbar and window controls, as in Office title bars.
        const float bar_top=ImGui::GetWindowPos().y, bar_height=ImGui::GetFrameHeight();
        float x=ImGui::GetCursorScreenPos().x+12*scale_;
        auto quick=[&](const char* id,Glyph icon,const char* tip,Action action) {
            ImGui::SetCursorScreenPos({x,bar_top+(bar_height-28*scale_)/2});
            if(icon_button(id,icon,scale_,tip)) action_=action;
            x+=32*scale_;
        };
        quick("quick-save",Glyph::Save,"Save (Ctrl+S)",Action::Save);
        quick("quick-undo",Glyph::Undo,"Undo (Ctrl+Z)",Action::Undo);
        quick("quick-redo",Glyph::Redo,"Redo (Ctrl+Y)",Action::Redo);
        caption_items_.push_back({ImGui::GetWindowPos().x,bar_top,x,bar_top+bar_height});
        draw_window_buttons();
        ImGui::EndMenuBar();
    }
    if(ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_F)) find_open_=true;
    if(find_open_) ImGui::OpenPopup("Find in worksheet");
    if(ImGui::BeginPopupModal("Find in worksheet",&find_open_,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Search values and formulas (case-sensitive)");
        ImGui::SetNextItemWidth(350*scale_);
        if(ImGui::InputText("Find",find_text_.data(),find_text_.size(),ImGuiInputTextFlags_EnterReturnsTrue)) find_next();
        if(ImGui::Button("Find next")) find_next(); ImGui::SameLine();
        if(ImGui::Button("Close")) { find_open_=false; ImGui::CloseCurrentPopup(); }
        ImGui::TextUnformatted(message_.c_str()); ImGui::EndPopup();
    }
}
void GridUI::prepare() {
    const auto instance_before=sheet_.instance(); const auto serial_before=sheet_.journal_serial();
    try {
        if(pending_) { auto job=std::move(pending_); pending_=nullptr; job(); }
        if(viewport_.filtered&&filter_revision_!=sheet_.revision())rebuild_filter();
        if(switch_sheet_>=0) {change_sheet(std::size_t(switch_sheet_));switch_sheet_=-1;}
        recovery_tick();
        ai_poll();
        if(editor_dirty_&&action_!=Action::None&&action_!=Action::New&&action_!=Action::Open&&action_!=Action::Demo)queue_edit();
        if(!queued_.cells.empty()) {
            sheet_.label_next_change(queued_.cells.size()==1?"Edit cell":"Edit cells");
            const auto result=sheet_.apply(queued_);
            message_=result.accepted?"Changes applied.":result.error->context;
            modified_|=result.accepted; queued_={}; selection_changed_=true;
        }
        switch(action_) {
        case Action::Paste:case Action::Cut: sheet_.label_next_change("Paste"); break;
        case Action::PasteValues: sheet_.label_next_change("Paste values"); break;
        case Action::Clear: sheet_.label_next_change("Clear contents"); break;
        case Action::FillDown: sheet_.label_next_change("Fill down"); break;
        case Action::FillRight: sheet_.label_next_change("Fill right"); break;
        case Action::DragFill: sheet_.label_next_change("Fill"); break;
        case Action::QuickSum: sheet_.label_next_change("AutoSum"); break;
        case Action::QuickAverage: sheet_.label_next_change("Average"); break;
        case Action::Bold:case Action::Tint:case Action::ResetStyle:case Action::DecimalsLess:case Action::DecimalsMore: sheet_.label_next_change("Row formatting"); break;
        case Action::CellFormat: sheet_.label_next_change("Format cells"); break;
        case Action::Macro: sheet_.label_next_change("Lua macro"); break;
        case Action::AiApply: sheet_.label_next_change("AI proposal"); break;
        case Action::InsertRow: sheet_.label_next_change("Insert row"); break;
        case Action::InsertColumn: sheet_.label_next_change("Insert column"); break;
        case Action::DeleteRow: case Action::DeleteColumn: {
            const bool rows=action_==Action::DeleteRow; const auto index=rows?active_.row:active_.column; std::size_t filled=0;
            for(const auto& [r,row]:sheet_.populated_rows()) { if(rows) { if(r==index) filled+=row.size(); } else if(row.contains(index)) ++filled; }
            sheet_.label_next_change(rows?"Delete row":"Delete column");
            if(filled) { char note[160]; std::snprintf(note,sizeof note,"%s %s held %zu filled cell%s, which are now removed. Formulas that used them may change.",rows?"Row":"Column",rows?std::to_string(index+1).c_str():std::get<std::string>(to_a1({0,index})).substr(0,std::get<std::string>(to_a1({0,index})).size()-1).c_str(),filled,filled==1?"":"s"); review_note_=note; }
            break;
        }
        case Action::SortAscending:case Action::SortDescending: {
            sheet_.label_next_change("Sort");
            // Sorting only part of a table silently mismatches rows: warn when adjacent columns hold data in the same rows.
            const auto top=std::min(active_.row,anchor_.row),bottom=std::max(active_.row,anchor_.row);
            const auto left=std::min(active_.column,anchor_.column),right=std::max(active_.column,anchor_.column);
            std::size_t beside=0;
            for(auto it=sheet_.populated_rows().lower_bound(top);it!=sheet_.populated_rows().end()&&it->first<=bottom;++it)
                if((left>0&&it->second.contains(left-1))||it->second.contains(right+1)) ++beside;
            if(beside*2>=std::size_t(bottom-top+1)) review_note_="Columns next to the sorted range hold data in the same rows but were not sorted with it. If they belong together, rows are now mismatched: undo and select the whole table before sorting.";
            break;
        }
        default: break;
        }
        switch(action_) {
        case Action::Copy:case Action::Cut:case Action::Paste:case Action::PasteValues:clipboard_command(action_);break;
        case Action::AddSheet:sheet_action(0);break;
        case Action::DuplicateSheet:sheet_action(1);break;
        case Action::DeleteSheet:sheet_action(2);break;
        case Action::RenameSheet:sheet_action(3);break;
        case Action::MoveSheetLeft:sheet_action(4);break;
        case Action::MoveSheetRight:sheet_action(5);break;
        case Action::PrintReport:print_report();break;
        case Action::ExportReport:if(auto path=workbook_dialog(true,{},native_window,L"html")){export_report(*path);message_="Report saved. Open the HTML file in your browser; Print / Save as PDF includes the chart.";}break;
        case Action::DragFill:apply_drag_fill();break;
        case Action::CellFormat:apply_cell_format();break;
        case Action::InsertRow:case Action::DeleteRow:case Action::InsertColumn:case Action::DeleteColumn: {
            if(viewport_.filtered)throw std::runtime_error("Clear the filter before structural edits.");
            for(std::size_t i=0;i<sheets_.size();++i){const auto& source=i==current_sheet_?sheet_:sheets_[i].sheet;for(const auto& [r,row]:source.populated_rows())for(const auto& [c,cell]:row)if(cell.formula)for(const auto& node:cell.formula->nodes)if(node.kind==NodeKind::Call&&(node.op=="SHEET"||node.op=="LUA"))throw std::runtime_error("Remove dynamic SHEET/LUA references before structural edits; their text addresses cannot be shifted safely.");}
            const bool rows=action_==Action::InsertRow||action_==Action::DeleteRow;
            auto b=structural_edit(sheet_,rows,rows?active_.row:active_.column,action_==Action::DeleteRow||action_==Action::DeleteColumn);
            auto result=sheet_.apply(b);if(!result.accepted)throw std::runtime_error(result.error->context);modified_=true;message_="Structure updated. Ctrl+Z undoes this change.";break;
        }
        case Action::SortAscending:case Action::SortDescending: {
            if(viewport_.filtered)throw std::runtime_error("Clear the filter before sorting.");
            auto b=sort_range(sheet_,{std::min(active_.row,anchor_.row),std::min(active_.column,anchor_.column)},{std::max(active_.row,anchor_.row),std::max(active_.column,anchor_.column)},active_.column,action_==Action::SortDescending);
            auto result=sheet_.apply(b);if(!result.accepted)throw std::runtime_error(result.error->context);modified_=true;message_="Sorted selected rows using the active column. Ctrl+Z to undo.";break;
        }
        case Action::Undo: if(sheet_.undo()) { modified_=true; message_="Undo complete."; } break;
        case Action::Redo: if(sheet_.redo()) { modified_=true; message_="Redo complete."; } break;
        case Action::Clear:clear_selection(); break;
        case Action::FillDown:case Action::FillRight:case Action::QuickSum:case Action::QuickAverage:selection_command(action_);break;
        case Action::DecimalsLess:case Action::DecimalsMore:case Action::Bold:case Action::Tint:case Action::ResetStyle:style_selection(action_); break;
        case Action::Macro: {
            auto result=lua_.run_macro(script_.data(),sheet_);
            message_=result.error?result.error->context:"Macro complete. "+std::to_string(result.writes)+" writes applied in one transaction.";
            modified_|=!result.error&&result.writes>0; break;
        }
        case Action::ImportXlsx:xlsx_file(false);break;
        case Action::ExportXlsx:xlsx_file(true);break;
        case Action::ImportCsv:csv_file(false);break;
        case Action::ExportCsv:csv_file(true);break;
        case Action::AiSend:ai_send();break;
        case Action::AiApply:ai_apply();break;
        case Action::Save:save();break;
        case Action::SaveAs:save(true);break;
        case Action::Open:open();break;
        case Action::New:if(!confirm_close()) break; sheets_.clear();sheets_.push_back({"Sheet 1",Sheet({},&lua_),""});current_sheet_=0;script_[0]=0; file_path_.clear(); workbook_name_="Untitled workbook"; file_label_.clear(); editor_dirty_=false; sheet_=Sheet({},&lua_); select({0,0}); row_selection_.clear(); modified_=false; message_="New workbook. Press Ctrl+S to save locally."; break;
        case Action::Demo:if(confirm_close()) load_template(template_choice_); break;
        default:break;
        }
        if(action_!=Action::None) { if(action_!=Action::Save&&action_!=Action::SaveAs&&action_!=Action::Open&&action_!=Action::New&&action_!=Action::Demo&&action_!=Action::ImportCsv&&action_!=Action::ExportCsv&&action_!=Action::ImportXlsx&&action_!=Action::ExportXlsx&&action_!=Action::AiSend) selection_changed_=true; action_=Action::None; }
        sheet_.label_next_change({});
        if(sheet_.instance()==instance_before&&sheet_.journal_serial()!=serial_before) evaluate_latest_change();
        review_note_.clear();
        refresh_health(health_force_); health_force_=false;
        if(counted_revision_!=sheet_.revision())recalculate_links();
        if(selection_changed_||counted_revision_!=sheet_.revision()) update_selection_stats();
        if(counted_revision_!=sheet_.revision()) {
            populated_=sheet_.populated_cells(); counted_revision_=sheet_.revision();
        }
        if(selection_changed_) refresh_editor();
    } catch(const std::exception& error) { message_=error.what(); queued_={}; action_=Action::None; sheet_.label_next_change({}); review_note_.clear(); }
}
void GridUI::evaluate_latest_change() {
    const auto& record=sheet_.journal().back();
    if(record.label=="Edit cell"||record.label=="Edit cells") {
        // Instant, non-blocking feedback for a mistyped amount.
        for(std::size_t i=0;i<record.cells.size()&&i<5;++i)
            if(record.cells[i].after_kind==1) if(auto note=outlier_note(sheet_,record.cells[i].coord)) { message_="Check this: "+*note+" Press Ctrl+Z to undo."; return; }
        return;
    }
    static const std::set<std::string,std::less<>> reviewable{"Paste","Paste values","Clear contents","Clear","Fill down","Fill right","Fill","Delete row","Delete column","Sort","Lua macro"};
    if(!review_enabled_||!reviewable.contains(record.label)) return;
    PendingReview review; review.label=record.label; review.total=record.total_cells;
    for(const auto& change:record.cells) if(change.before_kind==4&&change.after_kind!=4) ++review.formulas_replaced;
    const bool rearranges=record.label=="Sort"||record.label=="Delete row"||record.label=="Delete column";
    if(!review_note_.empty()) review.warnings.push_back(review_note_);
    if(review.formulas_replaced&&review.total>1&&!rearranges)
        review.warnings.push_back(std::to_string(review.formulas_replaced)+(review.formulas_replaced==1?" formula was":" formulas were")+" replaced or cleared. Those cells will no longer calculate.");
    std::size_t outliers=0;
    for(std::size_t i=0;i<record.cells.size()&&outliers<3;++i)
        if(record.cells[i].after_kind==1) if(auto note=outlier_note(sheet_,record.cells[i].coord)) { review.warnings.push_back(*note); ++outliers; }
    const bool large=!rearranges&&review.total>=std::uint32_t(review_threshold_);
    if(review.warnings.empty()&&!large) return;
    review.cells=record.cells;
    review_=std::move(review);
}
void GridUI::refresh_health(bool force) {
    if(!force&&health_revision_==sheet_.revision()&&health_instance_==sheet_.instance()) return;
    const auto now=std::chrono::steady_clock::now();
    // Small sheets scan in well under a millisecond; only large ones are throttled while typing.
    if(!force&&sheet_.populated_cells()>5'000&&now-health_time_<std::chrono::milliseconds(350)) return;
    // Very large sheets are only scanned while the panel is open, to keep typing smooth.
    if(!force&&!health_open_&&sheet_.populated_cells()>20'000) { health_.clear(); health_revision_=sheet_.revision(); health_instance_=sheet_.instance(); return; }
    health_=check_sheet_health(sheet_); health_revision_=sheet_.revision(); health_instance_=sheet_.instance(); health_time_=now;
}
std::size_t GridUI::visible_health_count() const {
    std::size_t count=0;
    for(const auto& issue:health_) if(!health_ignored_.contains({int(issue.kind),issue.cell})) ++count;
    return count;
}
namespace {
std::string local_time(std::int64_t seconds) {
    const std::time_t time=seconds; std::tm parts{};
#ifdef _WIN32
    localtime_s(&parts,&time);
#else
    localtime_r(&time,&parts);
#endif
    char buffer[32]; std::strftime(buffer,sizeof buffer,"%Y-%m-%d %H:%M",&parts); return buffer;
}
std::string change_text(const std::string& text,std::uint8_t kind) { return kind==0?std::string("(empty)"):text; }
}
void GridUI::draw_health_panel(float height) {
    ImGui::BeginChild("Sheet check",{400*scale_,height},ImGuiChildFlags_Borders);
    const auto accent=dark_?ImVec4{0.40f,0.85f,0.69f,1}:ImVec4{0.03f,0.43f,0.32f,1};
    const ImVec4 serious_color{0.84f,0.25f,0.22f,1}, warning_color{0.85f,0.52f,0.08f,1};
    ImGui::TextUnformatted("SHEET CHECK");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x-50*scale_); if(ImGui::SmallButton("Close")) health_open_=false;
    refresh_health();
    const auto visible=visible_health_count();
    std::size_t serious=0; for(const auto& issue:health_) if(issue.serious&&!health_ignored_.contains({int(issue.kind),issue.cell})) ++serious;
    if(!visible) ImGui::TextColored(accent,"No problems found.");
    else { ImGui::Text("%zu to review",visible); if(serious) { ImGui::SameLine(); ImGui::TextColored(serious_color,"(%zu may give wrong results)",serious); } }
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("Checks run as you edit: formula patterns, totals that skip rows, out-of-scale numbers, numbers stored as text and blank rows inside tables.");
    ImGui::PopTextWrapPos();
    if(!health_ignored_.empty()) { if(ImGui::SmallButton("Show ignored issues again")) health_ignored_.clear(); }
    ImGui::Separator();
    ImGui::BeginChild("Issues",{0,0},0);
    int index=0;
    for(const auto& issue:health_) {
        if(health_ignored_.contains({int(issue.kind),issue.cell})) continue;
        ImGui::PushID(index++);
        const auto p=ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddCircleFilled({p.x+6*scale_,p.y+ImGui::GetFontSize()/2+1},4*scale_,ImGui::ColorConvertFloat4ToU32(issue.serious?serious_color:warning_color));
        ImGui::SetCursorScreenPos({p.x+18*scale_,p.y});
        ImGui::PushTextWrapPos(0); ImGui::TextUnformatted(issue.title.c_str());
        ImGui::TextDisabled("%s",issue.detail.c_str()); ImGui::PopTextWrapPos();
        if(ImGui::SmallButton("Go to")) jump(issue.cell);
        if(!issue.fix_label.empty()) {
            ImGui::SameLine();
            if(ImGui::SmallButton(issue.fix_label.c_str())) {
                pending_=[this,issue] {
                    if(issue.delete_row) { select({issue.row,0}); action_=Action::DeleteRow; return; }
                    sheet_.label_next_change("Sheet check fix");
                    auto result=sheet_.apply(issue.fix); if(!result.accepted) throw std::runtime_error(result.error->context);
                    modified_=true; message_="Fixed: "+issue.title+". Ctrl+Z undoes it.";
                };
            }
        }
        ImGui::SameLine(); if(ImGui::SmallButton("Ignore")) health_ignored_.insert({int(issue.kind),issue.cell});
        ImGui::Separator(); ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::EndChild();
}
void GridUI::draw_safety_dialogs() {
    const ImVec4 warning_color{0.85f,0.52f,0.08f,1};
    auto changes_table=[&](const char* id,const std::vector<CellChange>& cells,float max_height) {
        const float row_height=ImGui::GetTextLineHeightWithSpacing()+4*scale_;
        const float height=std::min(max_height,row_height*float(cells.size()+1)+8*scale_);
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        if(!ImGui::BeginTable(id,3,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_ScrollY,{620*scale_,height})) { ImGui::PopStyleColor(); return; }
        ImGui::TableSetupColumn("Cell",ImGuiTableColumnFlags_WidthFixed,70*scale_);
        ImGui::TableSetupColumn("Before",ImGuiTableColumnFlags_WidthStretch); ImGui::TableSetupColumn("After",ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupScrollFreeze(0,1); ImGui::TableHeadersRow();
        ImGuiListClipper clipper; clipper.Begin(int(cells.size()));
        while(clipper.Step()) for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
            const auto& change=cells[std::size_t(i)]; ImGui::PushID(i); ImGui::TableNextRow();
            ImGui::TableNextColumn(); const auto address=std::get<std::string>(to_a1(change.coord));
            if(ImGui::Selectable(address.c_str())) jump(change.coord);
            ImGui::TableNextColumn(); ImGui::TextDisabled("%s",change_text(change.before,change.before_kind).c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(change_text(change.after,change.after_kind).c_str());
            ImGui::PopID();
        }
        ImGui::EndTable(); ImGui::PopStyleColor();
    };
    // Review of a risky change: the result is already visible in the sheet; Keep or Undo it.
    if(review_) ImGui::OpenPopup("Review this change");
    if(ImGui::BeginPopupModal("Review this change",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        if(!review_) { ImGui::CloseCurrentPopup(); ImGui::EndPopup(); return; }
        const auto& review=*review_;
        ImGui::Text("%s changed %u cell%s.",review.label.c_str(),review.total,review.total==1?"":"s");
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+620*scale_);
        for(const auto& warning:review.warnings) { ImGui::TextColored(warning_color,"!"); ImGui::SameLine(); ImGui::TextUnformatted(warning.c_str()); }
        ImGui::PopTextWrapPos();
        ImGui::Spacing();
        changes_table("review-changes",review.cells,260*scale_);
        if(review.total>review.cells.size()) ImGui::TextDisabled("...and %u more cells not listed.",unsigned(review.total-review.cells.size()));
        ImGui::Spacing();
        const bool keep=ImGui::Button("Keep change",{150*scale_,0})||ImGui::IsKeyPressed(ImGuiKey_Enter);
        ImGui::SameLine();
        const bool undo=ImGui::Button("Undo change",{150*scale_,0})||ImGui::IsKeyPressed(ImGuiKey_Escape);
        ImGui::SameLine(); ImGui::TextDisabled("Change how often this appears on the Review tab.");
        if(keep) { message_=review.label+" kept."; review_.reset(); ImGui::CloseCurrentPopup(); }
        else if(undo) {
            review_.reset(); ImGui::CloseCurrentPopup();
            pending_=[this] { if(sheet_.revert_last_change()) { modified_=true; selection_changed_=true; message_="Change undone. The sheet is back to how it was."; } };
        }
        ImGui::EndPopup();
    }
    // Everything recorded for one cell, newest first.
    if(history_open_) ImGui::OpenPopup("Cell history");
    if(ImGui::BeginPopupModal("Cell history",&history_open_,ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto address=std::get<std::string>(to_a1(history_cell_));
        ImGui::Text("History of %s on %s",address.c_str(),sheets_.empty()?"this sheet":sheets_[current_sheet_].name.c_str());
        std::size_t shown=0;
        ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        if(ImGui::BeginTable("cell-history",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_ScrollY,{680*scale_,300*scale_})) {
            ImGui::TableSetupColumn("When",ImGuiTableColumnFlags_WidthFixed,150*scale_); ImGui::TableSetupColumn("Change",ImGuiTableColumnFlags_WidthFixed,130*scale_);
            ImGui::TableSetupColumn("Before",ImGuiTableColumnFlags_WidthStretch); ImGui::TableSetupColumn("After",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupScrollFreeze(0,1); ImGui::TableHeadersRow();
            const auto& journal=sheet_.journal();
            for(auto record=journal.rbegin();record!=journal.rend();++record) for(const auto& change:record->cells) if(change.coord==history_cell_) {
                ImGui::TableNextRow(); ++shown;
                ImGui::TableNextColumn(); ImGui::TextUnformatted(local_time(record->time).c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(record->label.c_str());
                ImGui::TableNextColumn(); ImGui::TextDisabled("%s",change_text(change.before,change.before_kind).c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(change_text(change.after,change.after_kind).c_str());
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleColor();
        if(!shown) ImGui::TextDisabled("No recorded changes for this cell yet. History starts with this version of Julretsu.");
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+680*scale_);
        ImGui::TextDisabled("History is saved inside the .julretsu file. Very large changes list their first %zu cells.",journal_cells_per_record);
        ImGui::PopTextWrapPos();
        if(ImGui::Button("Close")) { history_open_=false; ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    // Every recorded change on this sheet, newest first.
    if(changelog_open_) ImGui::OpenPopup("Change log");
    if(ImGui::BeginPopupModal("Change log",&changelog_open_,ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto& journal=sheet_.journal();
        ImGui::Text("%zu recorded changes on %s",journal.size(),sheets_.empty()?"this sheet":sheets_[current_sheet_].name.c_str());
        ImGui::BeginChild("log",{700*scale_,360*scale_},ImGuiChildFlags_Borders);
        for(std::size_t i=journal.size();i-->0;) {
            const auto& record=journal[i]; ImGui::PushID(int(i));
            char heading[200]; std::snprintf(heading,sizeof heading,"%s   %s   (%u cell%s%s)",local_time(record.time).c_str(),record.label.c_str(),record.total_cells,record.total_cells==1?"":"s",record.formats?", formatting":"");
            if(ImGui::TreeNode("record","%s",heading)) {
                if(!record.cells.empty()) changes_table("log-changes",record.cells,220*scale_);
                if(record.total_cells>record.cells.size()) ImGui::TextDisabled("...and %u more cells not listed.",unsigned(record.total_cells-record.cells.size()));
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
        if(journal.empty()) ImGui::TextDisabled("No changes recorded yet.");
        ImGui::EndChild();
        if(ImGui::Button("Close")) { changelog_open_=false; ImGui::CloseCurrentPopup(); }
        ImGui::SameLine(); if(ImGui::Button("Clear history...")) clear_history_confirm_=true;
        if(clear_history_confirm_) {
            ImGui::SameLine(); ImGui::TextColored(warning_color,"Delete this sheet's change history?"); ImGui::SameLine();
            if(ImGui::Button("Delete history")) { clear_history_confirm_=false; pending_=[this]{ sheet_.clear_journal(); modified_=true; message_="Change history cleared for this sheet."; }; }
            ImGui::SameLine(); if(ImGui::Button("Keep")) clear_history_confirm_=false;
        }
        ImGui::EndPopup();
    }
}
void GridUI::smoke_open_review() {
    pending_=[this] {
        sheet_.label_next_change("Paste"); Batch b;
        for(std::uint32_t r=40;r<60;++r) for(std::uint32_t c=1;c<4;++c) b.cells.push_back({{r,c},double(r*10+c)});
        (void)sheet_.apply(b);
    };
}
void GridUI::smoke_health_example(bool show) {
    if(show) { pending_=[this]{ (void)sheet_.apply(Batch{{{{5,4},999.0},{{10,4},FormulaInput{"=SUM(E4:E8)"}}},{}}); }; show_health(true); }
    else { show_health(false); pending_=[this]{ (void)sheet_.revert_last_change(); }; }
}
void GridUI::smoke_close_review() { review_.reset(); pending_=[this]{ (void)sheet_.revert_last_change(); }; }
bool GridUI::smoke_safety_net(const std::filesystem::path& path) {
    std::string failed; load_demo(); disable_recovery();
    auto step=[&](const char* name,bool passed){ if(!passed) failed+=std::string(failed.empty()?"":", ")+name; };
    auto run=[&](std::function<void()> job) { pending_=std::move(job); prepare(); };
    refresh_health(true); step("demo has no false alarms",health_.empty()); // the example workbook must not raise false alarms
    // A mistyped amount is flagged immediately, without blocking.
    smoke_queue_edit({3,3},"4500000"); prepare(); step("typed outlier message",message_.find("typical value")!=std::string::npos);
    action_=Action::Undo; prepare(); step("undo typed edit",sheet_.read({3,3})==Value{450.0});
    // A large paste opens a review; Undo change restores the sheet exactly and drops it from history.
    review_threshold_=10; const auto records=sheet_.journal().size();
    smoke_open_review(); prepare(); step("large paste opens review",review_.has_value()&&review_->total==60&&review_->label=="Paste");
    smoke_close_review(); prepare(); step("undo change restores",!review_&&!sheet_.cell({40,1})&&sheet_.journal().size()==records);
    // Overwriting formulas is called out even below the size threshold.
    review_threshold_=50;
    run([this]{ sheet_.label_next_change("Paste"); (void)sheet_.apply(Batch{{{{4,4},1.0},{{5,4},2.0}},{}}); });
    step("formula overwrite warned",review_.has_value()&&review_->formulas_replaced==2&&!review_->warnings.empty());
    review_.reset(); (void)sheet_.revert_last_change();
    // Sorting part of a table warns that neighbouring columns were left behind.
    select({3,2}); select({8,2},true); action_=Action::SortAscending; prepare();
    step("partial sort warned",review_.has_value()&&!review_->warnings.empty()); review_.reset(); (void)sheet_.revert_last_change();
    // Sheet check finds a typed value in a formula column and repairs it with one click.
    run([this]{ (void)sheet_.apply(Batch{{{{5,4},999.0}},{}}); });
    refresh_health(true);
    auto found=std::find_if(health_.begin(),health_.end(),[](const HealthIssue& i){ return i.kind==HealthKind::InconsistentFormula&&i.cell==CellCoord{5,4}; });
    step("inconsistent formula found",found!=health_.end()&&!found->fix.cells.empty());
    if(found!=health_.end()) { const auto issue=*found; run([this,issue]{ sheet_.label_next_change("Sheet check fix"); (void)sheet_.apply(issue.fix); }); }
    step("one-click fix applied",sheet_.cell({5,4})&&std::holds_alternative<FormulaInput>(sheet_.cell({5,4})->input)&&sheet_.read({5,4})==Value{144.0});
    // Every change is in the history with its label, and the history survives saving.
    const auto& journal=sheet_.journal();
    step("history recorded",!journal.empty()&&journal.back().label=="Sheet check fix"&&journal.back().cells.size()==1&&journal.back().cells[0].before=="999");
    step("history saved",save_document_to(path));
    GridUI reopened; reopened.disable_recovery(); step("history reopened",reopened.open_from(path));
    step("history restored",reopened.sheet_.journal().size()==journal.size()&&reopened.sheet_.journal().back().label=="Sheet check fix");
    std::filesystem::remove(path);
    review_threshold_=50; review_.reset(); load_demo();
    if(!failed.empty()) throw std::runtime_error("Safety net checks failed: "+failed);
    return true;
}
void GridUI::draw_script_panel(float height) {
    ImGui::BeginChild("Scripts",{350*scale_,height},ImGuiChildFlags_Borders);
    ImGui::TextUnformatted("LUA WORKSPACE");
    ImGui::Spacing(); ImGui::TextUnformatted("Make repetitive edits simple.");
    ImGui::TextWrapped("Macros use cell('A1') to read and set('A1', value) to queue changes. All edits apply together.");
    ImGui::Separator(); ImGui::Spacing();
    if(ImGui::InputTextMultiline("##macro",script_.data(),script_.size(),{-1,std::max(100.0f,ImGui::GetContentRegionAvail().y-125*scale_)},ImGuiInputTextFlags_AllowTabInput)) modified_=true;
    if(ImGui::Button("Run macro",{150*scale_,32*scale_})) action_=Action::Macro; remember_target(MacroButton);
    ImGui::SameLine(); if(ImGui::Button("Undo macro")) action_=Action::Undo;
    ImGui::Spacing(); ImGui::TextDisabled("Local execution / bounded resources");
    ImGui::TextWrapped("No files, network, or system access. Failed scripts leave the sheet unchanged.");
    ImGui::EndChild();
}
void GridUI::draw_ai_panel(float height) {
    ImGui::BeginChild("AI assistant",{400*scale_,height},ImGuiChildFlags_Borders);
    const auto accent=dark_?ImVec4{0.40f,0.85f,0.69f,1}:ImVec4{0.03f,0.43f,0.32f,1};
    ImGui::TextUnformatted("AI ASSISTANT");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x-50*scale_); if(ImGui::SmallButton("Close")) ai_open_=false;
    ImGui::TextWrapped("Describe a change. Your AI proposes cell edits; nothing changes until you apply them.");
    ImGui::Spacing();
    const bool anthropic=ai_settings_.provider==AiProvider::Anthropic;
    if(ai_connection_open_) { ImGui::SetNextItemOpen(true); ai_connection_open_=false; }
    else if(ai_connection_collapse_) { ImGui::SetNextItemOpen(false); ai_connection_collapse_=false; }
    if(ImGui::CollapsingHeader("Connection",(!ai_key_saved_&&anthropic)?ImGuiTreeNodeFlags_DefaultOpen:0)) {
        int provider=anthropic?0:1; const char* providers[]{"Anthropic (Claude)","OpenAI-compatible"};
        ImGui::SetNextItemWidth(-1);
        if(ImGui::Combo("##provider",&provider,providers,2)) { ai_use_settings(default_ai_settings(provider==0?AiProvider::Anthropic:AiProvider::OpenAICompatible)); save_ai_settings(ai_settings_); }
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled(anthropic?"Uses your own Anthropic API account.":"OpenAI, Ollama, LM Studio and other servers with a /chat/completions endpoint.");
#ifndef _WIN32
        ImGui::TextColored({0.85f,0.52f,0.08f,1},"AI connections currently require the Windows version of Julretsu.");
#endif
        ImGui::PopTextWrapPos();
        ImGui::TextUnformatted("Model"); ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##model","Model name from your provider",ai_model_.data(),ai_model_.size());
        ImGui::TextUnformatted("Endpoint"); ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##endpoint",ai_endpoint_.data(),ai_endpoint_.size());
        if(ImGui::Button("Save connection")) {
            AiSettings settings=ai_settings_; settings.model=ai_model_.data(); settings.endpoint=ai_endpoint_.data();
            auto problem=validate_ai_endpoint(settings.endpoint);
            if(!problem.empty()) ai_status_=problem;
            else if(settings.model.empty()) ai_status_="Enter a model name.";
            else { ai_settings_=settings; ai_status_=save_ai_settings(settings)?"Connection saved.":"Connection used for this session, but it could not be saved."; }
        }
        ImGui::Spacing(); ImGui::TextUnformatted("API key");
        if(ai_key_saved_) {
            ImGui::TextColored(accent,"Saved in Windows Credential Manager");
            if(ImGui::Button("Remove key")) { ai_key_saved_=!delete_ai_key(ai_settings_.provider); ai_status_=ai_key_saved_?"The key could not be removed.":"Key removed from this computer."; }
        } else {
            ImGui::SetNextItemWidth(-90*scale_);
            ImGui::InputTextWithHint("##key",anthropic?"sk-ant-...":"Optional for local servers",ai_key_.data(),ai_key_.size(),ImGuiInputTextFlags_Password);
            ImGui::SameLine();
            if(ImGui::Button("Save key",{-1,0})) {
                if(ai_key_[0]==0) ai_status_="Paste your API key first.";
                else if(store_ai_key(ai_settings_.provider,ai_key_.data())) { ai_key_saved_=true; ai_status_="Key saved securely for your Windows account."; }
                else ai_status_="The key could not be saved.";
                std::fill(ai_key_.begin(),ai_key_.end(),'\0');
            }
        }
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("Keys are kept by Windows for your user account, never in workbooks or settings files. Usage is billed by your provider.");
        ImGui::PopTextWrapPos();
    }
    ImGui::Separator(); ImGui::Spacing();
    const bool busy=ai_session_->busy();
    if(!ai_proposal_) {
    ImGui::TextUnformatted("What should change?");
    ImGui::InputTextMultiline("##ai-prompt",ai_prompt_.data(),ai_prompt_.size(),{-1,86*scale_},busy?ImGuiInputTextFlags_ReadOnly:0);
    const auto host=endpoint_host(ai_endpoint_.data());
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("Sends your request and up to 2,000 filled cells (this sheet has %zu) to %s.",populated_,host.empty()?"your provider":host.c_str());
    ImGui::PopTextWrapPos();
    if(busy) {
        static const char* dots[]{"   ",".  ",".. ","..."};
        ImGui::AlignTextToFramePadding(); ImGui::Text("Waiting for a proposal%s",dots[int(ImGui::GetTime()*3)%4]);
        ImGui::SameLine(); if(ImGui::Button("Cancel")) ai_session_->cancel();
    } else {
        ImGui::BeginDisabled(ai_prompt_[0]==0||ai_proposal_.has_value());
        if(ImGui::Button("Propose edits",{-1,32*scale_})) action_=Action::AiSend;
        ImGui::EndDisabled();
    }
    }
    if(!ai_status_.empty()) { ImGui::PushTextWrapPos(0); ImGui::TextUnformatted(ai_status_.c_str()); ImGui::PopTextWrapPos(); }
    if(ai_proposal_) {
        auto& proposal=*ai_proposal_;
        ImGui::Separator(); ImGui::TextColored(accent,"Proposed changes");
        ImGui::PushTextWrapPos(0);
        if(!proposal.summary.empty()) ImGui::TextUnformatted(proposal.summary.c_str());
        for(const auto& warning:proposal.warnings) ImGui::TextColored({0.85f,0.52f,0.08f,1},"%s",warning.c_str());
        ImGui::PopTextWrapPos();
        const auto selected=std::size_t(std::count_if(proposal.edits.begin(),proposal.edits.end(),[](const AiEdit& e){ return e.selected; }));
        if(!proposal.edits.empty()) {
            ImGui::AlignTextToFramePadding(); ImGui::Text("%zu of %zu edits ticked",selected,proposal.edits.size());
            ImGui::SameLine(); if(ImGui::SmallButton("All")) for(auto& e:proposal.edits) e.selected=true;
            ImGui::SameLine(); if(ImGui::SmallButton("None")) for(auto& e:proposal.edits) e.selected=false;
            auto brief=[](const std::string& text) {
                std::string line=text.substr(0,text.find_first_of("\r\n"));
                if(line.size()>48) { line.resize(48); while(!line.empty()&&(static_cast<unsigned char>(line.back())&0xc0)==0x80) line.pop_back(); if(!line.empty()) line.pop_back(); line+="..."; }
                else if(line.size()<text.size()) line+=" ...";
                return line;
            };
            const float table_height=std::max(120*scale_,ImGui::GetContentRegionAvail().y-78*scale_);
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg,ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
            const bool table=ImGui::BeginTable("ai-edits",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_ScrollY,{-1,table_height});
            if(table) {
                ImGui::TableSetupColumn("",ImGuiTableColumnFlags_WidthFixed,26*scale_);
                ImGui::TableSetupColumn("Cell",ImGuiTableColumnFlags_WidthFixed,64*scale_);
                ImGui::TableSetupColumn("Before",ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("After",ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupScrollFreeze(0,1); ImGui::TableHeadersRow();
                ImGuiListClipper clipper; clipper.Begin(int(proposal.edits.size()));
                while(clipper.Step()) for(int i=clipper.DisplayStart;i<clipper.DisplayEnd;++i) {
                    auto& edit=proposal.edits[std::size_t(i)]; ImGui::PushID(i); ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::Checkbox("##on",&edit.selected);
                    ImGui::TableNextColumn(); if(ImGui::Selectable(edit.address.c_str())) jump(edit.coord);
                    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Go to %s",edit.address.c_str());
                    ImGui::TableNextColumn(); ImGui::TextDisabled("%s",brief(edit.before).c_str());
                    if(ImGui::IsItemHovered()&&edit.before.size()>48) ImGui::SetTooltip("%s",edit.before.c_str());
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(brief(edit.after).c_str());
                    if(ImGui::IsItemHovered()&&(edit.after.size()>48||edit.lua)) ImGui::SetTooltip("%s%s",edit.lua?"Runs a Lua script.\n":"",edit.after.c_str());
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::PopStyleColor();
            ImGui::BeginDisabled(selected==0);
            char apply[48]; std::snprintf(apply,sizeof apply,"Apply %zu edit%s",selected,selected==1?"":"s");
            if(ImGui::Button(apply,{180*scale_,32*scale_})) action_=Action::AiApply;
            ImGui::EndDisabled(); ImGui::SameLine();
        }
        if(ImGui::Button(proposal.edits.empty()?"Dismiss":"Discard",{100*scale_,32*scale_})) { ai_proposal_.reset(); ai_status_="Proposal discarded. Nothing was changed."; }
        ImGui::TextDisabled("Applied together as one step; Ctrl+Z undoes it.");
    }
    ImGui::EndChild();
}
void GridUI::draw_grid(float width,float height) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::BeginChild("Grid",{width,height},ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    const ImVec2 origin=ImGui::GetCursorScreenPos();
    const ImVec2 available=ImGui::GetContentRegionAvail();
    viewport_.resize(available.x,available.y);
    ImGui::InvisibleButton("sheet-canvas",available,ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered=ImGui::IsItemHovered();
    if(ImGui::BeginPopupContextItem("Cell actions")) {
        ImGui::TextDisabled("Current selection");
        if(ImGui::MenuItem("Cell history...")) { history_cell_=active_; history_open_=true; }
        if(ImGui::MenuItem("Clear contents")) action_=Action::Clear;
        if(ImGui::MenuItem("Fill down")) action_=Action::FillDown;
        if(ImGui::MenuItem("Fill right")) action_=Action::FillRight;
        ImGui::Separator();
        if(ImGui::MenuItem("Sum below selection")) action_=Action::QuickSum;
        if(ImGui::MenuItem("Average below selection")) action_=Action::QuickAverage;
        ImGui::Separator();
        if(ImGui::MenuItem("Bold selected rows")) action_=Action::Bold;
        if(ImGui::MenuItem("Reset row formatting")) action_=Action::ResetStyle;
        ImGui::EndPopup();
    }
    const auto& io=ImGui::GetIO();
    const float hx=origin.x+viewport_.row_header+float(viewport_.screen_column(std::max(active_.column,anchor_.column))+1)*viewport_.column_width;
    const float hy=origin.y+viewport_.column_header+float(viewport_.screen_row(std::max(active_.row,anchor_.row))+1)*viewport_.row_height;
    const bool handle=hovered&&std::abs(io.MousePos.x-hx)<7*scale_&&std::abs(io.MousePos.y-hy)<7*scale_;
    if(handle&&ImGui::IsMouseClicked(0)&&!editing_){fill_drag_=true;fill_first_={std::min(active_.row,anchor_.row),std::min(active_.column,anchor_.column)};fill_last_={std::max(active_.row,anchor_.row),std::max(active_.column,anchor_.column)};fill_target_=fill_last_;}
    if(fill_drag_){fill_target_=viewport_.hit(io.MousePos.x-origin.x,io.MousePos.y-origin.y);if(ImGui::IsMouseReleased(0)){fill_drag_=false;action_=Action::DragFill;}}
    if(hovered&&!io.WantTextInput&&!fill_drag_&&!handle) {
        if(io.KeyShift) viewport_.first_column-=int(io.MouseWheel*3);
        else viewport_.first_row-=int(io.MouseWheel*3);
        viewport_.first_column-=int(io.MouseWheelH*3); viewport_.clamp();
        const float x=io.MousePos.x-origin.x,y=io.MousePos.y-origin.y;
        if(ImGui::IsMouseClicked(0)&&y>=viewport_.column_header) {
            grid_focused_=true;
            auto c=viewport_.hit(x,y);
            if(x<viewport_.row_header) {
                if(!io.KeyCtrl&&!io.KeyShift) row_selection_.clear();
                if(io.KeyShift) row_selection_.push_back({std::min(anchor_.row,c.row),std::max(anchor_.row,c.row)});
                else {
                    bool removed=false;
                    if(io.KeyCtrl) {
                        std::vector<RowInterval> updated;
                        for(auto interval:row_selection_) {
                            if(c.row<interval.first||c.row>interval.last) updated.push_back(interval);
                            else {
                                removed=true;
                                if(interval.first<c.row) updated.push_back({interval.first,c.row-1});
                                if(c.row<interval.last) updated.push_back({c.row+1,interval.last});
                            }
                        }
                        row_selection_=std::move(updated);
                    }
                    if(!removed) row_selection_.push_back({c.row,c.row});
                }
                row_selection_=RowSelection(row_selection_).intervals();
                select(c,io.KeyShift);
            } else {
                row_selection_.clear(); select(c,io.KeyShift);
                if(ImGui::IsMouseDoubleClicked(0)) { editing_=true; focus_editor_=true; }
            }
        }
        if(ImGui::IsMouseDragging(0)&&x>=viewport_.row_header&&y>=viewport_.column_header&&!editing_) {
            active_=viewport_.hit(x,y); selection_changed_=true;
        }
    }
    if(grid_focused_&&!io.WantTextInput&&!ImGui::IsAnyItemActive()&&
       !ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)) {
        auto next=active_; bool moved=false;
        if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)) { next.row=std::min(next.row+1,max_rows-1); moved=true; }
        if(ImGui::IsKeyPressed(ImGuiKey_UpArrow)) { next.row=next.row?next.row-1:0; moved=true; }
        if(ImGui::IsKeyPressed(ImGuiKey_RightArrow)||ImGui::IsKeyPressed(ImGuiKey_Tab)) { next.column=std::min(next.column+1,max_columns-1); moved=true; }
        if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) { next.column=next.column?next.column-1:0; moved=true; }
        if(ImGui::IsKeyPressed(ImGuiKey_PageDown)) { next.row=std::min(next.row+unsigned(viewport_.visible_rows),max_rows-1); moved=true; }
        if(ImGui::IsKeyPressed(ImGuiKey_PageUp)) { next.row=next.row>unsigned(viewport_.visible_rows)?next.row-unsigned(viewport_.visible_rows):0; moved=true; }
        if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_End)) { next={max_rows-1,max_columns-1}; moved=true; }
        if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_Home)) { next={0,0}; moved=true; }
        if(moved) { row_selection_.clear(); select(next,io.KeyShift); viewport_.reveal(next); }
        if(ImGui::IsKeyPressed(ImGuiKey_Delete)) action_=Action::Clear;
        if(ImGui::IsKeyPressed(ImGuiKey_Enter)||ImGui::IsKeyPressed(ImGuiKey_F2)) { editing_=true; focus_editor_=true; }
        if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_C)) action_=Action::Copy;
        if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_X)) action_=Action::Cut;
        if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_V)) action_=io.KeyShift?Action::PasteValues:Action::Paste;
        if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_Z)) action_=Action::Undo;
        if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_Y)) action_=Action::Redo;
    }
    auto* draw=ImGui::GetWindowDrawList();
    const float rh=viewport_.row_height,cw=viewport_.column_width,header=viewport_.row_header,ch=viewport_.column_header;
    const ImVec2 end{origin.x+available.x,origin.y+available.y};
    draw->PushClipRect(origin,end,true);
    draw->AddRectFilled(origin,end,(dark_?IM_COL32(25,34,43,255):IM_COL32(255,255,255,255)));
    draw->AddRectFilled(origin,{end.x,origin.y+ch},(dark_?IM_COL32(31,42,52,255):IM_COL32(241,245,246,255)));
    draw->AddRectFilled(origin,{origin.x+header,end.y},(dark_?IM_COL32(31,42,52,255):IM_COL32(241,245,246,255)));
    char buffer[512],colname[24];
    const auto top=std::min(active_.row,anchor_.row),bottom=std::max(active_.row,anchor_.row);
    const auto left=std::min(active_.column,anchor_.column),right=std::max(active_.column,anchor_.column);
    for(int c=0;c<=viewport_.visible_columns&&viewport_.column_at(c)<max_columns;++c) {
        const float x=origin.x+header+float(c)*cw;
        if(viewport_.column_at(c)==active_.column) {
            draw->AddRectFilled({x,origin.y},{x+cw,origin.y+ch},dark_?IM_COL32(35,74,64,255):IM_COL32(212,239,229,255));
            draw->AddLine({x,origin.y+ch-1},{x+cw,origin.y+ch-1},IM_COL32(24,153,116,255),2*scale_);
        }
        label({0,viewport_.column_at(c)},colname,sizeof colname,false);
        draw->AddText({x+cw/2-5*scale_,origin.y+8*scale_},(dark_?IM_COL32(163,181,194,255):IM_COL32(99,111,123,255)),colname);
        if(gridlines_) draw->AddLine({x,origin.y},{x,end.y},(dark_?IM_COL32(48,61,73,255):IM_COL32(224,232,237,255)));
    }
    for(int r=0;r<=viewport_.visible_rows&&viewport_.row_at(r)<max_rows;++r) {
        const auto row=viewport_.row_at(r);
        const float y=origin.y+ch+float(r)*rh;
        bool selected_row=false; for(auto interval:row_selection_) if(row>=interval.first&&row<=interval.last) selected_row=true;
        if(row==active_.row) draw->AddRectFilled({origin.x,y},{origin.x+header,y+rh},dark_?IM_COL32(35,74,64,255):IM_COL32(212,239,229,255));
        std::snprintf(buffer,sizeof buffer,"%u",row+1);
        draw->AddText({origin.x+7*scale_,y+8*scale_},(dark_?IM_COL32(163,181,194,255):IM_COL32(99,111,123,255)),buffer);
        for(int c=0;c<=viewport_.visible_columns&&viewport_.column_at(c)<max_columns;++c) {
            const auto column=viewport_.column_at(c);
            const auto style=sheet_.cell_style({row,column});
            const float x=origin.x+header+float(c)*cw;
            const bool selected=selected_row||(row>=top&&row<=bottom&&column>=left&&column<=right);
            draw->AddRectFilled({x+1,y+1},{x+cw,y+rh},selected?(dark_?IM_COL32(31,67,60,255):IM_COL32(229,244,238,255)):(dark_?(style.background==0xFFFFFFFF?IM_COL32(25,34,43,255):rgba(style.background)):rgba(style.background)));
        }
        for(int c=0;c<=viewport_.visible_columns&&viewport_.column_at(c)<max_columns;++c) {
            const auto column=viewport_.column_at(c);
            const auto style=sheet_.cell_style({row,column});
            const float x=origin.x+header+float(c)*cw;
            const auto* cell=sheet_.cell({row,column});
            if(cell) {
                const char* value=format_value(cell->cached,buffer,sizeof buffer,style.decimals);
                if(show_formulas_) if(auto formula=std::get_if<FormulaInput>(&cell->input)) value=formula->source.c_str();
                auto color=std::holds_alternative<CellError>(cell->cached)?IM_COL32(173,50,63,255):(dark_?(style.foreground==0x155E4BFF?IM_COL32(132,220,190,255):(style.foreground==0x202B39FF?IM_COL32(221,230,237,255):rgba(style.foreground))):rgba(style.foreground));
                if(auto n=std::get_if<double>(&cell->cached);n&&style.number_format) {
                    if(style.number_format==1)std::snprintf(buffer,sizeof buffer,"%.*f",style.decimals,*n);
                    if(style.number_format==2)grouped(buffer,sizeof buffer,*n,style.decimals,"$");
                    if(style.number_format==5)grouped(buffer,sizeof buffer,*n,style.decimals,"");
                    if(style.number_format==3)std::snprintf(buffer,sizeof buffer,"%.*f%%",style.decimals,*n*100);
                    if(style.number_format==4&&*n>=1&&*n<=2958465) {
                        auto serial=static_cast<long long>(std::floor(*n));
                        const auto date=std::chrono::year_month_day{std::chrono::sys_days{std::chrono::year{1899}/12/31}+std::chrono::days{serial-(serial>=60?1:0)}};
                        std::snprintf(buffer,sizeof buffer,"%04d-%02u-%02u",int(date.year()),unsigned(date.month()),unsigned(date.day()));
                    }
                    value=buffer;
                }
                auto* cell_font=ImGui::GetIO().Fonts->Fonts[std::min(int(style.font_family),ImGui::GetIO().Fonts->Fonts.Size-1)];
                const float text_size=ImGui::GetFontSize()*zoom_*float(style.font_size)/16;
                const float text_width=cell_font->CalcTextSizeA(text_size,FLT_MAX,0,value).x;
                float tx=style.alignment==2?x+(cw-text_width)/2:style.alignment==1?x+9*scale_:(style.alignment==3||std::holds_alternative<double>(cell->cached))?std::max(x+8*scale_,x+cw-10*scale_-cell_font->CalcTextSizeA(text_size,FLT_MAX,0,value).x):x+9*scale_;
                float text_right=x+cw-5*scale_;
                // Excel-style spill into empty neighbours is optional; by default text stays in its own cell.
                if(text_overflow_&&std::holds_alternative<std::string>(cell->cached)) {
                    for(unsigned next=column+1;next<max_columns&&text_right<end.x;++next) {
                        if(sheet_.cell({row,next})) break;
                        text_right+=cw;
                    }
                }
                const char* full=value; char fitted[512]; bool clipped=false;
                const float left_edge=x+9*scale_, room=text_right-left_edge;
                if(text_width>room+0.5f) {
                    clipped=true;
                    if(std::holds_alternative<double>(cell->cached)&&!show_formulas_) {
                        // A cut-off number is easy to misread (1234567 as 12345): show ### like Excel instead.
                        value="###"; tx=x+(cw-cell_font->CalcTextSizeA(text_size,FLT_MAX,0,value).x)/2;
                    } else {
                        // Cut at a character boundary and end with "..."; the full text is in the formula bar.
                        const float dots=cell_font->CalcTextSizeA(text_size,FLT_MAX,0,"...").x;
                        const std::size_t length=std::min<std::size_t>(std::strlen(full),sizeof fitted-4);
                        std::size_t low=0, high=length;
                        while(low<high) {
                            std::size_t mid=(low+high+1)/2;
                            while(mid>low&&(static_cast<unsigned char>(full[mid])&0xc0)==0x80) --mid;
                            if(mid==low) break;
                            if(cell_font->CalcTextSizeA(text_size,FLT_MAX,0,full,full+mid).x+dots<=room) low=mid; else high=mid-1;
                        }
                        while(low>0&&(static_cast<unsigned char>(full[low])&0xc0)==0x80) --low;
                        std::memcpy(fitted,full,low); std::memcpy(fitted+low,"...",4);
                        value=fitted; tx=left_edge;
                    }
                }
                draw->PushClipRect({x+5*scale_,y+1},{text_right,y+rh-1},true);
                draw->AddText(cell_font,text_size,{tx,y+8*scale_*zoom_},color,value);
                if(style.bold) draw->AddText(cell_font,text_size,{tx+0.5f*scale_,y+8*scale_*zoom_},color,value);
                draw->PopClipRect();
                if(clipped&&hovered) {
                    const auto mouse=ImGui::GetIO().MousePos;
                    if(mouse.x>=x&&mouse.x<x+cw&&mouse.y>=y&&mouse.y<y+rh) ImGui::SetTooltip("%s",full);
                }
            }
            if(style.border)draw->AddRect({x+1,y+1},{x+cw-1,y+rh-1},ImGui::GetColorU32(ImGuiCol_TextDisabled));
            if(CellCoord{row,column}==active_) draw->AddRect({x+1,y+1},{x+cw-1,y+rh-1},(dark_?IM_COL32(89,211,171,255):IM_COL32(15,148,109,255)),0,0,2*scale_);
        }
        if(gridlines_) draw->AddLine({origin.x,y},{end.x,y},(dark_?IM_COL32(48,61,73,255):IM_COL32(224,232,237,255)));
    }
    draw->AddLine({origin.x+header,origin.y},{origin.x+header,end.y},(dark_?IM_COL32(56,71,83,255):IM_COL32(214,222,226,255)));
    draw->AddLine({origin.x,origin.y+ch},{end.x,origin.y+ch},(dark_?IM_COL32(56,71,83,255):IM_COL32(214,222,226,255)));
    draw->AddRectFilled({hx-4*scale_,hy-4*scale_},{hx+4*scale_,hy+4*scale_},ImGui::GetColorU32(ImGuiCol_CheckMark));
    draw->PopClipRect();
    if(editing_&&!oversized_&&!selection_changed_) {
        const float x=origin.x+header+float(viewport_.screen_column(active_.column))*cw;
        const float y=origin.y+ch+float(viewport_.screen_row(active_.row))*rh;
        if(x>=origin.x+header&&x<end.x&&y>=origin.y+ch&&y<end.y) {
            ImGui::SetCursorScreenPos({x+2,y+2}); ImGui::SetNextItemWidth(cw-4);
            if(focus_editor_) { ImGui::SetKeyboardFocusHere(); focus_editor_=false; }
            if(ImGui::InputText("##cell-edit",editor_.data(),editor_.size(),ImGuiInputTextFlags_EnterReturnsTrue|ImGuiInputTextFlags_AutoSelectAll)) queue_edit();
            if(ImGui::IsItemEdited()) editor_dirty_=true;
            if(ImGui::IsKeyPressed(ImGuiKey_Escape)) { editor_dirty_=false; editing_=false; selection_changed_=true; }
        }
    }
    ImGui::EndChild();
}
void GridUI::caption_item() {
    const auto a=ImGui::GetItemRectMin(), b=ImGui::GetItemRectMax();
    caption_items_.push_back({a.x,a.y,b.x,b.y});
}
void GridUI::draw_window_buttons() {
#ifdef _WIN32
    if(!custom_frame||!native_window) return;
    auto window=static_cast<HWND>(native_window);
    const float w=46*scale_, h=ImGui::GetFrameHeight();
    const float top=ImGui::GetWindowPos().y, right=ImGui::GetWindowPos().x+ImGui::GetWindowWidth();
    const bool maximized=IsZoomed(window);
    const Glyph icons[]{Glyph::WinMin,maximized?Glyph::WinRestore:Glyph::WinMax,Glyph::WinClose};
    const char* tips[]{"Minimize",maximized?"Restore down":"Maximize","Close"};
    auto* d=ImGui::GetWindowDrawList();
    for(int i=0;i<3;++i) {
        const ImVec2 p{right-float(3-i)*w,top};
        ImGui::SetCursorScreenPos(p); ImGui::PushID(i);
        const bool clicked=ImGui::InvisibleButton("window-button",{w,h}), hovered=ImGui::IsItemHovered();
        caption_item();
        if(hovered) d->AddRectFilled(p,{p.x+w,p.y+h},i==2?IM_COL32(196,43,28,255):ImGui::GetColorU32(ImGuiCol_ButtonHovered));
        const float g=10*scale_;
        glyph(d,{p.x+(w-g)/2,p.y+(h-g)/2},g,hovered&&i==2?IM_COL32(255,255,255,255):ImGui::GetColorU32(ImGuiCol_Text),icons[i]);
        if(hovered) ImGui::SetTooltip("%s",tips[i]);
        if(clicked) {
            if(i==0) ShowWindow(window,SW_MINIMIZE);
            else if(i==1) ShowWindow(window,maximized?SW_RESTORE:SW_MAXIMIZE);
            else PostMessageW(window,WM_CLOSE,0,0); // the main loop asks to save first
        }
        ImGui::PopID();
    }
#endif
}
void GridUI::close_workbook() { workbook_minimized_=workbook_floating_=false; action_=Action::New; }
void GridUI::format_cells(std::function<void(Style&)> change,std::string done) {
    pending_=[this,change=std::move(change),done=std::move(done)] {
        if(viewport_.filtered) throw std::runtime_error("Clear the filter before formatting cells.");
        Batch b;
        if(!row_selection_.empty()) {
            // Whole rows are selected: format the rows, as the row-format commands do.
            std::size_t count=0; for(auto interval:row_selection_) count+=interval.last-interval.first+1;
            if(count>10'000) throw std::runtime_error("Format at most 10,000 rows per batch.");
            for(auto interval:row_selection_) for(auto row=interval.first;row<=interval.last;++row) { auto style=sheet_.row_style(row); change(style); b.rows.push_back({row,style}); }
        } else {
            const auto top=std::min(active_.row,anchor_.row),bottom=std::max(active_.row,anchor_.row);
            const auto left=std::min(active_.column,anchor_.column),right=std::max(active_.column,anchor_.column);
            if(std::uint64_t(bottom-top+1)*(right-left+1)>10'000) throw std::runtime_error("Select up to 10,000 cells to format.");
            for(auto r=top;r<=bottom;++r) for(auto c=left;c<=right;++c) { auto style=sheet_.cell_style({r,c}); change(style); b.formats.push_back({{r,c},style}); }
        }
        auto result=sheet_.apply(b); if(!result.accepted) throw std::runtime_error(result.error->context);
        modified_=true; message_=done;
    };
}
void GridUI::clear_formats(bool contents) {
    pending_=[this,contents] {
        if(viewport_.filtered) throw std::runtime_error("Clear the filter before clearing a range.");
        const auto top=std::min(active_.row,anchor_.row),bottom=std::max(active_.row,anchor_.row);
        const auto left=std::min(active_.column,anchor_.column),right=std::max(active_.column,anchor_.column);
        auto selected=[&](CellCoord c) {
            if(!row_selection_.empty()) { for(auto interval:row_selection_) if(c.row>=interval.first&&c.row<=interval.last) return true; return false; }
            return c.row>=top&&c.row<=bottom&&c.column>=left&&c.column<=right;
        };
        Batch b;
        for(const auto& [coord,style]:sheet_.cell_styles()) { (void)style; if(selected(coord)) b.formats.push_back({coord,std::nullopt}); }
        if(contents) for(const auto& [r,row]:sheet_.populated_rows()) for(const auto& [c,cell]:row) { (void)cell; if(selected({r,c})) b.cells.push_back({{r,c},std::monostate{}}); }
        if(b.formats.empty()&&b.cells.empty()) { message_="Nothing to clear in the selection."; return; }
        auto result=sheet_.apply(b); if(!result.accepted) throw std::runtime_error(result.error->context);
        modified_=true; message_=contents?"Contents and formatting cleared.":"Cell formatting cleared.";
    };
}
void GridUI::draw_minimized_workbook(float x,float y,float,float height) {
    const float s=scale_, bar_width=330*s, bar_height=38*s;
    const ImVec2 p{x+14*s,y+height-bar_height-14*s};
    auto* d=ImGui::GetWindowDrawList();
    d->AddRectFilled(p,{p.x+bar_width,p.y+bar_height},ImGui::GetColorU32(ImGuiCol_TitleBgActive),6*s);
    d->AddRect(p,{p.x+bar_width,p.y+bar_height},ImGui::GetColorU32(ImGuiCol_Border),6*s);
    const auto hint="Workbook minimized. Restore it from the bar below, or from the buttons beside the ribbon tabs.";
    const auto hint_size=ImGui::CalcTextSize(hint);
    d->AddText({x+(std::max(0.0f,bar_width*2.4f-hint_size.x))/2+14*s,y+40*s},ImGui::GetColorU32(ImGuiCol_TextDisabled),hint);
    ImGui::PushClipRect(p,{p.x+bar_width-104*s,p.y+bar_height},true);
    d->AddText({p.x+12*s,p.y+(bar_height-ImGui::GetFontSize())/2},ImGui::GetColorU32(ImGuiCol_Text),workbook_name_.c_str());
    ImGui::PopClipRect();
    // Each button is placed explicitly: SameLine here would inherit the navigation column's line.
    auto place=[&](int i){ ImGui::SetCursorScreenPos({p.x+bar_width-100*s+float(i)*32*s,p.y+5*s}); };
    place(0); if(icon_button("minimized-restore",Glyph::WinRestore,s,"Restore workbook window")) { workbook_minimized_=false; workbook_floating_=true; }
    place(1); if(icon_button("minimized-max",Glyph::WinMax,s,"Maximize workbook")) { workbook_minimized_=false; workbook_floating_=false; }
    place(2); if(icon_button("minimized-close",Glyph::WinClose,s,"Close workbook")) pending_=[this]{ close_workbook(); };
}
void GridUI::draw_home_ribbon(bool& guide_popup,bool& settings_popup) {
    const float s=scale_; const Style current=sheet_.cell_style(active_);
    auto* d=ImGui::GetWindowDrawList();
    constexpr float body=66; bool last_group=false;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{6*s,4*s}); ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{4*s,6*s});
    // A labelled ribbon group with a divider after it, like Excel's Clipboard / Font / Number groups.
    auto group=[&](const char* label,auto&& contents) {
        ImGui::BeginGroup(); const auto top=ImGui::GetCursorScreenPos();
        contents();
        ImGui::SetCursorScreenPos({top.x,top.y+body*s}); ImGui::Dummy({1,ImGui::GetFontSize()});
        ImGui::EndGroup();
        const auto a=ImGui::GetItemRectMin(), b=ImGui::GetItemRectMax(); const float fs=ImGui::GetFontSize()*.8f;
        const auto size=ImGui::GetFont()->CalcTextSizeA(fs,FLT_MAX,0,label);
        d->AddText(ImGui::GetFont(),fs,{(a.x+b.x-size.x)/2,top.y+(body+1)*s},ImGui::GetColorU32(ImGuiCol_TextDisabled),label);
        if(last_group) return;
        ImGui::SameLine(0,6*s);
        const auto p=ImGui::GetCursorScreenPos(); d->AddLine({p.x,a.y},{p.x,b.y},ImGui::GetColorU32(ImGuiCol_Border));
        ImGui::Dummy({1,1}); ImGui::SameLine(0,6*s);
    };
    auto palette=[&](const char* id,bool fill) {
        if(!ImGui::BeginPopup(id)) return;
        static const std::uint32_t colors[]{0xFFFFFFFF,0xF2F2F2FF,0xD9D9D9FF,0x808080FF,0x262626FF,0x000000FF,0xC00000FF,0xFF0000FF,0xFFC000FF,0xFFFF00FF,
                                            0x92D050FF,0x00B050FF,0x1F7A5CFF,0x00B0F0FF,0x0070C0FF,0x002060FF,0x7030A0FF,0xFFC7CEFF,0xC6EFCEFF,0xFFEB9CFF};
        ImGui::TextDisabled(fill?"Fill color":"Font color");
        for(int i=0;i<20;++i) {
            const auto c=colors[i]; ImGui::PushID(i);
            if(ImGui::ColorButton("##swatch",ImVec4{float(c>>24)/255,float((c>>16)&255)/255,float((c>>8)&255)/255,1},ImGuiColorEditFlags_NoTooltip,{24*s,24*s})) {
                format_cells([c,fill](Style& style){ (fill?style.background:style.foreground)=c; },fill?"Fill color applied.":"Font color applied.");
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopID(); if(i%10!=9) ImGui::SameLine();
        }
        if(ImGui::MenuItem(fill?"No fill":"Automatic"))
            format_cells([fill](Style& style){ const Style base; if(fill) style.background=base.background; else style.foreground=base.foreground; },fill?"Fill removed.":"Font color reset.");
        if(ImGui::MenuItem("More colors...")) { format_style_=current; format_open_=true; }
        ImGui::EndPopup();
    };
    group("Clipboard",[&] {
        if(ribbon_button("Paste",Glyph::Paste,s,68,true)) ImGui::OpenPopup("paste-menu");
        if(ImGui::BeginPopup("paste-menu")) {
            if(ImGui::MenuItem("Paste","Ctrl+V")) action_=Action::Paste;
            if(ImGui::MenuItem("Paste values","Ctrl+Shift+V")) action_=Action::PasteValues;
            ImGui::EndPopup();
        }
        ImGui::SameLine(); ImGui::BeginGroup();
        if(labeled_button("Cut",Glyph::Cut,s,66)) action_=Action::Cut;
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Cut (Ctrl+X)");
        if(labeled_button("Copy",Glyph::Copy,s,66)) action_=Action::Copy;
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Copy (Ctrl+C)");
        ImGui::EndGroup();
    });
    group("Font",[&] {
        static const char* families[]{"Segoe UI","Georgia","Consolas"};
        ImGui::SetNextItemWidth(108*s);
        if(ImGui::BeginCombo("##font-family",families[std::min<int>(current.font_family,2)])) {
            for(int i=0;i<3;++i) if(ImGui::Selectable(families[i],current.font_family==i)) format_cells([i](Style& style){ style.font_family=std::uint8_t(i); },"Font changed.");
            ImGui::EndCombo();
        }
        ImGui::SameLine(); ImGui::SetNextItemWidth(56*s);
        char size_label[8]; std::snprintf(size_label,sizeof size_label,"%d",current.font_size);
        if(ImGui::BeginCombo("##font-size",size_label)) {
            static const int sizes[]{8,9,10,11,12,14,16,18,20,22,24,26,28,32,36};
            for(int size:sizes) { char text[8]; std::snprintf(text,sizeof text,"%d",size); if(ImGui::Selectable(text,current.font_size==size)) format_cells([size](Style& style){ style.font_size=std::uint8_t(size); },"Font size changed."); }
            ImGui::EndCombo();
        }
        ImGui::SameLine(); if(icon_button("font-up",Glyph::FontUp,s,"Increase font size")) format_cells([](Style& style){ style.font_size=std::uint8_t(std::min(36,style.font_size+2)); },"Font size increased.");
        ImGui::SameLine(); if(icon_button("font-down",Glyph::FontDown,s,"Decrease font size")) format_cells([](Style& style){ style.font_size=std::uint8_t(std::max(8,style.font_size-2)); },"Font size decreased.");
        const bool bold=current.bold, border=current.border;
        if(icon_button("bold",Glyph::Bold,s,"Bold",bold)) format_cells([bold](Style& style){ style.bold=!bold; },bold?"Bold removed.":"Bold applied.");
        remember_target(BoldButton);
        ImGui::SameLine(); if(icon_button("border",Glyph::Border,s,"Borders",border)) format_cells([border](Style& style){ style.border=!border; },border?"Borders removed.":"Borders applied.");
        ImGui::SameLine(); if(icon_button("fill",Glyph::Fill,s,"Fill color")) ImGui::OpenPopup("fill-palette");
        ImGui::SameLine(); if(icon_button("text-color",Glyph::TextColor,s,"Font color")) ImGui::OpenPopup("text-palette");
        palette("fill-palette",true); palette("text-palette",false);
    });
    group("Alignment",[&] {
        const Glyph icons[]{Glyph::AlignLeft,Glyph::AlignCenter,Glyph::AlignRight}; const char* tips[]{"Align left","Center","Align right"};
        for(int i=0;i<3;++i) {
            if(i) ImGui::SameLine();
            const int value=i+1;
            if(icon_button(tips[i],icons[i],s,tips[i],current.alignment==value)) format_cells([value](Style& style){ style.alignment=std::uint8_t(value); },"Alignment changed.");
        }
        if(ImGui::Button("Automatic",{98*s,28*s})) format_cells([](Style& style){ style.alignment=0; },"Automatic alignment: numbers right, text left.");
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Numbers align right and text aligns left");
    });
    group("Number",[&] {
        static const char* formats[]{"General","Number","Currency","Percentage","Date","Comma"};
        ImGui::SetNextItemWidth(150*s);
        if(ImGui::BeginCombo("##number-format",formats[std::min<int>(current.number_format,5)])) {
            for(int i=0;i<6;++i) if(ImGui::Selectable(formats[i],current.number_format==i)) format_cells([i](Style& style){ style.number_format=std::uint8_t(i); },"Number format applied.");
            ImGui::EndCombo();
        }
        if(icon_button("currency",Glyph::Currency,s,"Currency ($1,234.00)")) format_cells([](Style& style){ style.number_format=2; style.decimals=2; },"Currency format applied.");
        ImGui::SameLine(); if(icon_button("percent",Glyph::Percent,s,"Percent style")) format_cells([](Style& style){ style.number_format=3; style.decimals=0; },"Percent format applied.");
        ImGui::SameLine(); if(icon_button("comma",Glyph::Comma,s,"Comma style (1,234.00)")) format_cells([](Style& style){ style.number_format=5; style.decimals=2; },"Comma style applied.");
        ImGui::SameLine(); if(icon_button("decimals-less",Glyph::DecLess,s,"Decrease decimal")) format_cells([](Style& style){ if(style.decimals>0) --style.decimals; if(!style.number_format) style.number_format=1; },"Fewer decimal places.");
        ImGui::SameLine(); if(icon_button("decimals-more",Glyph::DecMore,s,"Increase decimal",false,36)) format_cells([](Style& style){ if(style.decimals<12) ++style.decimals; if(!style.number_format) style.number_format=1; },"More decimal places.");
    });
    group("Styles",[&] {
        if(ribbon_button("Cell styles",Glyph::Styles,s,96,true)) ImGui::OpenPopup("cell-styles");
        if(ImGui::BeginPopup("cell-styles")) {
            struct Preset { const char* name; std::uint32_t foreground,background; bool bold,border; std::uint8_t size; };
            static const Preset presets[]{{"Normal",0x202B39FF,0xFFFFFFFF,false,false,16},{"Good",0x006100FF,0xC6EFCEFF,false,false,16},
                {"Bad",0x9C0006FF,0xFFC7CEFF,false,false,16},{"Neutral",0x9C5700FF,0xFFEB9CFF,false,false,16},
                {"Title",0x155E4BFF,0xFFFFFFFF,true,false,26},{"Heading 1",0x155E4BFF,0xFFFFFFFF,true,false,20},
                {"Heading 2",0x155E4BFF,0xFFFFFFFF,true,false,18},{"Total",0x202B39FF,0xFFFFFFFF,true,true,16},
                {"Input",0x3F3F76FF,0xFFCC99FF,false,true,16},{"Note",0x202B39FF,0xFFFFCCFF,false,true,16},{"Accent",0xFFFFFFFF,0x1F7A5CFF,true,false,16}};
            ImGui::TextDisabled("Cell styles keep each cell's number format and alignment");
            for(const auto& preset:presets) {
                const auto bg=preset.background; ImGui::PushID(preset.name);
                ImGui::ColorButton("##chip",ImVec4{float(bg>>24)/255,float((bg>>16)&255)/255,float((bg>>8)&255)/255,1},ImGuiColorEditFlags_NoTooltip,{20*s,20*s});
                ImGui::SameLine();
                if(ImGui::Selectable(preset.name)) format_cells([preset](Style& style) {
                    Style next; next.number_format=style.number_format; next.decimals=style.decimals; next.alignment=style.alignment; next.font_family=style.font_family;
                    next.foreground=preset.foreground; next.background=preset.background; next.bold=preset.bold; next.border=preset.border; next.font_size=preset.size;
                    style=next;
                },std::string("Cell style applied: ")+preset.name);
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        if(ribbon_button("Table style",Glyph::Table,s,104,true)) ImGui::OpenPopup("table-styles");
        if(ImGui::BeginPopup("table-styles")) {
            struct TableStyle { const char* name; std::uint32_t header,band; };
            static const TableStyle styles[]{{"Green",0x1F7A5CFF,0xE2F1EBFF},{"Blue",0x0070C0FF,0xDDEBF7FF},{"Grey",0x595959FF,0xEDEDEDFF},{"Orange",0xC65911FF,0xFCE4D6FF}};
            ImGui::TextDisabled("Formats the selection as a table: header row plus banded rows");
            for(const auto& style:styles) if(ImGui::MenuItem(style.name)) {
                const auto header=style.header, band=style.band;
                pending_=[this,header,band] {
                    if(viewport_.filtered) throw std::runtime_error("Clear the filter before formatting a table.");
                    const auto top=std::min(active_.row,anchor_.row),bottom=std::max(active_.row,anchor_.row);
                    const auto left=std::min(active_.column,anchor_.column),right=std::max(active_.column,anchor_.column);
                    if(bottom==top) throw std::runtime_error("Select a header row and at least one data row.");
                    if(std::uint64_t(bottom-top+1)*(right-left+1)>10'000) throw std::runtime_error("Select up to 10,000 cells to format.");
                    Batch b;
                    for(auto r=top;r<=bottom;++r) for(auto c=left;c<=right;++c) {
                        auto st=sheet_.cell_style({r,c}); st.border=true;
                        if(r==top) { st.bold=true; st.background=header; st.foreground=0xFFFFFFFF; }
                        else st.background=(r-top)%2==0?band:0xFFFFFFFF;
                        b.formats.push_back({{r,c},st});
                    }
                    auto result=sheet_.apply(b); if(!result.accepted) throw std::runtime_error(result.error->context);
                    modified_=true; message_="Table formatting applied. Ctrl+Z undoes it.";
                };
            }
            ImGui::EndPopup();
        }
    });
    group("Cells",[&] {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{4*s,3*s});
        if(labeled_button("Insert",Glyph::Insert,s,90,true,20)) ImGui::OpenPopup("insert-menu");
        if(labeled_button("Delete",Glyph::Delete,s,90,true,20)) ImGui::OpenPopup("delete-menu");
        if(labeled_button("Format",Glyph::Sheet,s,90,true,20)) ImGui::OpenPopup("cells-format-menu");
        ImGui::PopStyleVar();
        if(ImGui::BeginPopup("insert-menu")) {
            if(ImGui::MenuItem("Insert sheet row above")) action_=Action::InsertRow;
            if(ImGui::MenuItem("Insert sheet column before")) action_=Action::InsertColumn;
            ImGui::Separator(); if(ImGui::MenuItem("Insert worksheet")) action_=Action::AddSheet;
            ImGui::EndPopup();
        }
        if(ImGui::BeginPopup("delete-menu")) {
            if(ImGui::MenuItem("Delete sheet row")) action_=Action::DeleteRow;
            if(ImGui::MenuItem("Delete sheet column")) action_=Action::DeleteColumn;
            ImGui::Separator(); if(ImGui::MenuItem("Delete worksheet...")) sheets_open_=true;
            ImGui::EndPopup();
        }
        if(ImGui::BeginPopup("cells-format-menu")) {
            if(ImGui::MenuItem("Format cells...")) { format_style_=current; format_open_=true; }
            ImGui::Separator(); ImGui::TextDisabled("Whole selected rows");
            if(ImGui::MenuItem("Bold rows")) action_=Action::Bold;
            if(ImGui::MenuItem("Green row fill")) action_=Action::Tint;
            if(ImGui::MenuItem("Reset row formatting")) action_=Action::ResetStyle;
            ImGui::Separator();
            if(ImGui::MenuItem("Rename, move or delete worksheets...")) sheets_open_=true;
            ImGui::EndPopup();
        }
    });
    group("Editing",[&] {
        ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{4*s,3*s});
        if(labeled_button("AutoSum",Glyph::Sigma,s,98,true,20)) ImGui::OpenPopup("autosum-menu");
        if(labeled_button("Fill",Glyph::Down,s,98,true,20)) ImGui::OpenPopup("fill-menu");
        if(labeled_button("Clear",Glyph::Clear,s,98,true,20)) ImGui::OpenPopup("clear-menu");
        ImGui::PopStyleVar(); ImGui::EndGroup();
        ImGui::SameLine();
        if(ribbon_button("Sort & Filter",Glyph::Filter,s,110,true)) ImGui::OpenPopup("sort-menu");
        ImGui::SameLine();
        if(ribbon_button("Find & Select",Glyph::Find,s,110,true)) ImGui::OpenPopup("find-menu");
        if(ImGui::BeginPopup("autosum-menu")) {
            if(ImGui::MenuItem("Sum")) action_=Action::QuickSum;
            if(ImGui::MenuItem("Average")) action_=Action::QuickAverage;
            ImGui::TextDisabled("Adds the formula below the selection.");
            ImGui::EndPopup();
        }
        if(ImGui::BeginPopup("fill-menu")) {
            if(ImGui::MenuItem("Down")) action_=Action::FillDown;
            if(ImGui::MenuItem("Right")) action_=Action::FillRight;
            ImGui::TextDisabled("Or drag the handle at the selection corner.");
            ImGui::EndPopup();
        }
        if(ImGui::BeginPopup("clear-menu")) {
            if(ImGui::MenuItem("Clear all")) clear_formats(true);
            if(ImGui::MenuItem("Clear formats")) clear_formats(false);
            if(ImGui::MenuItem("Clear contents","Delete")) action_=Action::Clear;
            ImGui::EndPopup();
        }
        if(ImGui::BeginPopup("sort-menu")) {
            if(ImGui::MenuItem("Sort A to Z (smallest first)")) action_=Action::SortAscending;
            if(ImGui::MenuItem("Sort Z to A (largest first)")) action_=Action::SortDescending;
            ImGui::TextDisabled("Sorts the selection by its active column; exclude your header.");
            ImGui::Separator();
            if(ImGui::MenuItem("Filter active column...")) { filter_column_=active_.column; filter_open_=true; }
            if(ImGui::MenuItem("Clear filter",nullptr,false,viewport_.filtered)) { viewport_.filtered=false; viewport_.filtered_rows.clear(); viewport_.first_row=0; }
            ImGui::EndPopup();
        }
        if(ImGui::BeginPopup("find-menu")) {
            if(ImGui::MenuItem("Find...","Ctrl+F")) find_open_=true;
            if(ImGui::MenuItem("Go to cell...","Ctrl+K")) focus_jump_=true;
            if(ImGui::MenuItem("Select used range")) select_used();
            ImGui::EndPopup();
        }
    });
    last_group=true;
    group("Assist",[&] {
        if(ribbon_button("Ask AI",Glyph::Ai,s,62)) show_ai(!ai_open_);
        ImGui::SameLine(); ImGui::BeginGroup();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{4*s,3*s});
        if(labeled_button("Functions",Glyph::Function,s,100,false,20)) guide_popup=true;
        if(labeled_button("Lua scripts",Glyph::Code,s,100,false,20)) show_scripts(!scripts_open_);
        if(labeled_button("Appearance",Glyph::Sun,s,100,false,20)) settings_popup=true;
        ImGui::PopStyleVar(); ImGui::EndGroup();
    });
    ImGui::PopStyleVar(2);
}
void GridUI::draw() {
    auto* vp=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos); ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{12*scale_,12*scale_});
    ImGui::Begin("Julretsu workspace",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_MenuBar|ImGuiWindowFlags_NoBringToFrontOnFocus);
    // While any popup is open the title rows behave as ordinary client area, so menus stay clickable.
    caption_items_.clear(); caption_enabled_=!ImGui::IsPopupOpen("",ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
    draw_tools();
    const auto accent=dark_?ImVec4{0.40f,0.85f,0.69f,1}:ImVec4{0.03f,0.43f,0.32f,1};
    bool new_popup=false, guide_popup=false, template_popup=false, settings_popup=false;

    ImGui::AlignTextToFramePadding();
    const auto brand=ImGui::GetCursorScreenPos();
    if(brand_icon) ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(brand_icon),brand,{brand.x+30*scale_,brand.y+30*scale_});
    ImGui::Dummy({38*scale_,30*scale_}); ImGui::SameLine();
    ImGui::AlignTextToFramePadding(); ImGui::TextColored(accent,"JULRETSU");
    ImGui::SameLine(180*scale_); ImGui::BeginChild("Workbook name",{280*scale_,34*scale_},0,ImGuiWindowFlags_NoScrollbar); ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(workbook_name_.c_str()); if(ImGui::IsItemHovered()) ImGui::SetTooltip("%s",file_path_.empty()?workbook_name_.c_str():file_label_.c_str()); ImGui::EndChild();
    ImGui::SameLine(); ImGui::TextDisabled(modified()?"  /  Unsaved changes":(file_path_.empty()?"  /  Not saved yet":"  /  Saved to this device"));
    ImGui::SameLine(std::max(560*scale_,vp->WorkSize.x*0.48f));
    ImGui::SetNextItemWidth(std::max(140*scale_,vp->WorkSize.x*0.21f));
    if(focus_jump_||(ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_K))) { ImGui::SetKeyboardFocusHere(); focus_jump_=false; }
    if(ImGui::InputTextWithHint("##jump","Go to cell  (Ctrl + K)",search_.data(),search_.size(),ImGuiInputTextFlags_EnterReturnsTrue)) {
        auto parsed=from_a1(search_.data());
        if(auto c=std::get_if<CellCoord>(&parsed)) { jump(*c); grid_focused_=true; search_[0]=0; }
        else message_="Enter a cell address, such as C4 or XFD1000000.";
    }
    caption_item();
    ImGui::SameLine(vp->WorkSize.x-140*scale_);
    if(ImGui::Button(dark_?"Light mode":"Dark mode",{120*scale_,0})) set_dark(!dark_,remember_theme_);
    remember_target(ThemeButton); caption_item();
    ImGui::Spacing();
    caption_height_=ImGui::GetCursorScreenPos().y-vp->Pos.y;
    if(ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_S)) action_=ImGui::GetIO().KeyShift?Action::SaveAs:Action::Save;
    if(ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_O)) action_=Action::Open;
    if(ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_N)) action_=Action::New;
    if(!file_error_.empty()) ImGui::OpenPopup("File could not be saved or opened");
    if(ImGui::BeginPopupModal("File could not be saved or opened",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+500*scale_);ImGui::TextUnformatted(file_error_.c_str());ImGui::PopTextWrapPos();
        if(ImGui::Button("OK")) {file_error_.clear();ImGui::CloseCurrentPopup();} ImGui::EndPopup();
    }
    if(ImGui::Button("File",{72*scale_,30*scale_})) ImGui::OpenPopup("File menu"); remember_target(FileButton);
    ImGui::SetNextWindowSizeConstraints({540*scale_,0},{650*scale_,FLT_MAX});
    if(ImGui::BeginPopup("File menu")) {
        ImGui::TextColored(accent,"This workbook");
        ImGui::TextUnformatted(workbook_name_.c_str());
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+420*scale_);
        ImGui::TextDisabled("%s",file_path_.empty()?"Choose a folder to save this workbook.":file_label_.c_str()); ImGui::PopTextWrapPos();
        ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
        if(ImGui::MenuItem("New workbook","Ctrl+N")) action_=Action::New;
        if(ImGui::MenuItem("Open...","Ctrl+O")) action_=Action::Open;
        ImGui::Separator();
        if(ImGui::MenuItem("Save","Ctrl+S")) action_=Action::Save;
        if(ImGui::MenuItem("Save As...","Ctrl+Shift+S")) action_=Action::SaveAs;
        ImGui::Spacing();ImGui::Separator();
        if(ImGui::MenuItem("Import CSV...")) action_=Action::ImportCsv;
        if(ImGui::MenuItem("Export CSV (values)...")) action_=Action::ExportCsv;
        ImGui::Separator();
        if(ImGui::MenuItem("Import Excel (cached values)...")){xlsx_formulas_=false;action_=Action::ImportXlsx;}
        if(ImGui::MenuItem("Import Excel (supported formulas)...")){xlsx_formulas_=true;action_=Action::ImportXlsx;}
        if(ImGui::MenuItem("Export Excel (.xlsx)...")) action_=Action::ExportXlsx;
        ImGui::Separator();
        ImGui::TextDisabled("Julretsu Workbook (.julretsu)");
        ImGui::TextDisabled("Keeps cells, formulas, row formatting and Lua scripts.");
        ImGui::EndPopup();
    }
    ImGui::SameLine();

    const char* tabs[]{"Home","Format","Formulas","Review","View","Help"};
    for(int i=0;i<6;++i) {
        if(i) ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4{0,0,0,0}); ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
        if(ImGui::Button(tabs[i],{90*scale_,30*scale_})) { ribbon_tab_=i; ribbon_collapsed_=false; }
        ImGui::PopStyleVar();
        if(i==ribbon_tab_) { auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax(); ImGui::GetWindowDrawList()->AddLine({a.x+12*scale_,b.y},{b.x-12*scale_,b.y},ImGui::ColorConvertFloat4ToU32(accent),2*scale_); }
        // The pushed color corresponds to the tab that was selected before the click.
        ImGui::PopStyleColor();
    }
    {
        // Ribbon and workbook window controls, as beside Excel's ribbon tabs.
        const float w=30*scale_, spacing=ImGui::GetStyle().ItemSpacing.x;
        ImGui::SameLine(std::max(ImGui::GetCursorPosX(),ImGui::GetWindowContentRegionMax().x-5*w-4*spacing-12*scale_));
        if(icon_button("ribbon-toggle",ribbon_collapsed_?Glyph::ChevronDown:Glyph::ChevronUp,scale_,ribbon_collapsed_?"Show the ribbon":"Collapse the ribbon")) ribbon_collapsed_=!ribbon_collapsed_;
        ImGui::SameLine(); if(icon_button("help",Glyph::Help,scale_,"Help: shortcuts and formulas")) guide_popup=true;
        const bool floating=workbook_floating_&&!workbook_minimized_;
        ImGui::SameLine(0,12*scale_); if(icon_button("workbook-minimize",Glyph::WinMin,scale_,"Minimize workbook")) workbook_minimized_=true;
        ImGui::SameLine(); if(icon_button("workbook-restore",floating?Glyph::WinMax:Glyph::WinRestore,scale_,floating?"Maximize workbook":"Restore workbook window")) { workbook_minimized_=false; workbook_floating_=!floating; }
        ImGui::SameLine(); if(icon_button("workbook-close",Glyph::WinClose,scale_,"Close workbook")) pending_=[this]{ close_workbook(); };
    }
    ImGui::Spacing();
    if(!ribbon_collapsed_) {
    ImGui::BeginChild("Ribbon",{0,(ribbon_tab_==0?114:96)*scale_},ImGuiChildFlags_Borders,ribbon_tab_==0?ImGuiWindowFlags_HorizontalScrollbar:ImGuiWindowFlags_NoScrollbar);

    if(ribbon_tab_==0) {
        draw_home_ribbon(guide_popup,settings_popup);
    } else if(ribbon_tab_==1) {
        if(ImGui::Button("Format selected cells...")){format_style_=sheet_.cell_style(active_);format_open_=true;}
        ImGui::SameLine();ImGui::TextDisabled("Font, color, borders, alignment and number formats");
        if(ImGui::Button("Bold rows")) action_=Action::Bold; ImGui::SameLine();
        if(ImGui::Button("Green fill")) action_=Action::Tint; ImGui::SameLine();
        if(ImGui::Button("Fewer decimals")) action_=Action::DecimalsLess; ImGui::SameLine();
        if(ImGui::Button("More decimals")) action_=Action::DecimalsMore; ImGui::SameLine();
        if(ImGui::Button("Reset formatting")) action_=Action::ResetStyle;
        ImGui::TextDisabled("Formatting applies across each selected row and is saved with the workbook.");
    } else if(ribbon_tab_==2) {
        if(ImGui::Button("Sum below")) action_=Action::QuickSum; ImGui::SameLine();
        if(ImGui::Button("Average below")) action_=Action::QuickAverage; ImGui::SameLine();
        if(ImGui::Button("Formula reference")) guide_popup=true;
        ImGui::SameLine(); if(ImGui::Button("Open Lua workspace")) show_scripts(true);
        ImGui::SameLine(); if(ImGui::Button("Ask AI for formulas")) show_ai(true);
        ImGui::Spacing(); ImGui::TextDisabled("SUM  /  AVERAGE  /  IF  /  Lua-powered formulas");
    } else if(ribbon_tab_==3) {
        if(ribbon_button("Check sheet",Glyph::Find,scale_,100)) show_health(!health_open_);
        ImGui::SameLine(); if(ribbon_button("Cell history",Glyph::Undo,scale_,100)) { history_cell_=active_; history_open_=true; }
        ImGui::SameLine(); if(ribbon_button("Change log",Glyph::Book,scale_,100)) changelog_open_=true;
        ImGui::SameLine(0,20*scale_); ImGui::BeginGroup();
        ImGui::Checkbox("Review large or risky changes before keeping them",&review_enabled_);
        ImGui::BeginDisabled(!review_enabled_);
        ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted("Ask when at least"); ImGui::SameLine(); ImGui::SetNextItemWidth(130*scale_);
        if(ImGui::InputInt("##review-threshold",&review_threshold_,10,50)) review_threshold_=std::clamp(review_threshold_,5,1000);
        ImGui::SameLine(); ImGui::TextUnformatted("cells change at once");
        ImGui::EndDisabled();
        ImGui::TextDisabled("Sorts that leave columns behind, deleted data, replaced formulas and out-of-scale numbers are always reviewed.");
        ImGui::EndGroup();
    } else if(ribbon_tab_==4) {
        if(ImGui::Button(dark_?"Use light appearance":"Use dark appearance")) set_dark(!dark_,remember_theme_);
        ImGui::SameLine(); if(ImGui::Button("First cell")) jump({0,0});
        ImGui::SameLine(); if(ImGui::Button("Used range")) select_used();
        ImGui::SameLine(); ImGui::Checkbox("Gridlines",&gridlines_);
        ImGui::SameLine(); ImGui::Checkbox("Show formulas",&show_formulas_);
        ImGui::SameLine(); ImGui::Checkbox("Long text spills into empty cells",&text_overflow_);
        ImGui::Spacing(); ImGui::TextDisabled("Scroll with the mouse wheel. Hold Shift to scroll across columns.");
    } else {
        if(ImGui::Button("Keyboard shortcuts & formulas")) guide_popup=true;
        ImGui::SameLine(); if(ImGui::Button("About Julretsu")) ImGui::OpenPopup("About Julretsu");
        if(ImGui::BeginPopup("About Julretsu")) {
#ifndef JULRETSU_VERSION
#define JULRETSU_VERSION "development"
#endif
            ImGui::Text("Julretsu %s",JULRETSU_VERSION);
            ImGui::TextDisabled("Copyright (c) 2026 Matthew Menchinton & Circuitspecter Studio");
            ImGui::TextDisabled("Free to use under the Julretsu Software License. Not open source.");
            ImGui::EndPopup();
        }
        ImGui::Spacing(); ImGui::TextDisabled("Local calculations. Optional AI with your own provider. Your workspace, your way.");
    }
    ImGui::EndChild();
    }
    const float body_y=ImGui::GetCursorPosY();
    const float body_height=std::max(240*scale_,vp->WorkSize.y-body_y-50*scale_);
    ImGui::BeginChild("Navigation",{190*scale_,body_height},0,ImGuiWindowFlags_NoScrollbar);
    ImGui::Spacing(); ImGui::TextUnformatted("Workbook"); ImGui::Spacing(); ImGui::Spacing();
    auto navigation=[&](const char* text,Glyph icon,bool selected=false) {
        auto p=ImGui::GetCursorScreenPos();
        const bool clicked=ImGui::InvisibleButton(text,{ImGui::GetContentRegionAvail().x,40*scale_});
        auto* d=ImGui::GetWindowDrawList();
        if(selected||ImGui::IsItemHovered()) d->AddRectFilled(p,{p.x+ImGui::GetItemRectSize().x,p.y+40*scale_},ImGui::GetColorU32(selected?ImGuiCol_Header:ImGuiCol_ButtonHovered),6*scale_);
        glyph(d,{p.x+12*scale_,p.y+11*scale_},18*scale_,ImGui::GetColorU32(ImGuiCol_Text),icon);
        d->AddText({p.x+45*scale_,p.y+9*scale_},ImGui::GetColorU32(ImGuiCol_Text),text); return clicked;
    };
    if(navigation("Sheets",Glyph::Sheet,!scripts_open_&&!ai_open_)) { scripts_open_=false; ai_open_=false; grid_focused_=true; }
    if(navigation("Templates",Glyph::Book)) template_popup=true;
    if(navigation("Lua workspace",Glyph::Code,scripts_open_)) show_scripts(!scripts_open_);
    if(navigation("AI assistant",Glyph::Ai,ai_open_)) show_ai(!ai_open_);
    if(navigation("Settings",Glyph::Settings)) settings_popup=true;    ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY()+12*scale_,body_height-220*scale_));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,ImGui::GetStyleColorVec4(ImGuiCol_Header)); ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{16*scale_,14*scale_}); ImGui::BeginChild("Tips",{0,206*scale_},ImGuiChildFlags_Borders);
    ImGui::PopStyleVar(); ImGui::PopStyleColor(); ImGui::TextColored(accent,"Work smarter"); ImGui::TextColored(accent,"with Julretsu");
    ImGui::Spacing(); ImGui::TextWrapped("Small ideas. Powerful formulas. A little help to do more.");
    ImGui::Spacing(); if(ImGui::Button("Explore",{-1,30*scale_})) guide_popup=true;
    ImGui::EndChild(); ImGui::EndChild();
    ImGui::SameLine();
    const ImVec2 area_pos=ImGui::GetCursorScreenPos(), area_size{ImGui::GetContentRegionAvail().x,body_height};
    const bool minimized=workbook_minimized_, floating=workbook_floating_&&!minimized;
    if(minimized||floating) {
        // The empty workspace behind a restored or minimised workbook, like Excel's grey application area.
        ImGui::GetWindowDrawList()->AddRectFilled(area_pos,{area_pos.x+area_size.x,area_pos.y+area_size.y},ImGui::GetColorU32(ImGuiCol_FrameBg),8*scale_);
        if(minimized) draw_minimized_workbook(area_pos.x,area_pos.y,area_size.x,area_size.y);
        ImGui::SetCursorScreenPos(area_pos); ImGui::Dummy(area_size);
    }
    if(floating) {
        ImGui::SetNextWindowPos({area_pos.x+30*scale_,area_pos.y+24*scale_},ImGuiCond_Appearing);
        ImGui::SetNextWindowSize({area_size.x*.8f,area_size.y*.84f},ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints({480*scale_,300*scale_},area_size);
        char title[320]; std::snprintf(title,sizeof title,"%s###Workbook window",workbook_name_.c_str());
        bool open=true;
        ImGui::Begin(title,&open,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        if(!open) pending_=[this]{ close_workbook(); };
        // Keep the restored workbook inside the workspace, as Excel does.
        auto position=ImGui::GetWindowPos(); const auto size=ImGui::GetWindowSize(); const auto before=position;
        position.x=std::clamp(position.x,area_pos.x,std::max(area_pos.x,area_pos.x+area_size.x-size.x));
        position.y=std::clamp(position.y,area_pos.y,std::max(area_pos.y,area_pos.y+area_size.y-size.y));
        if(position.x!=before.x||position.y!=before.y) ImGui::SetWindowPos(position);
    } else if(!minimized) ImGui::BeginChild("Worksheet area",{0,body_height},0,ImGuiWindowFlags_NoScrollbar);
    if(!minimized) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,1); ImGui::SetNextItemWidth(125*scale_);
    if(ImGui::InputText("##address",address_.data(),address_.size(),ImGuiInputTextFlags_EnterReturnsTrue)) {
        auto parsed=from_a1(address_.data());
        if(auto c=std::get_if<CellCoord>(&parsed)) { row_selection_.clear(); jump(*c); }
        else message_="Enter a coordinate from A1 through XFD1000000.";
    }
    ImGui::SameLine(); ImGui::TextColored(accent,"fx"); ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(60.0f,ImGui::GetContentRegionAvail().x-78*scale_));
    if(ImGui::InputText("##formula",editor_.data(),editor_.size(),ImGuiInputTextFlags_EnterReturnsTrue|(oversized_?ImGuiInputTextFlags_ReadOnly:0))) queue_edit();
    if(ImGui::IsItemEdited()) editor_dirty_=true;
    remember_target(FormulaField); ImGui::PopStyleVar();
    if(ImGui::IsItemActive()) { grid_focused_=false; if(ImGui::IsKeyPressed(ImGuiKey_Escape)) { selection_changed_=true; grid_focused_=true; } }
    ImGui::SameLine(); if(ImGui::Button("Apply",{68*scale_,0})) queue_edit();
    const float height=std::max(100.0f,ImGui::GetContentRegionAvail().y-48*scale_);
    const float width=ImGui::GetContentRegionAvail().x-(scripts_open_?362*scale_:(ai_open_||health_open_)?412*scale_:0);
    draw_grid(width,height);
    if(scripts_open_) { ImGui::SameLine(); draw_script_panel(height); }
    else if(ai_open_) { ImGui::SameLine(); draw_ai_panel(height); }
    else if(health_open_) { ImGui::SameLine(); draw_health_panel(height); }
    ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_Header));
    if(ImGui::Button(sheets_.empty()?"Sheet 1":sheets_[current_sheet_].name.c_str(),{110*scale_,32*scale_}))ImGui::OpenPopup("Choose worksheet");
    if(ImGui::BeginPopup("Choose worksheet")) {for(std::size_t i=0;i<sheets_.size();++i)if(ImGui::MenuItem(sheets_[i].name.c_str(),nullptr,i==current_sheet_))switch_sheet_=int(i);ImGui::Separator();if(ImGui::MenuItem("Manage sheets..."))sheets_open_=true;ImGui::EndPopup();}
    ImGui::PopStyleColor(); ImGui::SameLine();
    ImGui::TextDisabled("%s",selection_stats_.data());
    ImGui::SameLine(); ImGui::SetNextItemWidth(std::max(80*scale_,width-520*scale_));
    ImGui::SliderInt("##scrollcolumn",&viewport_.first_column,0,std::max(0,int(max_columns)-viewport_.visible_columns),"Column %d",ImGuiSliderFlags_AlwaysClamp);
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Horizontal position (zero-based). Shift + wheel also scrolls columns.");
    if(floating) ImGui::End(); else ImGui::EndChild();
    }
    ImGui::Separator(); ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%zu cells | Sheet %zu of %zu",populated_,current_sheet_+1,sheets_.size());
    {
        // Sheet check badge: always visible, one click opens the panel.
        const auto issues=visible_health_count(); char badge[48];
        if(issues) std::snprintf(badge,sizeof badge,"Sheet check: %zu issue%s",issues,issues==1?"":"s"); else std::snprintf(badge,sizeof badge,"Sheet check: OK");
        ImGui::SameLine(); ImGui::PushStyleColor(ImGuiCol_Text,issues?ImVec4{0.85f,0.52f,0.08f,1}:(dark_?ImVec4{0.40f,0.85f,0.69f,1}:ImVec4{0.03f,0.43f,0.32f,1}));
        if(ImGui::SmallButton(badge)) show_health(!health_open_);
        ImGui::PopStyleColor();
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Checks formulas, totals, out-of-scale numbers, numbers stored as text and blank rows");
    }
    ImGui::SameLine(); ImGui::TextDisabled("|"); ImGui::SameLine();
    const float status_width=std::max(80.0f,ImGui::GetContentRegionAvail().x-180*scale_);
    ImGui::BeginChild("Status",{status_width,28*scale_},0,ImGuiWindowFlags_NoScrollbar);
    ImGui::TextUnformatted(message_.c_str()); ImGui::EndChild();
    ImGui::SameLine(); ImGui::SetNextItemWidth(155*scale_);
    int zoom=int(zoom_*100+0.5f);
    if(ImGui::SliderInt("##zoom",&zoom,75,150,"%d%%",ImGuiSliderFlags_AlwaysClamp)) { zoom_=float(zoom)/100; set_scale(scale_); viewport_.reveal(active_); }
    if(new_popup) action_=Action::New;
    if(template_popup) ImGui::OpenPopup("Templates");
    if(guide_popup) ImGui::OpenPopup("Formula guide");
    if(settings_popup) ImGui::OpenPopup("Settings");
    if(ImGui::BeginPopupModal("New workbook?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Discard this in-memory workbook and start a blank sheet?");
        if(ImGui::Button("Start blank")) { action_=Action::New; ImGui::CloseCurrentPopup(); }
        ImGui::SameLine(); if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup(); ImGui::EndPopup();
    }
    if(ImGui::BeginPopupModal("Templates",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        const char* templates[]={"Project budget","Monthly sales","Household expenses","Inventory planning","Weekly project hours"};
        ImGui::Combo("Sample workbook",&template_choice_,templates,5);
        ImGui::TextWrapped("Fictional sample data with formulas. Use Reports to chart the numbers.");
        ImGui::Spacing(); ImGui::TextUnformatted("You can save the current workbook before loading this template.");
        if(ImGui::Button("Use selected template")) { action_=Action::Demo; ImGui::CloseCurrentPopup(); }
        ImGui::SameLine(); if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup(); ImGui::EndPopup();
    }
    if(ImGui::BeginPopup("Settings")) {
        ImGui::TextUnformatted("Appearance"); bool dark=dark_;
        if(ImGui::Checkbox("Dark mode",&dark)) set_dark(dark,remember_theme_);
        ImGui::TextDisabled("Your appearance preference is remembered on this device.");
        ImGui::Separator(); ImGui::TextUnformatted("Save workbooks locally with File > Save or Ctrl+S.");
        ImGui::Separator(); ImGui::TextUnformatted("AI assistant");
        ImGui::TextDisabled("Optional. Connect your own provider in the AI assistant panel.");
        if(ImGui::Button("Open AI assistant")) { show_ai(true); ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
    if(ImGui::BeginPopup("Formula guide")) {
        ImGui::TextUnformatted("=SUM(A1:A10)\n=AVERAGE(B1:B5)\n=IF(C1>0,C1*2,0)\n=LUA(\"return cell('A1') * 2\")");
        ImGui::Separator(); ImGui::TextUnformatted("F2 / double-click: edit   Enter: commit   Escape: cancel\nShift-click: range   Ctrl-click row headers: multiple rows\nCtrl+End: final cell   Ctrl+Home: first cell\nWheel: rows   Shift+wheel: columns\nCtrl+K: go to cell   Ctrl+Z / Ctrl+Y: undo / redo\nPrefix an apostrophe to force literal text.");
        ImGui::EndPopup();
    }
    ImGui::End(); ImGui::PopStyleVar();
}

}
