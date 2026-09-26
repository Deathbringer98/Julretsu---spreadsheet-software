#include "julretsu/GridUI.hpp"
#include "julretsu/WindowFrame.hpp"
#include "julretsu/Branding.hpp"
#include "julretsu/AllocationMetrics.hpp"
#include "julretsu/Glyphs.hpp"
#include "julretsu/I18n.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <shellapi.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#else
#include <sys/resource.h>
#endif

namespace {
using Clock=std::chrono::steady_clock;
struct GLFWLifetime {
    GLFWLifetime() {
        glfwSetErrorCallback([](int,const char* error){std::cerr<<"GLFW: "<<error<<'\n';});
        if(!glfwInit()) throw std::runtime_error("Unable to initialize GLFW. Check your graphics driver.");
    }
    ~GLFWLifetime() { glfwTerminate(); }
};
struct ImGuiLifetime {
    bool glfw{}, gl{};
    ImGuiLifetime() { IMGUI_CHECKVERSION(); ImGui::CreateContext(); }
    ~ImGuiLifetime() {
        if(gl) ImGui_ImplOpenGL3_Shutdown();
        if(glfw) ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
};
void theme(float scale,bool dark=false) {
    ImGui::StyleColorsLight(); auto& s=ImGui::GetStyle();
    s.WindowPadding={16,14}; s.FramePadding={10,7}; s.ItemSpacing={9,8};
    s.WindowRounding=0; s.ChildRounding=6; s.FrameRounding=4;
    s.Colors[ImGuiCol_MenuBarBg]={0.93f,0.95f,0.96f,1};
    s.Colors[ImGuiCol_WindowBg]={0.975f,0.98f,0.985f,1};
    s.Colors[ImGuiCol_ChildBg]={1,1,1,1};
    s.Colors[ImGuiCol_Text]={0.12f,0.17f,0.22f,1};
    s.Colors[ImGuiCol_TextDisabled]={0.43f,0.48f,0.52f,1};
    s.Colors[ImGuiCol_Button]={0.90f,0.94f,0.93f,1};
    s.Colors[ImGuiCol_ButtonHovered]={0.78f,0.89f,0.85f,1};
    s.Colors[ImGuiCol_ButtonActive]={0.65f,0.83f,0.76f,1};
    s.Colors[ImGuiCol_FrameBg]={1,1,1,1};
    s.Colors[ImGuiCol_Border]={0.83f,0.87f,0.88f,1};
    s.Colors[ImGuiCol_CheckMark]={0.10f,0.48f,0.36f,1};
    s.Colors[ImGuiCol_SliderGrab]={0.10f,0.48f,0.36f,1};
    s.Colors[ImGuiCol_Header]={0.79f,0.9f,0.85f,1};
    s.FrameRounding=6; s.ChildRounding=9; s.WindowPadding={12,12};
    s.FrameBorderSize=1; // keeps checkboxes, sliders and colour swatches visible on popups in both themes
    s.Colors[ImGuiCol_Button]={0.95f,0.975f,0.975f,1};
    if(dark) {
        s.Colors[ImGuiCol_MenuBarBg]={0.10f,0.15f,0.19f,1};
        s.Colors[ImGuiCol_WindowBg]={0.07f,0.105f,0.14f,1};
        s.Colors[ImGuiCol_ChildBg]={0.085f,0.125f,0.16f,1};
        s.Colors[ImGuiCol_PopupBg]={0.11f,0.155f,0.19f,1};
        s.Colors[ImGuiCol_Text]={0.88f,0.92f,0.95f,1};
        s.Colors[ImGuiCol_TextDisabled]={0.59f,0.68f,0.74f,1};
        s.Colors[ImGuiCol_Button]={0.13f,0.19f,0.23f,1};
        s.Colors[ImGuiCol_ButtonHovered]={0.18f,0.32f,0.30f,1};
        s.Colors[ImGuiCol_ButtonActive]={0.19f,0.41f,0.35f,1};
        s.Colors[ImGuiCol_FrameBg]={0.15f,0.21f,0.26f,1};
        s.Colors[ImGuiCol_FrameBgHovered]={0.19f,0.28f,0.32f,1};
        s.Colors[ImGuiCol_FrameBgActive]={0.21f,0.33f,0.33f,1};
        s.Colors[ImGuiCol_Border]={0.21f,0.28f,0.33f,1};
        s.Colors[ImGuiCol_Separator]=s.Colors[ImGuiCol_Border];
        s.Colors[ImGuiCol_CheckMark]={0.36f,0.82f,0.66f,1};
        s.Colors[ImGuiCol_SliderGrab]=s.Colors[ImGuiCol_CheckMark];
        s.Colors[ImGuiCol_SliderGrabActive]={0.54f,0.94f,0.78f,1};
        s.Colors[ImGuiCol_Header]={0.14f,0.29f,0.25f,1};
        s.Colors[ImGuiCol_HeaderHovered]={0.17f,0.37f,0.31f,1};
        s.Colors[ImGuiCol_HeaderActive]=s.Colors[ImGuiCol_HeaderHovered];
        s.Colors[ImGuiCol_TextSelectedBg]={0.16f,0.42f,0.35f,0.8f};
        s.Colors[ImGuiCol_TitleBgActive]=s.Colors[ImGuiCol_Header];
    }
    s.ScaleAllSizes(scale);
}
struct FontFace { const char* path; int index=0; };
// Font files are read once and shared with the atlas, which is rebuilt whenever new characters appear.
const std::vector<char>* font_file(const char* path) {
    static std::map<std::string,std::vector<char>> files;
    auto [entry,inserted]=files.try_emplace(path);
    if(inserted) {
        std::ifstream in(std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(path))),std::ios::binary);
        if(in) entry->second.assign(std::istreambuf_iterator<char>(in),{});
    }
    return entry->second.empty()?nullptr:&entry->second;
}
bool add_face(const std::vector<FontFace>& candidates,float size,const ImWchar* ranges,bool merge,float oversample=2) {
    for(const auto& face:candidates) {
        const auto* data=font_file(face.path); if(!data) continue;
        ImFontConfig config; config.FontDataOwnedByAtlas=false; config.FontNo=face.index; config.MergeMode=merge;
        config.OversampleH=int(oversample); config.OversampleV=1; config.GlyphRanges=ranges;
        if(ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<char*>(data->data()),int(data->size()),size,&config)) return true;
    }
    return false;
}
void font(float scale) {
    auto& io=ImGui::GetIO(); io.Fonts->Clear();
    const float size=20*scale;
    // Latin text comes from the sans, serif and monospace fonts; Korean and Japanese characters that are
    // actually shown are merged in from the system's CJK fonts. System fonts are used, never redistributed.
    static std::vector<ImWchar> ranges, extra;
    extra.clear();
    for(const auto c:julretsu::glyphs::extra()) {
        if(!extra.empty()&&extra.back()+1==ImWchar(c)) extra.back()=ImWchar(c); else { extra.push_back(ImWchar(c)); extra.push_back(ImWchar(c)); }
    }
    ranges={0x0020,0x024F,0x2010,0x205E,0x20A0,0x20BF,0x2122,0x2122,0x2190,0x2193,0xFFFD,0xFFFD};
    ranges.insert(ranges.end(),extra.begin(),extra.end()); ranges.push_back(0); extra.push_back(0);
    const bool korean_first=julretsu::language()!=julretsu::Language::Japanese;
#ifdef _WIN32
    const std::vector<FontFace> korean{{"C:/Windows/Fonts/malgun.ttf"}}, japanese{{"C:/Windows/Fonts/YuGothM.ttc",1},{"C:/Windows/Fonts/meiryo.ttc",2},{"C:/Windows/Fonts/msgothic.ttc",1}}; // the "UI" faces with compact kana
    const std::vector<FontFace> faces[]{{{"C:/Windows/Fonts/segoeui.ttf"}},{{"C:/Windows/Fonts/georgia.ttf"}},{{"C:/Windows/Fonts/consola.ttf"}}};
#elif defined(__APPLE__)
    const std::vector<FontFace> korean{{"/System/Library/Fonts/AppleSDGothicNeo.ttc"},{"/Library/Fonts/Arial Unicode.ttf"}},
        japanese{{"/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc"},{"/System/Library/Fonts/Hiragino Sans GB.ttc"},{"/Library/Fonts/Arial Unicode.ttf"}};
    const std::vector<FontFace> faces[]{{{"/System/Library/Fonts/Supplemental/Arial.ttf"},{"/Library/Fonts/Arial.ttf"}},
        {{"/System/Library/Fonts/Supplemental/Georgia.ttf"},{"/Library/Fonts/Georgia.ttf"}},
        {{"/System/Library/Fonts/Supplemental/Courier New.ttf"},{"/Library/Fonts/Courier New.ttf"}}};
#else
    // Noto Sans CJK collections hold Japanese (0) and Korean (1) faces; distributions install them in different folders.
    const std::vector<FontFace> korean{{"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",1},{"/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",1},
        {"/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",1},{"/usr/share/fonts/truetype/nanum/NanumGothic.ttf"}},
        japanese{{"/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",0},{"/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",0},
        {"/usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc",0},{"/usr/share/fonts/opentype/ipafont-gothic/ipag.ttf"},{"/usr/share/fonts/truetype/fonts-japanese-gothic.ttf"}};
    const std::vector<FontFace> faces[]{
        {{"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"},{"/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf"},{"/usr/share/fonts/TTF/DejaVuSans.ttf"},{"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"}},
        {{"/usr/share/fonts/truetype/dejavu/DejaVuSerif.ttf"},{"/usr/share/fonts/dejavu-serif-fonts/DejaVuSerif.ttf"},{"/usr/share/fonts/TTF/DejaVuSerif.ttf"},{"/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf"}},
        {{"/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"},{"/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf"},{"/usr/share/fonts/TTF/DejaVuSansMono.ttf"},{"/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf"}}};
#endif
    for(const auto& face:faces) {
        if(!add_face(face,size,ranges.data(),false)) { ImFontConfig config; config.SizePixels=size; io.Fonts->AddFontDefault(&config); }
        if(extra.size()>1) {
            // The first font that has a character supplies it, so the interface language decides which
            // style shared Chinese characters and kana use.
            add_face(korean_first?korean:japanese,size,extra.data(),true,1);
            add_face(korean_first?japanese:korean,size,extra.data(),true,1);
        }
    }
}
void screenshot(const std::string& path,int width,int height) {
    if(width<=0||height<=0) return;
    const auto stride=(std::size_t(width)*3+3)&~std::size_t(3);
    std::vector<unsigned char> pixels(stride*std::size_t(height));
    glPixelStorei(GL_PACK_ALIGNMENT,4);
    glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    for(int y=0;y<height;++y) for(int x=0;x<width;++x) std::swap(pixels[std::size_t(y)*stride+std::size_t(x)*3],pixels[std::size_t(y)*stride+std::size_t(x)*3+2]);
    std::ofstream file(path,std::ios::binary); if(!file) throw std::runtime_error("Cannot write screenshot");
    auto u16=[&](unsigned v){file.put(char(v));file.put(char(v>>8));};
    auto u32=[&](std::uint32_t v){for(int i=0;i<4;++i) file.put(char(v>>(8*i)));};
    file.write("BM",2); u32(std::uint32_t(54+pixels.size())); u32(0); u32(54);
    u32(40); u32(unsigned(width)); u32(unsigned(height)); u16(1); u16(24); u32(0); u32(std::uint32_t(pixels.size())); u32(2835);u32(2835);u32(0);u32(0);
    file.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()));
}
std::size_t memory_bytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS memory{};
    return GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof memory)?memory.WorkingSetSize:0;
