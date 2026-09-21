// Julretsu Windows installer and uninstaller.
// The installer is this program with a zip of the app appended:
//   [setup program][zip][zip size: 8 bytes little endian]["JULSETUP"]
// It installs for the current user only (no administrator rights), registers an uninstaller
// in Settings > Apps, and removes exactly what it installed.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <miniz.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace fs=std::filesystem;
namespace {
#ifndef JULRETSU_VERSION
#define JULRETSU_VERSION "1.0.0"
#endif
const wchar_t* const product=L"Julretsu";
const wchar_t* const publisher=L"Circuitspecter Studio";
const char footer_magic[8]={'J','U','L','S','E','T','U','P'};

std::wstring widen(std::string_view text) {
    if(text.empty()) return {};
    const int n=MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),nullptr,0);
    std::wstring out(std::size_t(n),L'\0'); MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),out.data(),n); return out;
}
std::string narrow(std::wstring_view text) {
    if(text.empty()) return {};
    const int n=WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);
    std::string out(std::size_t(n),'\0'); WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),out.data(),n,nullptr,nullptr); return out;
}
std::wstring version() { return widen(JULRETSU_VERSION); }
fs::path self_path() {
    std::wstring buffer(32768,L'\0');
    const auto n=GetModuleFileNameW(nullptr,buffer.data(),DWORD(buffer.size()));
    buffer.resize(n); return fs::path(buffer);
}
std::string read_file(const fs::path& path) {
    std::ifstream in(path,std::ios::binary); if(!in) throw std::runtime_error("Cannot read "+narrow(path.wstring()));
    return std::string(std::istreambuf_iterator<char>(in),{});
}
void write_file(const fs::path& path,const void* data,std::size_t size) {
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) {
        if(GetLastError()==ERROR_SHARING_VIOLATION) throw std::runtime_error("A Julretsu file is in use. Close Julretsu and try again.");
        throw std::runtime_error("Cannot write "+narrow(path.wstring())+". Choose a folder you can write to.");
    }
    DWORD written=0; const bool ok=WriteFile(file,data,DWORD(size),&written,nullptr)&&written==size;
    CloseHandle(file); if(!ok) throw std::runtime_error("Writing "+narrow(path.wstring())+" failed. Check free disk space.");
}
// The program and payload within this file. The uninstaller is the program part alone.
struct Package { std::string bytes; std::size_t program_size{}, zip_offset{}, zip_size{}; };
std::optional<Package> load_package() {
    Package package; package.bytes=read_file(self_path());
    const auto& b=package.bytes;
    if(b.size()<16||std::memcmp(b.data()+b.size()-8,footer_magic,8)!=0) return std::nullopt;
    std::uint64_t size=0; std::memcpy(&size,b.data()+b.size()-16,8);
    if(size==0||size>b.size()-16) return std::nullopt;
    package.zip_size=std::size_t(size); package.zip_offset=b.size()-16-package.zip_size; package.program_size=package.zip_offset;
    return package;
}
// Install options, and where registry keys and shortcuts go (a test root keeps tests off the real system).
struct Options {
    fs::path folder; bool desktop=true, associate=true, start_menu=true, silent=false; std::wstring test_root;
    std::wstring classes() const { return test_root.empty()?L"Software\\Classes":L"Software\\"+test_root+L"\\Classes"; }
    std::wstring uninstall_key() const { return (test_root.empty()?L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\":L"Software\\"+test_root+L"\\Uninstall\\")+product; }
};
fs::path known_folder(REFKNOWNFOLDERID id) {
    PWSTR raw=nullptr; fs::path result;
    if(SUCCEEDED(SHGetKnownFolderPath(id,0,nullptr,&raw))) result=raw;
    CoTaskMemFree(raw); return result;
}
fs::path shortcut_folder(const Options& o,bool desktop) {
    if(!o.test_root.empty()) return o.folder.parent_path()/(o.test_root+(desktop?L"-desktop":L"-start-menu"));
    return known_folder(desktop?FOLDERID_Desktop:FOLDERID_Programs);
}
fs::path default_folder() { return known_folder(FOLDERID_LocalAppData)/L"Programs"/product; }
void set_string(HKEY key,const wchar_t* name,const std::wstring& value) {
    RegSetValueExW(key,name,0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),DWORD((value.size()+1)*sizeof(wchar_t)));
}
void set_dword(HKEY key,const wchar_t* name,DWORD value) { RegSetValueExW(key,name,0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof value); }
HKEY create_key(const std::wstring& path) {
    HKEY key=nullptr;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,path.c_str(),0,nullptr,0,KEY_WRITE|KEY_READ,nullptr,&key,nullptr)!=ERROR_SUCCESS) throw std::runtime_error("Cannot write the Windows registry settings for Julretsu.");
    return key;
}
std::wstring read_string(const std::wstring& path,const wchar_t* name) {
    wchar_t buffer[4096]{}; DWORD size=sizeof buffer;
    if(RegGetValueW(HKEY_CURRENT_USER,path.c_str(),name,RRF_RT_REG_SZ,nullptr,buffer,&size)!=ERROR_SUCCESS) return {};
    return buffer;
}
void make_shortcut(const fs::path& link,const fs::path& target,const fs::path& icon,const std::wstring& description) {
    IShellLinkW* shell_link=nullptr;
    if(FAILED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_IShellLinkW,reinterpret_cast<void**>(&shell_link)))) throw std::runtime_error("Cannot create shortcuts.");
    shell_link->SetPath(target.c_str()); shell_link->SetWorkingDirectory(target.parent_path().c_str());
    shell_link->SetIconLocation(icon.c_str(),0); shell_link->SetDescription(description.c_str());
    IPersistFile* file=nullptr; HRESULT saved=E_FAIL;
    if(SUCCEEDED(shell_link->QueryInterface(IID_IPersistFile,reinterpret_cast<void**>(&file)))) { saved=file->Save(link.c_str(),TRUE); file->Release(); }
    shell_link->Release();
    if(FAILED(saved)) throw std::runtime_error("Cannot save the shortcut "+narrow(link.wstring()));
}
bool file_locked(const fs::path& path) {
    if(!fs::exists(path)) return false;
    HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return GetLastError()==ERROR_SHARING_VIOLATION;
    CloseHandle(file); return false;
}
// Safe relative path from a zip entry: forward or back slashes, no drive, no "..".
std::optional<fs::path> entry_path(std::string name) {
    std::replace(name.begin(),name.end(),'\\','/');
    if(name.empty()||name.front()=='/'||name.find(':')!=std::string::npos) return std::nullopt;
    fs::path path;
    std::stringstream parts(name); std::string part;
    while(std::getline(parts,part,'/')) { if(part.empty()||part==".") continue; if(part=="..") return std::nullopt; path/=widen(part); }
    if(path.empty()) return std::nullopt;
    return path;
}

