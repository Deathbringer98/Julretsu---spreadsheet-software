#include "julretsu/Branding.hpp"
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace julretsu {
namespace {
using Pixels=std::unique_ptr<unsigned char,decltype(&stbi_image_free)>;
struct Image { int width{},height{}; Pixels pixels{nullptr,stbi_image_free}; };
Image load(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input) throw std::runtime_error("Branding image not found: "+path.filename().string());
    auto size=input.tellg();
    if(size<=0||size>16*1024*1024) throw std::runtime_error("Branding image size limit");
    std::vector<unsigned char> data(static_cast<std::size_t>(size));
    input.seekg(0); input.read(reinterpret_cast<char*>(data.data()),std::streamsize(data.size()));
    if(!input) throw std::runtime_error("Cannot read branding image");
    Image image; int channels{};
    if(!stbi_info_from_memory(data.data(),int(data.size()),&image.width,&image.height,&channels)||
       image.width<=0||image.height<=0||image.width>4096||image.height>4096)
        throw std::runtime_error("Invalid or oversized branding image");
    image.pixels.reset(stbi_load_from_memory(data.data(),int(data.size()),&image.width,&image.height,&channels,4));
    if(!image.pixels) throw std::runtime_error("Cannot decode branding PNG");
    return image;
}
}
std::filesystem::path Branding::asset_directory() {
    std::filesystem::path executable_dir;
#ifdef _WIN32
    std::array<wchar_t,32768> path{};
    const auto n=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));
    if(n&&n<path.size()) executable_dir=std::filesystem::path(path.data()).parent_path();
#elif defined(__APPLE__)
    std::uint32_t size=0; _NSGetExecutablePath(nullptr,&size);
    std::vector<char> path(size);
    if(_NSGetExecutablePath(path.data(),&size)==0) executable_dir=std::filesystem::weakly_canonical(path.data()).parent_path();
#else
    std::array<char,4096> path{};
    const auto n=readlink("/proc/self/exe",path.data(),path.size()-1);
    if(n>0) executable_dir=std::filesystem::path(std::string(path.data(),std::size_t(n))).parent_path();
#endif
    // Beside the program (Windows, portable), in a macOS app bundle, or in a Linux /usr or /opt layout.
    if(!executable_dir.empty())
        for(const auto& candidate:{executable_dir/"assets",executable_dir/".."/"Resources"/"assets",executable_dir/".."/"share"/"julretsu"/"assets"}) {
            std::error_code error;
            if(std::filesystem::exists(candidate/"app-icon.png",error)) return candidate;
        }
    return executable_dir.empty()?std::filesystem::current_path()/"assets":executable_dir/"assets";
}
Branding::Branding(GLFWwindow* window) {
    const auto assets=asset_directory();
    try {
        auto icon=load(assets/"app-icon.png");
#ifndef __APPLE__
        GLFWimage image{icon.width,icon.height,icon.pixels.get()};
        glfwSetWindowIcon(window,1,&image);
#else
        (void)window; // macOS application icons belong to an eventual .app bundle.
#endif
        glGenTextures(1,&icon_texture_); glBindTexture(GL_TEXTURE_2D,icon_texture_);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,icon.width,icon.height,0,GL_RGBA,GL_UNSIGNED_BYTE,icon.pixels.get());
        glBindTexture(GL_TEXTURE_2D,0);
        icon_loaded_=true;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; }
    try {
        auto banner=load(assets/"loading-banner.png");
        width_=banner.width; height_=banner.height;
        glGenTextures(1,&texture_); glBindTexture(GL_TEXTURE_2D,texture_);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        // OpenGL 1.2 token, omitted by Windows' system OpenGL 1.1 header.
        constexpr GLint clamp_to_edge=0x812F;
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,clamp_to_edge);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,clamp_to_edge);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,banner.width,banner.height,0,GL_RGBA,GL_UNSIGNED_BYTE,banner.pixels.get());
        glBindTexture(GL_TEXTURE_2D,0);
        glPixelStorei(GL_UNPACK_ALIGNMENT,4);
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; }
}
Branding::~Branding() { if(icon_texture_) glDeleteTextures(1,&icon_texture_); if(texture_) glDeleteTextures(1,&texture_); }
void Branding::draw(const char* status) const {
    const auto* viewport=ImGui::GetMainViewport();
    auto* draw=ImGui::GetForegroundDrawList();
    const auto origin=viewport->Pos,size=viewport->Size;
    draw->AddRectFilled(origin,{origin.x+size.x,origin.y+size.y},IM_COL32(255,255,255,255));
    const float available_width=std::max(1.0f,size.x-48);
    const float available_height=std::max(1.0f,size.y-108);
    const float ratio=texture_?std::min(available_width/float(width_),available_height/float(height_)):1;
    const float w=texture_?float(width_)*ratio:320;
    const float h=texture_?float(height_)*ratio:160;
    const ImVec2 position{origin.x+(size.x-w)*0.5f,origin.y+24};
    if(texture_) {
        draw->AddImage(static_cast<ImTextureID>(texture_),position,{position.x+w,position.y+h});
    } else {
        draw->AddText({origin.x+(size.x-70)*0.5f,origin.y+size.y*0.45f},IM_COL32(20,80,150,255),"Julretsu");
    }
    const auto text_size=ImGui::CalcTextSize(status);
    const float baseline=origin.y+size.y-62;
    draw->AddText({origin.x+(size.x-text_size.x)*0.5f,baseline},IM_COL32(62,82,106,255),status);
    // Indeterminate activity marker, never a fabricated completion percentage.
    const float track_width=std::min(220.0f,size.x-48);
    const float x=origin.x+(size.x-track_width)*0.5f,y=baseline+30;
    draw->AddRectFilled({x,y},{x+track_width,y+3},IM_COL32(224,232,240,255),2);
    const float t=float(std::fmod(ImGui::GetTime()*0.7,1.0));
    draw->AddRectFilled({x+t*(track_width-44),y},{x+t*(track_width-44)+44,y+3},IM_COL32(12,111,223,255),2);
}
}