#else
    rusage usage{}; if(getrusage(RUSAGE_SELF,&usage)!=0) return 0;
#ifdef __APPLE__
    return std::size_t(usage.ru_maxrss);
#else
    return std::size_t(usage.ru_maxrss)*1024;
#endif
#endif
}
}
int main(int argc,char** argv) {
    bool smoke=false, benchmark=false, show=false, no_splash=false, system_titlebar=false;
    std::string manual_dir; // --manual-shots: clean screenshots of each feature for the user manual
    std::string capture="julretsu";
    std::string language_code; // --language en|ko|ja overrides the saved choice
    std::filesystem::path open_path;
    bool check_open=false;
    for(int i=1;i<argc;++i) {
        std::string arg=argv[i];
        if(arg=="--check-open") check_open=true;
        else if(arg=="--open"&&i+1<argc) {
#ifndef _WIN32
            open_path=argv[i+1];
#endif
            ++i;
        }
        else if(arg=="--smoke") smoke=true;
        else if(arg=="--system-titlebar") system_titlebar=true;
        else if(arg=="--manual-shots"&&i+1<argc) { manual_dir=argv[++i]; no_splash=true; }
        else if(arg=="--benchmark") benchmark=true;
        else if(arg=="--visible") show=true;
        else if(arg=="--no-splash") no_splash=true;
        else if(arg=="--capture-prefix"&&i+1<argc) capture=argv[++i];
        else if(arg=="--language"&&i+1<argc) language_code=argv[++i];
    }
    try {
#ifdef _WIN32
        int argument_count=0;
        wchar_t** arguments=CommandLineToArgvW(GetCommandLineW(),&argument_count);
        if(!arguments) throw std::runtime_error("Cannot read the workbook filename.");
        for(int i=1;i<argument_count;++i) {
            const std::wstring_view argument=arguments[i];
            if(argument==L"--open"&&i+1<argument_count) open_path=arguments[++i];
            else if((argument==L"--capture-prefix"||argument==L"--manual-shots"||argument==L"--language")&&i+1<argument_count) ++i;
            else if(!argument.empty()&&argument.front()!=L'-') open_path=arguments[i];
        }
        LocalFree(arguments);
#endif
        GLFWLifetime glfw;
#ifdef __APPLE__
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,2);
        glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE); glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT,GL_TRUE);
        const char* glsl="#version 150";