// ---- Install ------------------------------------------------------------
void install(const Package& package,const Options& o,const std::function<void(int,const std::wstring&)>& progress) {
    if(o.folder.empty()||!o.folder.is_absolute()) throw std::runtime_error("Choose a full install folder path, such as C:\\Users\\You\\AppData\\Local\\Programs\\Julretsu.");
    const auto exe=o.folder/L"Julretsu.exe";
    if(file_locked(exe)) throw std::runtime_error("Julretsu is running. Close it, then try again.");
    std::error_code error; fs::create_directories(o.folder,error);
    if(error) throw std::runtime_error("Cannot create the install folder. Choose another location.");
    mz_zip_archive zip{};
    if(!mz_zip_reader_init_mem(&zip,package.bytes.data()+package.zip_offset,package.zip_size,0)) throw std::runtime_error("The installer is damaged. Download it again.");
    std::vector<std::wstring> manifest{L"root="+o.test_root};
    const auto count=mz_zip_reader_get_num_files(&zip);
    try {
        for(mz_uint i=0;i<count;++i) {
            mz_zip_archive_file_stat stat{};
            if(!mz_zip_reader_file_stat(&zip,i,&stat)) throw std::runtime_error("The installer is damaged. Download it again.");
            if(mz_zip_reader_is_file_a_directory(&zip,i)) continue;
            auto relative=entry_path(stat.m_filename); if(!relative) throw std::runtime_error("The installer contains an unsafe file name.");
            if(relative->filename()==L"Register-Julretsu.ps1") continue; // the installer registers files itself
            progress(int(90*i/std::max<mz_uint>(count,1)),L"Copying "+relative->wstring());
            std::size_t size=0; void* data=mz_zip_reader_extract_to_heap(&zip,i,&size,0);
            if(!data) throw std::runtime_error("The installer is damaged. Download it again.");
            const auto target=o.folder/ *relative; fs::create_directories(target.parent_path(),error);
            try { write_file(target,data,size); } catch(...) { mz_free(data); throw; }
            mz_free(data); manifest.push_back(relative->wstring());
        }
    } catch(...) { mz_zip_reader_end(&zip); throw; }
    mz_zip_reader_end(&zip);
    if(!fs::exists(exe)) throw std::runtime_error("The installer is incomplete. Download it again.");
    progress(92,L"Adding the uninstaller");
    const auto uninstaller=o.folder/L"Uninstall Julretsu.exe";
    write_file(uninstaller,package.bytes.data(),package.program_size);
    manifest.push_back(L"Uninstall Julretsu.exe");
    const auto icon=o.folder/L"assets"/L"app-icon.ico";
    progress(94,L"Creating shortcuts");
    auto shortcut=[&](bool desktop,const std::wstring& name,const fs::path& target,const std::wstring& description) {
        const auto folder=shortcut_folder(o,desktop); fs::create_directories(folder,error);
        const auto link=folder/(name+L".lnk"); make_shortcut(link,target,icon,description); manifest.push_back(L"!"+link.wstring());
    };
    if(o.start_menu) {
        shortcut(false,L"Julretsu",exe,L"Julretsu spreadsheet");
        if(fs::exists(o.folder/L"Julretsu-User-Manual.pdf")) shortcut(false,L"Julretsu User Manual",o.folder/L"Julretsu-User-Manual.pdf",L"How to use Julretsu");
    }
    if(o.desktop) shortcut(true,L"Julretsu",exe,L"Julretsu spreadsheet");
    progress(97,L"Registering Julretsu");
    if(o.associate) {
        const auto type=o.classes()+L"\\Julretsu.Workbook";
        HKEY key=create_key(type); set_string(key,nullptr,L"Julretsu Workbook"); RegCloseKey(key);
        key=create_key(type+L"\\DefaultIcon"); set_string(key,nullptr,L"\""+icon.wstring()+L"\",0"); RegCloseKey(key);
        key=create_key(type+L"\\shell\\open\\command"); set_string(key,nullptr,L"\""+exe.wstring()+L"\" --open \"%1\""); RegCloseKey(key);
        key=create_key(o.classes()+L"\\.julretsu"); set_string(key,nullptr,L"Julretsu.Workbook"); RegCloseKey(key);
        key=create_key(o.classes()+L"\\.julretsu\\OpenWithProgids"); set_string(key,L"Julretsu.Workbook",L""); RegCloseKey(key);
        manifest.push_back(L"association=1");
    }
    std::uint64_t bytes=0; for(auto& entry:fs::recursive_directory_iterator(o.folder,error)) if(entry.is_regular_file(error)) bytes+=entry.file_size(error);
    HKEY key=create_key(o.uninstall_key());
    set_string(key,L"DisplayName",product); set_string(key,L"DisplayVersion",version()); set_string(key,L"Publisher",publisher);
    set_string(key,L"DisplayIcon",L"\""+exe.wstring()+L"\",0"); set_string(key,L"InstallLocation",o.folder.wstring());
    set_string(key,L"UninstallString",L"\""+uninstaller.wstring()+L"\" /uninstall");
    set_string(key,L"QuietUninstallString",L"\""+uninstaller.wstring()+L"\" /uninstall /S");
    set_string(key,L"URLInfoAbout",L"https://github.com/Deathbringer98");
    set_dword(key,L"EstimatedSize",DWORD(bytes/1024)); set_dword(key,L"NoModify",1); set_dword(key,L"NoRepair",1);
    RegCloseKey(key);
    std::string text; for(const auto& line:manifest) text+=narrow(line)+"\n";
    write_file(o.folder/L"install-manifest.txt",text.data(),text.size());
    if(o.test_root.empty()) SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,nullptr,nullptr);
    progress(100,L"Done");
}

