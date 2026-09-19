#include "julretsu/GridUI.hpp"
#include "julretsu/WorkbookFile.hpp"
#include "julretsu/Csv.hpp"
#include "julretsu/Xlsx.hpp"
#include <sstream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
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
enum class Glyph { Sheet, Undo, Redo, Bold, Fill, Reset, Clear, Function, Code, Settings, Book, Sun, Moon, Plus, Ai };
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
    else {
        const char* text=kind==Glyph::Bold?"B":kind==Glyph::Function?"fx":kind==Glyph::Ai?"AI":"J";
        d->AddText(ImGui::GetFont(),size,point(.08f,-.07f),color,text);
    }
}
bool ribbon_button(const char* text,Glyph icon,float scale,float width=76) {
    ImGui::PushID(text); const auto p=ImGui::GetCursorScreenPos();
    const bool clicked=ImGui::InvisibleButton("button",{width*scale,65*scale});
    auto* d=ImGui::GetWindowDrawList();
    if(ImGui::IsItemHovered()) d->AddRectFilled(p,{p.x+width*scale,p.y+65*scale},ImGui::GetColorU32(ImGuiCol_ButtonHovered),5*scale);
    glyph(d,{p.x+(width-25)*scale/2,p.y+5*scale},25*scale,ImGui::GetColorU32(ImGuiCol_Text),icon);
    const auto size=ImGui::CalcTextSize(text);
    d->AddText({p.x+(width*scale-size.x)/2,p.y+40*scale},ImGui::GetColorU32(ImGuiCol_Text),text);
    ImGui::PopID(); return clicked;
}