#else
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);
        glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
        const char* glsl="#version 330";
#endif
        // Show the first rendered frame instead of a blank startup window.
        glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
        using Window=std::unique_ptr<GLFWwindow,decltype(&glfwDestroyWindow)>;
        Window window(glfwCreateWindow(1580,960,"Julretsu - Spreadsheet",nullptr,nullptr),glfwDestroyWindow);
        if(!window) throw std::runtime_error("OpenGL window creation failed.");
        glfwSetWindowSizeLimits(window.get(),1100,640,GLFW_DONT_CARE,GLFW_DONT_CARE);
        julretsu::WindowFrame window_frame;
#ifdef _WIN32
        if(!system_titlebar) window_frame.install(glfwGetWin32Window(window.get()));
#endif
        glfwMakeContextCurrent(window.get()); glfwSwapInterval(benchmark||smoke?0:1);
        ImGui::SetAllocatorFunctions(
            [](std::size_t n,void*)->void* { ++julretsu::metrics::imgui_allocations; julretsu::metrics::imgui_bytes+=n; return std::malloc(n); },
            [](void* p,void*){std::free(p);});
        ImGuiLifetime gui;
        auto& io=ImGui::GetIO(); io.IniFilename=nullptr;
        io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
        float sx=1,sy=1; glfwGetWindowContentScale(window.get(),&sx,&sy);
        float scale=std::clamp(std::max(sx,sy),1.0f,2.5f);
        // Tests and manual screenshots are English unless a language is asked for.
        julretsu::set_language(!language_code.empty()?julretsu::language_from_code(language_code):
            (smoke||benchmark||!manual_dir.empty())?julretsu::Language::English:julretsu::GridUI::saved_language());
        for(auto text:julretsu::translations(julretsu::language())) julretsu::glyphs::note(text);
        theme(scale); font(scale);
        if(!ImGui_ImplGlfw_InitForOpenGL(window.get(),true)) throw std::runtime_error("ImGui GLFW initialization failed.");
        gui.glfw=true;
        if(!ImGui_ImplOpenGL3_Init(glsl)) throw std::runtime_error("ImGui OpenGL initialization failed.");
        gui.gl=true;
        julretsu::Branding branding(window.get());
        const auto splash_start=Clock::now();
        auto loading_frame=[&](const char* status,bool capture_image=false) {
            glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
            branding.draw(status); ImGui::Render();
            int width{},height{}; glfwGetFramebufferSize(window.get(),&width,&height);
            glViewport(0,0,width,height); glClearColor(1,1,1,1); glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            if(capture_image) screenshot(capture+"-loading.bmp",width,height);
            glfwSwapBuffers(window.get());
        };
        if(!no_splash) loading_frame(julretsu::tr("Starting Julretsu..."));
        if((!smoke&&!benchmark&&!check_open)||show) glfwShowWindow(window.get());
        if(!no_splash) loading_frame(julretsu::tr("Preparing your worksheet..."),smoke);
        julretsu::GridUI app; app.set_scale(scale); app.brand_icon=branding.icon_texture();