// ---- Uninstall ----------------------------------------------------------
int uninstall(bool silent) {
    const auto folder=self_path().parent_path();
    // Only act inside a folder this installer created; never touch anything else.
    if(!fs::exists(folder/L"install-manifest.txt")) {
        if(!silent) MessageBoxW(nullptr,L"This uninstaller only works from the folder where Julretsu is installed.",L"Uninstall Julretsu",MB_OK|MB_ICONWARNING);
        return 3;
    }
    if(!silent&&MessageBoxW(nullptr,L"Remove Julretsu from this computer?\n\nYour workbooks are not affected.",L"Uninstall Julretsu",MB_YESNO|MB_ICONQUESTION)!=IDYES) return 1;
    if(file_locked(folder/L"Julretsu.exe")) {
        if(!silent) MessageBoxW(nullptr,L"Julretsu is running. Close it, then run the uninstaller again.",L"Uninstall Julretsu",MB_OK|MB_ICONWARNING);
        return 2;
    }
    std::vector<std::string> lines; bool association=false; Options o; o.folder=folder;
    { std::ifstream in(folder/L"install-manifest.txt"); std::string line; while(std::getline(in,line)) lines.push_back(line); }
    std::error_code error;
    for(const auto& line:lines) {
        if(line.rfind("root=",0)==0) { o.test_root=widen(line.substr(5)); continue; }
        if(line=="association=1") { association=true; continue; }
        if(line.empty()||line=="Uninstall Julretsu.exe") continue;
        if(line[0]=='!') { fs::remove(fs::path(widen(line.substr(1))),error); continue; }
        auto relative=entry_path(line); if(relative) fs::remove(folder/ *relative,error);
    }
    fs::remove(folder/L"install-manifest.txt",error);
    // Remove now-empty folders we created (assets, licenses), deepest first.
    std::vector<fs::path> folders;
    for(auto& entry:fs::recursive_directory_iterator(folder,error)) if(entry.is_directory(error)) folders.push_back(entry.path());
    std::sort(folders.begin(),folders.end(),[](const fs::path& a,const fs::path& b){ return a.wstring().size()>b.wstring().size(); });
    for(const auto& f:folders) if(fs::is_empty(f,error)) fs::remove(f,error);
    if(!o.test_root.empty()) for(bool desktop:{false,true}) { const auto f=shortcut_folder(o,desktop); if(fs::exists(f,error)&&fs::is_empty(f,error)) fs::remove(f,error); }
    // Only remove the file association if it still points at this copy of Julretsu.
    const auto command=read_string(o.classes()+L"\\Julretsu.Workbook\\shell\\open\\command",nullptr);
    if(association&&command.find((folder/L"Julretsu.exe").wstring())!=std::wstring::npos) {
        RegDeleteTreeW(HKEY_CURRENT_USER,(o.classes()+L"\\Julretsu.Workbook").c_str());
        if(read_string(o.classes()+L"\\.julretsu",nullptr)==L"Julretsu.Workbook") RegDeleteTreeW(HKEY_CURRENT_USER,(o.classes()+L"\\.julretsu").c_str());
        if(o.test_root.empty()) SHChangeNotify(SHCNE_ASSOCCHANGED,SHCNF_IDLIST,nullptr,nullptr);
    }
    const auto registered=read_string(o.uninstall_key(),L"InstallLocation");
    if(!registered.empty()&&CompareStringOrdinal(registered.c_str(),-1,folder.wstring().c_str(),-1,TRUE)==CSTR_EQUAL) RegDeleteTreeW(HKEY_CURRENT_USER,o.uninstall_key().c_str());
    if(!o.test_root.empty()) { RegDeleteTreeW(HKEY_CURRENT_USER,(L"Software\\"+o.test_root).c_str()); }
    // A running program cannot delete itself; a short-lived hidden command removes the uninstaller and the empty folder.
    std::wstring cleanup=L"cmd.exe /c ping 127.0.0.1 -n 3 > nul & del /f /q \""+self_path().wstring()+L"\" & rmdir \""+folder.wstring()+L"\"";
    STARTUPINFOW startup{}; startup.cb=sizeof startup; PROCESS_INFORMATION process{};
    if(CreateProcessW(nullptr,cleanup.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&process)) { CloseHandle(process.hThread); CloseHandle(process.hProcess); }
    if(!silent) MessageBoxW(nullptr,L"Julretsu has been removed.",L"Uninstall Julretsu",MB_OK|MB_ICONINFORMATION);
    return 0;
}