Input interpret(std::string_view text) {
    if(text.empty()) return std::monostate{};
    if(text.front()=='\'') return std::string(text.substr(1));
    if(text.front()=='=') return FormulaInput{std::string(text)};
    if(text=="TRUE"||text=="true") return true;
    if(text=="FALSE"||text=="false") return false;
    double value{};
    auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
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
}
void GridUI::load_demo() {
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
std::optional<std::filesystem::path> workbook_dialog(bool save,const std::filesystem::path& current,void* owner,const wchar_t* extension=L"julretsu") {
#ifdef _WIN32
    std::array<wchar_t,32768> name{};
    const auto initial=current.empty()?(std::wstring(L"Untitled workbook.")+extension):current.wstring();
    std::copy_n(initial.c_str(),std::min(initial.size(),name.size()-1),name.data());
    OPENFILENAMEW dialog{}; dialog.lStructSize=sizeof(dialog); dialog.hwndOwner=static_cast<HWND>(owner);
    dialog.lpstrFilter=std::wstring_view(extension)==L"xlsx"?L"Excel Workbook (*.xlsx)\0*.xlsx\0\0":std::wstring_view(extension)==L"csv"?L"CSV (UTF-8) (*.csv)\0*.csv\0\0":L"Julretsu Workbook (*.julretsu)\0*.julretsu\0\0";
    dialog.lpstrFile=name.data(); dialog.nMaxFile=DWORD(name.size()); dialog.lpstrDefExt=extension;
    dialog.lpstrTitle=save?L"Save workbook locally":L"Open a Julretsu workbook";
    dialog.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    if(save?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog)) return std::filesystem::path(name.data());
    if(CommDlgExtendedError()) throw std::runtime_error("Windows could not open the file dialog. Please try again.");
    return std::nullopt;
#else
    (void)save;(void)current;(void)owner;(void)extension;
    throw std::runtime_error("Native file dialogs are currently supported on Windows.");
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
        write_workbook(path,sheet_,script_.data());
        file_path_=path; workbook_name_=path_label(path.filename()); file_label_=path_label(path);
        modified_=false; editor_dirty_=false; message_="Saved to this device."; return true;
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
        auto workbook=read_workbook(path);
        Limits limits; limits.batch_edits=max_rows;
        Sheet replacement(limits,&lua_); auto result=replacement.apply(workbook.data);
        if(!result.accepted) throw std::runtime_error(result.error->context);
        // Install only after complete parsing and validation; an invalid file cannot erase work.
        replacement.clear_history(); sheet_=std::move(replacement); queued_={}; row_selection_.clear();
        std::snprintf(script_.data(),script_.size(),"%s",workbook.script.c_str());
        file_path_=path; workbook_name_=path_label(path.filename()); file_label_=path_label(path);
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
    message_="Save the workbook before replacing or closing it.";return false;
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
            Sheet replacement({},&lua_);CsvAdapter adapter;StreamOptions options;options.interpretation=ImportInterpretation::Values;
            auto result=adapter.import_sheet(in,replacement,options);if(result.error)throw std::runtime_error(result.error->context);
            replacement.clear_history();sheet_=std::move(replacement);file_path_.clear();workbook_name_="Imported CSV";file_label_.clear();script_[0]=0;
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
            if(!confirm_close())return;Sheet replacement({},&lua_);auto result=replacement.apply(imported.data);if(!result.accepted)throw std::runtime_error(result.error->context);
            replacement.clear_history();sheet_=std::move(replacement);file_path_.clear();workbook_name_="Imported Excel workbook";file_label_.clear();script_[0]=0;
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

void GridUI::prepare() {
    try {
        ai_poll();
        if(!queued_.cells.empty()) {
            const auto result=sheet_.apply(queued_);
            message_=result.accepted?"Changes applied.":result.error->context;
            modified_|=result.accepted; queued_={}; selection_changed_=true;
        }
        switch(action_) {
        case Action::Undo: if(sheet_.undo()) { modified_=true; message_="Undo complete."; } break;
        case Action::Redo: if(sheet_.redo()) { modified_=true; message_="Redo complete."; } break;
        case Action::Clear:clear_selection(); break;
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
        case Action::New:if(!confirm_close()) break; file_path_.clear(); workbook_name_="Untitled workbook"; file_label_.clear(); editor_dirty_=false; sheet_=Sheet({},&lua_); select({0,0}); row_selection_.clear(); modified_=false; message_="New workbook. Press Ctrl+S to save locally."; break;
        case Action::Demo:if(confirm_close()) load_demo(); break;
        default:break;
        }
        if(action_!=Action::None) { if(action_!=Action::Save&&action_!=Action::SaveAs&&action_!=Action::Open&&action_!=Action::New&&action_!=Action::Demo&&action_!=Action::ImportCsv&&action_!=Action::ExportCsv&&action_!=Action::ImportXlsx&&action_!=Action::ExportXlsx&&action_!=Action::AiSend) selection_changed_=true; action_=Action::None; }
        if(counted_revision_!=sheet_.revision()) {
            populated_=sheet_.populated_cells(); counted_revision_=sheet_.revision();
        }
        if(selection_changed_) refresh_editor();
    } catch(const std::exception& error) { message_=error.what(); queued_={}; action_=Action::None; }
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
    const auto& io=ImGui::GetIO();
    if(hovered&&!io.WantTextInput) {
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
    for(int c=0;c<=viewport_.visible_columns&&viewport_.first_column+c<int(max_columns);++c) {
        const float x=origin.x+header+float(c)*cw;
        if(unsigned(viewport_.first_column+c)==active_.column) {
            draw->AddRectFilled({x,origin.y},{x+cw,origin.y+ch},dark_?IM_COL32(35,74,64,255):IM_COL32(212,239,229,255));
            draw->AddLine({x,origin.y+ch-1},{x+cw,origin.y+ch-1},IM_COL32(24,153,116,255),2*scale_);
        }
        label({0,unsigned(viewport_.first_column+c)},colname,sizeof colname,false);
        draw->AddText({x+cw/2-5*scale_,origin.y+8*scale_},(dark_?IM_COL32(163,181,194,255):IM_COL32(99,111,123,255)),colname);
        draw->AddLine({x,origin.y},{x,end.y},(dark_?IM_COL32(48,61,73,255):IM_COL32(224,232,237,255)));
    }
    for(int r=0;r<=viewport_.visible_rows&&viewport_.first_row+r<int(max_rows);++r) {
        const auto row=unsigned(viewport_.first_row+r); const auto style=sheet_.row_style(row);
        const float y=origin.y+ch+float(r)*rh;
        bool selected_row=false; for(auto interval:row_selection_) if(row>=interval.first&&row<=interval.last) selected_row=true;
        if(row==active_.row) draw->AddRectFilled({origin.x,y},{origin.x+header,y+rh},dark_?IM_COL32(35,74,64,255):IM_COL32(212,239,229,255));
        std::snprintf(buffer,sizeof buffer,"%u",row+1);
        draw->AddText({origin.x+7*scale_,y+8*scale_},(dark_?IM_COL32(163,181,194,255):IM_COL32(99,111,123,255)),buffer);
        for(int c=0;c<=viewport_.visible_columns&&viewport_.first_column+c<int(max_columns);++c) {
            const auto column=unsigned(viewport_.first_column+c);
            const float x=origin.x+header+float(c)*cw;
            const bool selected=selected_row||(row>=top&&row<=bottom&&column>=left&&column<=right);
            draw->AddRectFilled({x+1,y+1},{x+cw,y+rh},selected?(dark_?IM_COL32(31,67,60,255):IM_COL32(229,244,238,255)):(dark_?(style.background==0xFFFFFFFF?IM_COL32(25,34,43,255):IM_COL32(29,63,56,255)):rgba(style.background)));
        }
        for(int c=0;c<=viewport_.visible_columns&&viewport_.first_column+c<int(max_columns);++c) {
            const auto column=unsigned(viewport_.first_column+c);
            const float x=origin.x+header+float(c)*cw;
            const auto* cell=sheet_.cell({row,column});
            if(cell) {
                const char* value=format_value(cell->cached,buffer,sizeof buffer,style.decimals);
                auto color=std::holds_alternative<CellError>(cell->cached)?IM_COL32(173,50,63,255):(dark_?(style.foreground==0x155E4BFF?IM_COL32(132,220,190,255):IM_COL32(221,230,237,255)):rgba(style.foreground));
                const float tx=std::holds_alternative<double>(cell->cached)?std::max(x+8*scale_,x+cw-10*scale_-ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize()*zoom_,FLT_MAX,0,value).x):x+9*scale_;
                float text_right=x+cw-5*scale_;
                if(std::holds_alternative<std::string>(cell->cached)) {
                    for(unsigned next=column+1;next<max_columns&&text_right<end.x;++next) {
                        if(sheet_.cell({row,next})) break;
                        text_right+=cw;
                    }
                }
                draw->PushClipRect({x+5*scale_,y+1},{text_right,y+rh-1},true);
                if(std::strcmp(value,"Planned")==0||std::strcmp(value,"Review")==0) {
                    const bool review=std::strcmp(value,"Review")==0;
                    const auto badge=review?(dark_?IM_COL32(88,63,27,255):IM_COL32(255,234,186,255)):(dark_?IM_COL32(30,78,63,255):IM_COL32(214,241,229,255));
                    color=review?(dark_?IM_COL32(255,211,139,255):IM_COL32(144,87,15,255)):(dark_?IM_COL32(151,231,196,255):IM_COL32(17,106,78,255));
                    draw->AddRectFilled({tx-3*scale_,y+5*scale_},{tx+ImGui::CalcTextSize(value).x*zoom_+13*scale_,y+rh-5*scale_},badge,14*scale_);
                }
                draw->AddText(ImGui::GetFont(),ImGui::GetFontSize()*zoom_,{tx+(std::strcmp(value,"Planned")==0||std::strcmp(value,"Review")==0?5*scale_:0),y+8*scale_*zoom_},color,value);
                if(style.bold) draw->AddText(ImGui::GetFont(),ImGui::GetFontSize()*zoom_,{tx+0.5f*scale_,y+8*scale_*zoom_},color,value);
                draw->PopClipRect();
            }
            if(CellCoord{row,column}==active_) draw->AddRect({x+1,y+1},{x+cw-1,y+rh-1},(dark_?IM_COL32(89,211,171,255):IM_COL32(15,148,109,255)),0,0,2*scale_);
        }
        draw->AddLine({origin.x,y},{end.x,y},(dark_?IM_COL32(48,61,73,255):IM_COL32(224,232,237,255)));
    }
    draw->AddLine({origin.x+header,origin.y},{origin.x+header,end.y},(dark_?IM_COL32(56,71,83,255):IM_COL32(214,222,226,255)));
    draw->AddLine({origin.x,origin.y+ch},{end.x,origin.y+ch},(dark_?IM_COL32(56,71,83,255):IM_COL32(214,222,226,255)));
    draw->PopClipRect();
    if(editing_&&!oversized_&&!selection_changed_) {
        const float x=origin.x+header+float(int(active_.column)-viewport_.first_column)*cw;
        const float y=origin.y+ch+float(int(active_.row)-viewport_.first_row)*rh;
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
void GridUI::draw() {
    auto* vp=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos); ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{12*scale_,12*scale_});
    ImGui::Begin("Julretsu workspace",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar);
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
    if(ImGui::GetIO().KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_K)) ImGui::SetKeyboardFocusHere();
    if(ImGui::InputTextWithHint("##jump","Go to cell  (Ctrl + K)",search_.data(),search_.size(),ImGuiInputTextFlags_EnterReturnsTrue)) {
        auto parsed=from_a1(search_.data());
        if(auto c=std::get_if<CellCoord>(&parsed)) { jump(*c); grid_focused_=true; search_[0]=0; }
        else message_="Enter a cell address, such as C4 or XFD1000000.";
    }
    ImGui::SameLine(vp->WorkSize.x-140*scale_);
    if(ImGui::Button(dark_?"Light mode":"Dark mode",{120*scale_,0})) set_dark(!dark_,remember_theme_);
    remember_target(ThemeButton);
    ImGui::Spacing();
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

    const char* tabs[]{"Home","Format","Formulas","View","Help"};
    for(int i=0;i<5;++i) {
        if(i) ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4{0,0,0,0});
        if(ImGui::Button(tabs[i],{90*scale_,30*scale_})) ribbon_tab_=i;
        if(i==ribbon_tab_) { auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax(); ImGui::GetWindowDrawList()->AddLine({a.x+12*scale_,b.y},{b.x-12*scale_,b.y},ImGui::ColorConvertFloat4ToU32(accent),2*scale_); }
        // The pushed color corresponds to the tab that was selected before the click.
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    ImGui::BeginChild("Ribbon",{0,96*scale_},ImGuiChildFlags_Borders,ImGuiWindowFlags_NoScrollbar);

    if(ribbon_tab_==0||ribbon_tab_==1) {
        auto item=[&](const char* title,Glyph icon,Action action,float width=76) {
            if(ribbon_button(title,icon,scale_,width)) action_=action;
            ImGui::SameLine();
        };
        auto divider=[&]() { auto p=ImGui::GetCursorScreenPos(); ImGui::GetWindowDrawList()->AddLine({p.x+5*scale_,p.y},{p.x+5*scale_,p.y+65*scale_},ImGui::GetColorU32(ImGuiCol_Border)); ImGui::Dummy({14*scale_,65*scale_}); ImGui::SameLine(); };
        if(ribbon_button("New",Glyph::Sheet,scale_,64)) new_popup=true; ImGui::SameLine(); divider();
        item("Undo",Glyph::Undo,Action::Undo,64); item("Redo",Glyph::Redo,Action::Redo,64); divider();
        if(ribbon_button("Bold",Glyph::Bold,scale_,64)) action_=Action::Bold;
        remember_target(BoldButton); ImGui::SameLine();
        item("Fill color",Glyph::Fill,Action::Tint); item("Reset",Glyph::Reset,Action::ResetStyle,64); divider();
        ImGui::BeginGroup();
        ImGui::TextDisabled("Number format");
        if(ImGui::Button(".0",{44*scale_,28*scale_})) action_=Action::DecimalsLess;
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Show fewer decimal places in selected rows");
        ImGui::SameLine(); if(ImGui::Button(".00",{44*scale_,28*scale_})) action_=Action::DecimalsMore;
        if(ImGui::IsItemHovered()) ImGui::SetTooltip("Show more decimal places in selected rows");
        ImGui::EndGroup(); ImGui::SameLine(); divider();
        item("Clear",Glyph::Clear,Action::Clear,64); divider();
        if(ribbon_button("Functions",Glyph::Function,scale_,92)) guide_popup=true;
        ImGui::SameLine(); if(ribbon_button("Lua scripts",Glyph::Code,scale_,96)) show_scripts(!scripts_open_);
        ImGui::SameLine(); if(ribbon_button("Ask AI",Glyph::Ai,scale_,76)) show_ai(!ai_open_);
        if(vp->WorkSize.x>1200*scale_) { ImGui::SameLine(); divider(); if(ribbon_button("Appearance",Glyph::Sun,scale_,110)) settings_popup=true; }    } else if(ribbon_tab_==2) {
        if(ImGui::Button("Formula reference")) guide_popup=true;
        ImGui::SameLine(); if(ImGui::Button("Open Lua workspace")) show_scripts(true);
        ImGui::SameLine(); if(ImGui::Button("Ask AI for formulas")) show_ai(true);
        ImGui::Spacing(); ImGui::TextDisabled("SUM  /  AVERAGE  /  IF  /  Lua-powered formulas");
    } else if(ribbon_tab_==3) {
        if(ImGui::Button(dark_?"Use light appearance":"Use dark appearance")) set_dark(!dark_,remember_theme_);
        ImGui::SameLine(); if(ImGui::Button("First cell")) jump({0,0});
        ImGui::SameLine(); if(ImGui::Button("Last cell")) jump({max_rows-1,max_columns-1});
        ImGui::Spacing(); ImGui::TextDisabled("Scroll with the mouse wheel. Hold Shift to scroll across columns.");
    } else {
        if(ImGui::Button("Keyboard shortcuts & formulas")) guide_popup=true;
        ImGui::Spacing(); ImGui::TextDisabled("Local calculations. Optional AI with your own provider. Your workspace, your way.");
    }
    ImGui::EndChild();
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
    ImGui::BeginChild("Worksheet area",{0,body_height},0,ImGuiWindowFlags_NoScrollbar);
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
    const float width=ImGui::GetContentRegionAvail().x-(scripts_open_?362*scale_:ai_open_?412*scale_:0);
    draw_grid(width,height);
    if(scripts_open_) { ImGui::SameLine(); draw_script_panel(height); }
    else if(ai_open_) { ImGui::SameLine(); draw_ai_panel(height); }
    ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_Header));
    if(ImGui::Button("Sheet 1",{110*scale_,32*scale_})) grid_focused_=true;
    ImGui::PopStyleColor(); ImGui::SameLine();
    ImGui::TextDisabled("1 of 1");
    ImGui::SameLine(); ImGui::SetNextItemWidth(std::max(80*scale_,width-290*scale_));
    ImGui::SliderInt("##scrollcolumn",&viewport_.first_column,0,std::max(0,int(max_columns)-viewport_.visible_columns),"Column %d",ImGuiSliderFlags_AlwaysClamp);
    if(ImGui::IsItemHovered()) ImGui::SetTooltip("Horizontal position (zero-based). Shift + wheel also scrolls columns.");
    ImGui::EndChild();
    ImGui::Separator(); ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%zu cells   |   Sheet 1 of 1",populated_);
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
        ImGui::TextUnformatted("Project budget\nCategories, quantities, costs and automatic totals.");
        ImGui::Spacing(); ImGui::TextUnformatted("You can save the current workbook before loading this template.");
        if(ImGui::Button("Use project budget")) { action_=Action::Demo; ImGui::CloseCurrentPopup(); }
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