#ifdef _WIN32
        app.native_window=glfwGetWin32Window(window.get());
#endif
        app.custom_frame=window_frame.active();
        if(smoke||benchmark||!manual_dir.empty()) app.disable_recovery();
        if(!manual_dir.empty()) app.set_dark(false,false);
        if(smoke||benchmark) app.set_dark(false,false);
        bool current_dark=app.dark(); theme(scale,current_dark);
        if(smoke&&std::getenv("JULRETSU_SETTINGS_PATH")) {
            app.set_dark(true,true);
            julretsu::GridUI restored;
            if(!restored.dark()) throw std::runtime_error("Dark appearance preference did not survive reload");
            app.set_dark(false,true);
            julretsu::GridUI restored_light;
            if(restored_light.dark()) throw std::runtime_error("Light appearance preference did not survive reload");
            app.set_dark(false,false);
            std::cout<<"appearance preference round trip=PASS\n";
            app.change_language(julretsu::Language::Japanese,true);
            if(julretsu::GridUI::saved_language()!=julretsu::Language::Japanese) throw std::runtime_error("Language preference did not survive reload");
            app.change_language(julretsu::Language::English,true);
            if(julretsu::GridUI::saved_language()!=julretsu::Language::English) throw std::runtime_error("English preference did not survive reload");
            std::cout<<"language preference round trip=PASS\n";
        }
        if(!open_path.empty()) {
            const bool opened=app.open_from(open_path);
            if(check_open) {std::cout<<"startup_workbook_open="<<opened<<" cells="<<app.sheet().populated_cells()<<"\n";return opened?0:4;}
        } else if(check_open) return 4;
        if(benchmark) app.load_performance_fixture();
        if(!smoke&&!benchmark&&!no_splash) {
            // A short minimum makes the supplied splash readable on fast starts.
            // Ready is shown honestly once setup finishes; no fake percentage.
            while(Clock::now()-splash_start<std::chrono::milliseconds(650)&&!glfwWindowShouldClose(window.get())) {
                loading_frame(julretsu::tr("Ready"));
                glfwWaitEventsTimeout(0.016);
            }
        }
        if(glfwWindowShouldClose(window.get())) return 0;
        if(smoke) {
            // Every translation must take the same printf arguments as its English text.
            const auto problems=julretsu::translations(julretsu::Language::Korean).size()>400?julretsu::translation_problems():std::vector<std::string>{"translation table is missing"};
            for(const auto& problem:problems) std::cerr<<"translation: "<<problem<<'\n';
            julretsu::set_language(julretsu::Language::Korean);
            const bool lookups=std::string(julretsu::tr("Save"))=="저장"&&std::string(julretsu::tr("Bold##smoke"))=="굵게##smoke"
                &&julretsu::tr_text("Recovery copy failed: disk full")=="복구 사본을 만들지 못했습니다: disk full"
                &&julretsu::trf("Apply %zu edit%s",std::size_t(3),"s")=="편집 3개 적용"&&std::string(julretsu::tr("Not in the table"))=="Not in the table";
            julretsu::set_language(julretsu::Language::Japanese);
            const bool japanese=std::string(julretsu::tr("Save"))=="保存";
            julretsu::set_language(julretsu::Language::English);
            std::cout<<"translations="<<(problems.empty()&&lookups&&japanese?"PASS":"FAIL")<<"\n";
            if(!problems.empty()||!lookups||!japanese) return 5;
            julretsu::GridUI tool_check; tool_check.disable_recovery();
            if(!tool_check.smoke_workbook_features(capture+"-features.julretsu"))throw std::runtime_error("Advanced workbook smoke checks failed");
            std::cout<<"Cell styles, clipboard, sequences, worksheet links, multi-sheet persistence, filters and reports passed\n";
            if(!tool_check.smoke_report_export(capture+"-report-exports"))throw std::runtime_error("Report export regression failed");
            if(!tool_check.smoke_selection_tools()) throw std::runtime_error("Selection tools smoke checks failed");
            if(!tool_check.smoke_safety_net(capture+"-safety.julretsu")) throw std::runtime_error("Safety net smoke checks failed");
            std::cout<<"Safety net: typed outliers, change review, formula overwrite, partial sort, sheet check fix, saved history passed\n";
            std::cout<<"Selection tools: totals, overwrite protection, fill, undo, search, statistics passed\n";
            if(!tool_check.smoke_validation_import())throw std::runtime_error("Validation import regression failed");
            std::cout<<"branding_assets_loaded="<<branding.loaded()<<"\n";
            if(!branding.loaded()) return 3;
        }
        std::vector<double> frames; frames.reserve(400);
        std::size_t cpp_allocations=0,imgui_allocations=0,grid_allocations=0;
        bool quit=false;
        const int frame_limit=smoke?205:(benchmark?400:(!manual_dir.empty()?100:0));
        const int warmup=smoke?164:60;
        int frame=0; bool smoke_ok=true;
        while(!quit) {
            auto start=Clock::now();
            auto cpp_before=julretsu::metrics::cpp_allocations.load();
            auto imgui_before=julretsu::metrics::imgui_allocations.load();
            glfwPollEvents();
            if(glfwWindowShouldClose(window.get())) {
                if(app.modified()&&!frame_limit) { if(app.confirm_close()) break; glfwSetWindowShouldClose(window.get(),false); }
                else break;
            }
            glfwGetWindowContentScale(window.get(),&sx,&sy);
            const float new_scale=std::clamp(std::max(sx,sy),1.0f,2.5f);
            if(new_scale!=scale) {
                scale=new_scale; theme(scale,app.dark()); font(scale);
                ImGui_ImplOpenGL3_DestroyFontsTexture(); ImGui_ImplOpenGL3_CreateFontsTexture();
                app.set_scale(scale);
            }
            if(smoke) {
                if(frame==10) app.show_scripts(true);
                if(frame==25) app.jump({julretsu::max_rows-1,julretsu::max_columns-1});
                if(frame==40) { app.jump({3,2}); app.show_scripts(false); }
                if(frame==50) app.jump({3,7});
            }
            if(current_dark!=app.dark()) { current_dark=app.dark(); theme(scale,current_dark); }
            app.prepare();
            if(julretsu::glyphs::take_changes()) { font(scale); ImGui_ImplOpenGL3_DestroyFontsTexture(); ImGui_ImplOpenGL3_CreateFontsTexture(); }
            ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame();
            if(!manual_dir.empty()) {
                // Walk through the features on the example workbook, pausing a few frames for each screenshot.
                io.AddFocusEvent(true);
                const auto file_button=app.targets[julretsu::GridUI::FileButton];
                if(frame==6) { io.AddMousePosEvent(file_button.x,file_button.y); io.AddMouseButtonEvent(0,true); }
                if(frame==7) io.AddMouseButtonEvent(0,false);
                if(frame==11) io.AddKeyEvent(ImGuiKey_Escape,true);
                if(frame==12) { io.AddKeyEvent(ImGuiKey_Escape,false); io.AddMousePosEvent(-FLT_MAX,-FLT_MAX); }
                if(frame==14) app.smoke_show_format(true);
                if(frame==18) { app.smoke_show_format(false); io.AddKeyEvent(ImGuiKey_Escape,true); }
                if(frame==19) io.AddKeyEvent(ImGuiKey_Escape,false);
                if(frame==22) app.smoke_health_example(true);
                if(frame==27) app.smoke_health_example(false);
                if(frame==30) app.smoke_open_review();
                if(frame==35) app.smoke_close_review();
                if(frame==38) app.smoke_queue_edit({4,2},"3");
                if(frame==40) app.smoke_queue_edit({4,2},"5");
                if(frame==43) { app.jump({4,2}); app.smoke_show_history({4,2},true); }
                if(frame==48) app.smoke_show_history({4,2},false);
                if(frame==52) app.show_ai(true);
                if(frame==57) app.show_ai(false);
                if(frame==60) app.show_scripts(true);
                if(frame==64) app.show_scripts(false);
                if(frame==67) app.smoke_show_report(true,{2,0},{8,4});
                if(frame==72) app.smoke_show_report(false);
                if(frame==75) app.smoke_workbook_window(1);
                if(frame==79) app.smoke_workbook_window(0);
                if(frame==82) app.set_dark(true,false);
                if(frame==87) app.set_dark(false,false);
                if(frame==90) { app.jump({3,2}); app.smoke_ribbon_tab(3); }
            }
            if(smoke) {
                io.AddFocusEvent(true);
                if(frame==70) {
                    const bool saved=app.save_to(capture+"-roundtrip.julretsu");
                    smoke_ok &= saved&&!app.modified();
                    julretsu::GridUI reopened;
                    const bool loaded=reopened.open_from(capture+"-roundtrip.julretsu");
                    smoke_ok &= loaded&&reopened.sheet().read({3,7})==app.sheet().read({3,7})&&reopened.sheet().row_style(3)==app.sheet().row_style(3);
                    std::cout<<"workbook save/open with Lua and formatting="<<(saved&&loaded)<<"\n";
                }
                if(frame==79) {smoke_ok &= app.save_to(capture+"-draft.julretsu");app.jump({19,0});}
                if(frame==81) io.AddKeyEvent(ImGuiKey_F2,true);
                if(frame==82) io.AddKeyEvent(ImGuiKey_F2,false);
                if(frame==83) io.AddInputCharactersUTF8("123");
                if(frame==85) {io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_S,true);}
                if(frame==86) {io.AddKeyEvent(ImGuiMod_Ctrl,false);io.AddKeyEvent(ImGuiKey_S,false);}
                if(frame==89) {
                    julretsu::GridUI reopened;
                    const bool loaded=reopened.open_from(capture+"-draft.julretsu");
                    const bool saved=loaded&&reopened.sheet().read({19,0})==julretsu::Value{123.0}&&!app.modified();
                    smoke_ok &= saved;std::cout<<"Ctrl+S saves unfinished cell edit="<<saved<<"\n";
                    app.jump({3,2});
                }
                if(frame==91) {io.AddMousePosEvent(app.targets[julretsu::GridUI::FileButton].x,app.targets[julretsu::GridUI::FileButton].y);io.AddMouseButtonEvent(0,true);}
                if(frame==92) io.AddMouseButtonEvent(0,false);
                if(frame==98) {
                    app.show_ai(true);
                    app.smoke_ai_proposal(R"J({"summary":"Label and total","edits":[{"cell":"H1","kind":"text","value":"AI smoke"},{"cell":"B20","kind":"number","value":"7"},{"cell":"C20","kind":"formula","value":"=LUA(\"return 1\")"}]})J");
                    const auto& p=app.ai_proposal();
                    const bool ok=p&&p->edits.size()==3&&!p->edits[2].selected&&!app.sheet().cell({0,7});
                    std::cout<<"AI proposal preview (no sheet change)="<<ok<<"\n"; smoke_ok &= ok;
                }
                if(frame==127) app.smoke_health_example(true);
                if(frame==131) { app.smoke_health_example(false); }
                if(frame==132) app.smoke_open_review();
                if(frame==137) app.smoke_close_review();
                if(frame==138) { app.smoke_queue_edit({1,2},"This sentence is far too long to fit inside a single spreadsheet cell"); app.smoke_queue_edit({1,3},"123456789012345678"); }
                if(frame==139) app.jump({0,0});
                // Korean and Japanese: the interface and a sample workbook render with real glyphs, not "?".
                auto has_glyph=[&](unsigned c){ for(auto* font:io.Fonts->Fonts) if(!font->FindGlyphNoFallback(ImWchar(c))) return false; return true; };
                if(frame==142) { app.change_language(julretsu::Language::Korean,false); app.load_demo(); }
                if(frame==145) { const bool ok=has_glyph(0xD648)&&has_glyph(0xC608); std::cout<<"korean glyphs="<<ok<<"\n"; smoke_ok&=ok; }
                if(frame==146) { app.change_language(julretsu::Language::Japanese,false); app.load_demo(); }
                if(frame==149) { const bool ok=has_glyph(0x30DB)&&has_glyph(0x4E88); std::cout<<"japanese glyphs="<<ok<<"\n"; smoke_ok&=ok; }
                if(frame==151) app.smoke_validation(true);
                if(frame==155) app.change_language(julretsu::Language::Korean,false);
                if(frame==159) app.change_language(julretsu::Language::Japanese,false);
                if(frame==163) app.smoke_validation(false);
                if(frame==164) { app.change_language(julretsu::Language::English,false); app.smoke_restore_points(true); }
                if(frame==168) app.change_language(julretsu::Language::Korean,false);
                if(frame==172) app.change_language(julretsu::Language::Japanese,false);
                if(frame==176) { app.smoke_restore_points(false); app.smoke_tables(true); app.change_language(julretsu::Language::English,false); }
                if(frame==180) app.change_language(julretsu::Language::Korean,false);
                if(frame==184) app.change_language(julretsu::Language::Japanese,false);
                if(frame==188) { app.smoke_tables(false); app.show_scripts(true); app.change_language(julretsu::Language::English,false); }
                if(frame==192) app.change_language(julretsu::Language::Korean,false);
                if(frame==196) app.change_language(julretsu::Language::Japanese,false);
                if(frame==200) { app.show_scripts(false); app.change_language(julretsu::Language::English,false); }
                if(frame==150) { app.change_language(julretsu::Language::English,false); app.load_demo(); }
                if(frame==118) app.smoke_workbook_window(1);
                if(frame==122) app.smoke_workbook_window(2);
                if(frame==126) { app.smoke_workbook_window(0); std::cout<<"title bar="<<(app.custom_frame?"custom":"system")<<" caption_height="<<app.caption_height()<<"\n"; smoke_ok &= !app.custom_frame||app.caption_height()>0; }
                if(frame==112)app.smoke_show_report(true,{3,2},{8,4});
                if(frame==116)app.smoke_show_report(false);
                if(frame==106)app.smoke_show_format(true);
                if(frame==108) {app.smoke_show_format(false);io.AddKeyEvent(ImGuiKey_Escape,true);}
                if(frame==109)io.AddKeyEvent(ImGuiKey_Escape,false);
                if(frame==103) app.smoke_ai_apply();
                if(frame==105) {
                    const bool ok=!app.ai_proposal()&&app.sheet().read({0,7})==julretsu::Value{std::string("AI smoke")}&&app.sheet().read({19,1})==julretsu::Value{7.0}&&!app.sheet().cell({19,2});
                    std::cout<<"AI proposal applied (Lua edit left unticked)="<<ok<<"\n"; smoke_ok &= ok; app.show_ai(false);
                }
                if(frame==96) io.AddKeyEvent(ImGuiKey_Escape,true);
                if(frame==97) io.AddKeyEvent(ImGuiKey_Escape,false);

                if(frame==72) { io.AddMousePosEvent(app.targets[julretsu::GridUI::ThemeButton].x,app.targets[julretsu::GridUI::ThemeButton].y); io.AddMouseButtonEvent(0,true); }
                if(frame==73) io.AddMouseButtonEvent(0,false);
                if(frame==77) { app.load_demo(); app.jump({3,2}); } if(frame==76) { std::cout<<"dark mode toggle="<<app.dark()<<"\n"; smoke_ok &= app.dark(); }
                if(frame==2||frame==42) io.AddKeyEvent(ImGuiKey_F2,true);
                if(frame==3||frame==43) io.AddKeyEvent(ImGuiKey_F2,false);
                if(frame==5) io.AddInputCharactersUTF8("12");
                if(frame==7) io.AddKeyEvent(ImGuiKey_Enter,true);
                if(frame==8) io.AddKeyEvent(ImGuiKey_Enter,false);
                if(frame==12||frame==16) io.AddKeyEvent(ImGuiMod_Ctrl,true);
                if(frame==12) io.AddKeyEvent(ImGuiKey_Z,true);
                if(frame==13) { io.AddKeyEvent(ImGuiKey_Z,false); io.AddKeyEvent(ImGuiMod_Ctrl,false); }
                if(frame==16) io.AddKeyEvent(ImGuiKey_Y,true);
                if(frame==17) { io.AddKeyEvent(ImGuiKey_Y,false); io.AddKeyEvent(ImGuiMod_Ctrl,false); }
                if(frame==44) io.AddInputCharactersUTF8("99");
                if(frame==46) io.AddKeyEvent(ImGuiKey_Escape,true);
                if(frame==47) io.AddKeyEvent(ImGuiKey_Escape,false);
                if(frame==21) { io.AddMousePosEvent(app.targets[julretsu::GridUI::MacroButton].x,app.targets[julretsu::GridUI::MacroButton].y); io.AddMouseButtonEvent(0,true); }
                if(frame==22) io.AddMouseButtonEvent(0,false);
                if(frame==52) { io.AddMousePosEvent(app.targets[julretsu::GridUI::FormulaField].x,app.targets[julretsu::GridUI::FormulaField].y); io.AddMouseButtonEvent(0,true); }
                if(frame==53) io.AddMouseButtonEvent(0,false);
                if(frame==54) { io.AddKeyEvent(ImGuiMod_Ctrl,true); io.AddKeyEvent(ImGuiKey_A,true); }
                if(frame==55) { io.AddKeyEvent(ImGuiKey_A,false); io.AddKeyEvent(ImGuiMod_Ctrl,false); }
                if(frame==56) io.AddInputCharactersUTF8("=LUA(\"return cell('E11') * 2\")");
                if(frame==58) io.AddKeyEvent(ImGuiKey_Enter,true);
                if(frame==59) io.AddKeyEvent(ImGuiKey_Enter,false);
                if(frame==62) {
                    const auto actual=app.sheet().read({3,7});
                    const bool ok=std::holds_alternative<double>(actual)&&std::get<double>(actual)==16124;
                    std::cout<<"formula bar Lua value="<<julretsu::display(actual)<<" expected=16124\n";
                    smoke_ok &= ok;
                }
                if(frame==66) { io.AddMousePosEvent(app.targets[julretsu::GridUI::BoldButton].x,app.targets[julretsu::GridUI::BoldButton].y); io.AddMouseButtonEvent(0,true); }
                if(frame==67) io.AddMouseButtonEvent(0,false);
                if(frame==69) { const bool ok=app.sheet().cell_style({3,7}).bold; std::cout<<"ribbon bold (selected cell)="<<ok<<"\n"; smoke_ok &= ok; }
                if(frame==36) { io.AddKeyEvent(ImGuiMod_Ctrl,true); io.AddKeyEvent(ImGuiKey_Z,true); }
                if(frame==37) { io.AddKeyEvent(ImGuiKey_Z,false); io.AddKeyEvent(ImGuiMod_Ctrl,false); }
                if(frame==24||frame==38) {
                    const auto actual=app.sheet().read({3,4}); const double expected=frame==24?5850:5400;
                    const bool ok=std::holds_alternative<double>(actual)&&std::get<double>(actual)==expected;
                    std::cout<<"macro button frame="<<frame<<" value="<<julretsu::display(actual)<<" expected="<<expected<<"\n";
                    smoke_ok &= ok;
                }
                if(frame==10||frame==15||frame==19||frame==49) {
                    const double expected=frame==15?1350:5400;
                    const auto actual=app.sheet().read({3,4});
                    const bool ok=std::holds_alternative<double>(actual)&&std::get<double>(actual)==expected;
                    std::cout<<"keyboard frame="<<frame<<" value="<<julretsu::display(actual)<<" expected="<<expected<<"\n";
                    smoke_ok &= ok;
                }
            }
            ImGui::NewFrame();