// ---- Wizard -------------------------------------------------------------
enum Id { IdLicense=100, IdAccept, IdFolder, IdBrowse, IdDesktop, IdAssociate, IdStartMenu, IdProgress, IdStatus, IdLaunch, IdBack, IdNext, IdCancel, IdInfo, IdTitle, IdSubtitle };
constexpr UINT WmProgress=WM_APP+1, WmDone=WM_APP+2;
struct Wizard {
    HWND window{}; HFONT font{}, title_font{}; HICON icon{}; UINT dpi=96; int page=0;
    Package package; Options options; std::wstring license, error; std::atomic<bool> busy{false}; std::thread worker;
    std::wstring status; int percent=0;
    int s(int value) const { return MulDiv(value,int(dpi),96); }
    HWND item(int id) const { return GetDlgItem(window,id); }
};
Wizard* wizard=nullptr;
std::wstring license_text(const Package& package) {
    mz_zip_archive zip{}; std::wstring result;
    if(!mz_zip_reader_init_mem(&zip,package.bytes.data()+package.zip_offset,package.zip_size,0)) return result;
    std::size_t size=0; void* data=mz_zip_reader_extract_file_to_heap(&zip,"LICENSE",&size,0);
    if(data) {
        std::string text(static_cast<const char*>(data),size); mz_free(data);
        std::string crlf; for(char c:text) { if(c=='\n'&&(crlf.empty()||crlf.back()!='\r')) crlf+='\r'; crlf+=c; }
        result=widen(crlf);
    }
    mz_zip_reader_end(&zip); return result;
}
void show(int id,bool visible) { ShowWindow(wizard->item(id),visible?SW_SHOW:SW_HIDE); }
void set_page(int page) {
    auto& w=*wizard; w.page=page;
    const std::wstring titles[]{L"License agreement",L"Choose install options",L"Installing Julretsu",L"Julretsu is installed"};
    const std::wstring subtitles[]{L"Please read the Julretsu Software License before installing.",
        L"Julretsu installs for your Windows account only. No administrator permission is needed.",
        L"This takes a few seconds.",L"Julretsu "+version()+L" is ready to use."};
    SetWindowTextW(w.item(IdTitle),titles[page].c_str()); SetWindowTextW(w.item(IdSubtitle),subtitles[page].c_str());
    for(int id:{IdLicense,IdAccept}) show(id,page==0);
    for(int id:{IdFolder,IdBrowse,IdDesktop,IdAssociate,IdStartMenu,IdInfo}) show(id,page==1);
    for(int id:{IdProgress,IdStatus}) show(id,page==2);
    show(IdLaunch,page==3);
    show(IdBack,page==1); show(IdCancel,page<2);
    SetWindowTextW(w.item(IdNext),page==0?L"Next":page==1?L"Install":L"Finish");
    EnableWindow(w.item(IdNext),page==0?IsDlgButtonChecked(w.window,IdAccept)==BST_CHECKED:page!=2);
    if(page==3) SetWindowTextW(w.item(IdInfo),L"");
    InvalidateRect(w.window,nullptr,TRUE);
}
void browse() {
    IFileOpenDialog* dialog=nullptr;
    if(FAILED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) return;
    DWORD flags=0; dialog->GetOptions(&flags); dialog->SetOptions(flags|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM);
    dialog->SetTitle(L"Choose where to install Julretsu");
    if(SUCCEEDED(dialog->Show(wizard->window))) {
        IShellItem* item=nullptr;
        if(SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path=nullptr;
            if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))) {
                fs::path chosen(path); if(chosen.filename()!=product) chosen/=product;
                SetWindowTextW(wizard->item(IdFolder),chosen.c_str()); CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
}
void start_install() {
    auto& w=*wizard;
    wchar_t folder[MAX_PATH*4]{}; GetWindowTextW(w.item(IdFolder),folder,int(std::size(folder)));
    w.options.folder=fs::path(folder);
    w.options.desktop=IsDlgButtonChecked(w.window,IdDesktop)==BST_CHECKED;
    w.options.associate=IsDlgButtonChecked(w.window,IdAssociate)==BST_CHECKED;
    w.options.start_menu=IsDlgButtonChecked(w.window,IdStartMenu)==BST_CHECKED;
    set_page(2); w.busy=true;
    w.worker=std::thread([] {
        CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        auto& w=*wizard; std::wstring failure;
        try {
            install(w.package,w.options,[&](int percent,const std::wstring& text) {
                auto* message=new std::wstring(text); PostMessageW(w.window,WmProgress,WPARAM(percent),LPARAM(message));
            });
        } catch(const std::exception& e) { failure=widen(e.what()); }
        CoUninitialize();
        PostMessageW(w.window,WmDone,0,LPARAM(new std::wstring(failure)));
    });
}
LRESULT CALLBACK window_proc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
    auto& w=*wizard;
    switch(message) {
    case WM_COMMAND:
        switch(LOWORD(wparam)) {
        case IdAccept: EnableWindow(w.item(IdNext),IsDlgButtonChecked(hwnd,IdAccept)==BST_CHECKED); return 0;
        case IdBrowse: browse(); return 0;
        case IdBack: set_page(0); return 0;
        case IdCancel: if(!w.busy) DestroyWindow(hwnd); return 0;
        case IdNext:
            if(w.page==0) set_page(1);
            else if(w.page==1) start_install();
            else if(w.page==3) {
                if(IsDlgButtonChecked(hwnd,IdLaunch)==BST_CHECKED)
                    ShellExecuteW(nullptr,L"open",(w.options.folder/L"Julretsu.exe").c_str(),nullptr,w.options.folder.c_str(),SW_SHOWNORMAL);
                DestroyWindow(hwnd);
            }
            return 0;
        }
        break;
    case WmProgress: {
        std::unique_ptr<std::wstring> text(reinterpret_cast<std::wstring*>(lparam));
        SendMessageW(w.item(IdProgress),PBM_SETPOS,wparam,0); SetWindowTextW(w.item(IdStatus),text->c_str()); return 0;
    }
    case WmDone: {
        std::unique_ptr<std::wstring> failure(reinterpret_cast<std::wstring*>(lparam));
        if(w.worker.joinable()) w.worker.join();
        w.busy=false;
        if(failure->empty()) set_page(3);
        else { MessageBoxW(hwnd,failure->c_str(),L"Julretsu could not be installed",MB_OK|MB_ICONERROR); set_page(1); }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint; HDC dc=BeginPaint(hwnd,&paint);
        RECT client{}; GetClientRect(hwnd,&client); RECT header{0,0,client.right,w.s(78)};
        FillRect(dc,&header,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        RECT line{0,w.s(78),header.right,w.s(79)}; HBRUSH border=CreateSolidBrush(RGB(214,224,226)); FillRect(dc,&line,border); DeleteObject(border);
        if(w.icon) DrawIconEx(dc,w.s(18),w.s(15),w.icon,w.s(48),w.s(48),0,nullptr,DI_NORMAL);
        EndPaint(hwnd,&paint); return 0;
    }
    case WM_CTLCOLORSTATIC: {
        // Labels and tick boxes on the same white background as the window.
        const int id=GetDlgCtrlID(reinterpret_cast<HWND>(lparam)); HDC dc=reinterpret_cast<HDC>(wparam);
        SetBkColor(dc,RGB(255,255,255));
        if(id==IdTitle) SetTextColor(dc,RGB(21,94,75)); else if(id==IdSubtitle) SetTextColor(dc,RGB(80,90,98));
        return LRESULT(GetStockObject(WHITE_BRUSH));
    }
    case WM_CLOSE: if(!w.busy) DestroyWindow(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,message,wparam,lparam);
}
int run_wizard(HINSTANCE instance,Package package,Options options) {
    Wizard w; wizard=&w; w.package=std::move(package); w.options=std::move(options); w.license=license_text(w.package);
    INITCOMMONCONTROLSEX controls{}; controls.dwSize=sizeof controls; controls.dwICC=ICC_PROGRESS_CLASS|ICC_STANDARD_CLASSES; InitCommonControlsEx(&controls);
    WNDCLASSEXW type{}; type.cbSize=sizeof type; type.lpfnWndProc=window_proc; type.hInstance=instance; type.lpszClassName=L"JulretsuSetup";
    type.hCursor=LoadCursorW(nullptr,IDC_ARROW); type.hbrBackground=static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)); type.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));
    RegisterClassExW(&type);
    w.dpi=GetDpiForSystem();
    RECT area{0,0,w.s(620),w.s(470)}; AdjustWindowRectExForDpi(&area,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,0,w.dpi);
    const std::wstring caption=L"Julretsu "+version()+L" Setup";
    w.window=CreateWindowExW(0,type.lpszClassName,caption.c_str(),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
        CW_USEDEFAULT,CW_USEDEFAULT,area.right-area.left,area.bottom-area.top,nullptr,nullptr,instance,nullptr);
    w.dpi=GetDpiForWindow(w.window);
    NONCLIENTMETRICSW metrics{}; metrics.cbSize=sizeof metrics; SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS,sizeof metrics,&metrics,0,w.dpi);
    w.font=CreateFontIndirectW(&metrics.lfMessageFont);
    LOGFONTW big=metrics.lfMessageFont; big.lfHeight=-w.s(20); big.lfWeight=FW_SEMIBOLD; w.title_font=CreateFontIndirectW(&big);
    w.icon=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(1),IMAGE_ICON,w.s(48),w.s(48),0));
    auto control=[&](const wchar_t* cls,const wchar_t* text,DWORD style,int id,int x,int y,int width,int height,DWORD ex=0) {
        HWND h=CreateWindowExW(ex,cls,text,WS_CHILD|WS_VISIBLE|style,w.s(x),w.s(y),w.s(width),w.s(height),w.window,reinterpret_cast<HMENU>(INT_PTR(id)),instance,nullptr);
        SendMessageW(h,WM_SETFONT,WPARAM(id==IdTitle?w.title_font:w.font),TRUE); return h;
    };
    control(L"STATIC",L"",SS_LEFT,IdTitle,80,16,520,28);
    control(L"STATIC",L"",SS_LEFT,IdSubtitle,80,44,520,22);
    control(L"EDIT",w.license.c_str(),ES_MULTILINE|ES_READONLY|WS_VSCROLL|WS_TABSTOP,IdLicense,20,94,580,300,WS_EX_CLIENTEDGE);
    control(L"BUTTON",L"I accept the Julretsu Software License",BS_AUTOCHECKBOX|WS_TABSTOP,IdAccept,20,402,400,24);
    control(L"STATIC",L"Install Julretsu in this folder:",SS_LEFT,IdInfo,20,100,580,22);
    control(L"EDIT",w.options.folder.c_str(),ES_AUTOHSCROLL|WS_TABSTOP,IdFolder,20,126,480,28,WS_EX_CLIENTEDGE);
    control(L"BUTTON",L"Browse...",BS_PUSHBUTTON|WS_TABSTOP,IdBrowse,508,125,92,30);
    control(L"BUTTON",L"Add Julretsu to the Start menu",BS_AUTOCHECKBOX|WS_TABSTOP,IdStartMenu,20,180,500,24);
    control(L"BUTTON",L"Create a desktop shortcut",BS_AUTOCHECKBOX|WS_TABSTOP,IdDesktop,20,212,500,24);
    control(L"BUTTON",L"Open .julretsu workbooks with Julretsu",BS_AUTOCHECKBOX|WS_TABSTOP,IdAssociate,20,244,500,24);
    control(PROGRESS_CLASSW,L"",0,IdProgress,20,150,580,22);
    control(L"STATIC",L"",SS_LEFT|SS_ENDELLIPSIS,IdStatus,20,182,580,22);
    control(L"BUTTON",L"Start Julretsu now",BS_AUTOCHECKBOX|WS_TABSTOP,IdLaunch,20,110,400,24);
    control(L"BUTTON",L"Back",BS_PUSHBUTTON|WS_TABSTOP,IdBack,300,428,92,30);
    control(L"BUTTON",L"Next",BS_DEFPUSHBUTTON|WS_TABSTOP,IdNext,400,428,100,30);
    control(L"BUTTON",L"Cancel",BS_PUSHBUTTON|WS_TABSTOP,IdCancel,508,428,92,30);
    for(int id:{IdStartMenu,IdDesktop,IdAssociate,IdLaunch}) CheckDlgButton(w.window,id,BST_CHECKED);
    SendMessageW(w.item(IdProgress),PBM_SETRANGE,0,MAKELPARAM(0,100));
    set_page(0);
    ShowWindow(w.window,SW_SHOW); UpdateWindow(w.window);
    MSG msg; while(GetMessageW(&msg,nullptr,0,0)) { if(!IsDialogMessageW(w.window,&msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); } }
    if(w.worker.joinable()) w.worker.join();
    DeleteObject(w.font); DeleteObject(w.title_font); if(w.icon) DestroyIcon(w.icon);
    return 0;
}
}

int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR,int) {
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    int argc=0; LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    Options options; options.folder=default_folder(); bool uninstalling=false;
    for(int i=1;i<argc;++i) {
        std::wstring_view arg=argv[i];
        if(arg==L"/S") options.silent=true;
        else if(arg==L"/uninstall") uninstalling=true;
        else if(arg.rfind(L"/D=",0)==0) options.folder=fs::path(std::wstring(arg.substr(3)));
        else if(arg.rfind(L"/test-root=",0)==0) options.test_root=std::wstring(arg.substr(11));
        else if(arg==L"/nodesktop") options.desktop=false;
        else if(arg==L"/noassoc") options.associate=false;
        else if(arg==L"/nostartmenu") options.start_menu=false;
    }
    LocalFree(argv);
    int result=0;
    auto package=load_package();
    if(uninstalling||!package) result=uninstall(options.silent);
    else if(options.silent) {
        try { install(*package,options,[](int,const std::wstring&){}); }
        catch(const std::exception&) { result=1; }
    } else result=run_wizard(instance,std::move(*package),options);
    CoUninitialize();
    return result;
}