#ifdef __APPLE__
            io.KeyCtrl=io.KeyCtrl||io.KeySuper; // Cmd+S, Cmd+Z and friends work as Mac users expect
#endif
            const auto grid_before=julretsu::metrics::cpp_allocations.load();
            app.draw();
            window_frame.update(app.caption_height(),app.caption_enabled(),app.caption_items());
            if(frame>=warmup) grid_allocations+=julretsu::metrics::cpp_allocations.load()-grid_before;
            ImGui::Render();
            // Mismatched Begin/End calls are recovered silently in release builds; fail the smoke run instead.
            if(smoke&&GImGui->ErrorCountCurrentFrame>0) { std::cerr<<"ImGui usage error on frame "<<frame<<'\n'; smoke_ok=false; }
            int width{},height{}; glfwGetFramebufferSize(window.get(),&width,&height);
            glViewport(0,0,width,height); glClearColor(0.97f,0.98f,0.985f,1); glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            if(!manual_dir.empty()) {
                static const std::pair<int,const char*> shots[]{{5,"overview"},{10,"file-menu"},{17,"format-cells"},{26,"sheet-check"},{34,"review"},
                    {47,"cell-history"},{56,"ai-assistant"},{63,"lua"},{71,"chart-report"},{78,"workbook-floating"},{86,"dark-mode"},{93,"review-tab"}};
                for(const auto& [at,name]:shots) if(frame==at) screenshot(manual_dir+"/"+name+".bmp",width,height);
            }
            if(smoke&&frame==94) screenshot(capture+"-file-menu.bmp",width,height);
            if(smoke&&frame==107)screenshot(capture+"-cell-format.bmp",width,height);
            if(smoke&&frame==114)screenshot(capture+"-report-preview.bmp",width,height);
            if(smoke&&frame==120)screenshot(capture+"-workbook-floating.bmp",width,height);
            if(smoke&&frame==130)screenshot(capture+"-sheet-check.bmp",width,height);
            if(smoke&&frame==135)screenshot(capture+"-review.bmp",width,height);
            if(smoke&&frame==141)screenshot(capture+"-long-text.bmp",width,height);
            if(smoke&&frame==154)screenshot(capture+"-validation-en.bmp",width,height);
            if(smoke&&frame==158)screenshot(capture+"-validation-ko.bmp",width,height);
            if(smoke&&frame==162)screenshot(capture+"-validation-ja.bmp",width,height);
            if(smoke&&frame==167)screenshot(capture+"-restore-en.bmp",width,height);
            if(smoke&&frame==171)screenshot(capture+"-restore-ko.bmp",width,height);
            if(smoke&&frame==175)screenshot(capture+"-restore-ja.bmp",width,height);
            if(smoke&&frame==179)screenshot(capture+"-tables-en.bmp",width,height);
            if(smoke&&frame==183)screenshot(capture+"-tables-ko.bmp",width,height);
            if(smoke&&frame==187)screenshot(capture+"-tables-ja.bmp",width,height);
            if(smoke&&frame==191)screenshot(capture+"-lua-en.bmp",width,height);
            if(smoke&&frame==195)screenshot(capture+"-lua-ko.bmp",width,height);
            if(smoke&&frame==199)screenshot(capture+"-lua-ja.bmp",width,height);
            if(smoke&&frame==145)screenshot(capture+"-korean.bmp",width,height);
            if(smoke&&frame==149)screenshot(capture+"-japanese.bmp",width,height);
            if(smoke&&frame==124)screenshot(capture+"-workbook-minimized.bmp",width,height);
            if(smoke&&frame==101) screenshot(capture+"-ai-preview.bmp",width,height);
            if(smoke&&frame==1) screenshot(capture+"-light.bmp",width,height);
            if(smoke&&frame==78) screenshot(capture+"-dark.bmp",width,height);
            if(smoke&&frame==20) screenshot(capture+"-workspace.bmp",width,height);
            if(smoke&&frame==30) {
                screenshot(capture+"-last-cell.bmp",width,height);
                smoke_ok &= app.active()==julretsu::CellCoord{julretsu::max_rows-1,julretsu::max_columns-1};
                auto hit=app.viewport().hit(app.viewport().row_header+float(int(julretsu::max_columns)-1-app.viewport().first_column)*app.viewport().column_width+2,
                                          app.viewport().column_header+float(int(julretsu::max_rows)-1-app.viewport().first_row)*app.viewport().row_height+2);
                smoke_ok &= hit==app.active();
            }
            glfwSwapBuffers(window.get());
            if(frame>=warmup&&frame_limit) {
                frames.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());
                cpp_allocations+=julretsu::metrics::cpp_allocations.load()-cpp_before;
                imgui_allocations+=julretsu::metrics::imgui_allocations.load()-imgui_before;
            }
            ++frame; if(frame_limit&&frame>=frame_limit) break;
            if(!frame_limit&&height==0) glfwWaitEventsTimeout(0.05);
        }
        // A normal exit (work saved or deliberately discarded) needs no crash-recovery copy.
        if(!frame_limit) app.discard_recovery();
        if(frame_limit) {
            std::sort(frames.begin(),frames.end());
            auto percentile=[&](double p){return frames[std::min(frames.size()-1,std::size_t(std::ceil(double(frames.size())*p))-1)];};
            std::cout<<"Julretsu native "<<(smoke?"smoke":"benchmark")<<"\n"
                     <<"window=1580x960 dpi_scale="<<scale<<" populated_cells="<<app.sheet().populated_cells()
                     <<" logical_rows="<<julretsu::max_rows<<" samples="<<frames.size()<<" vsync=off\n"
                     <<"full_frame_ms p50="<<percentile(0.5)<<" p95="<<percentile(0.95)<<" p99="<<percentile(0.99)<<"\n"
                     <<"warmed_cpp_allocations="<<cpp_allocations<<" warmed_imgui_allocations="<<imgui_allocations
                     <<" warmed_draw_cpp_allocations="<<grid_allocations<<" process_memory_bytes="<<memory_bytes()<<"\n"
                     <<"renderer="<<glGetString(GL_RENDERER)<<"\n";
            if(smoke) { std::cout<<(smoke_ok?"PASS":"FAIL")<<" native smoke checks\n"; if(!smoke_ok) return 2; }
        }
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"Julretsu: "<<error.what()<<'\n';
#ifdef _WIN32
        if(!smoke&&!benchmark) MessageBoxA(nullptr,error.what(),"Julretsu startup error",MB_OK|MB_ICONERROR);
#endif
        return 1;
    }
}
