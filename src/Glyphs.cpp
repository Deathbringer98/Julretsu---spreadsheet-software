#include "julretsu/Glyphs.hpp"
#include <set>
namespace julretsu::glyphs {
namespace {
std::set<std::uint32_t> noted;
bool changed=false;
}
void note(std::string_view text) {
    for(std::size_t i=0;i<text.size();) {
        const auto lead=static_cast<unsigned char>(text[i]);
        if(lead<0x80) { ++i; continue; } // ASCII, the common case, is always in the font
        const int length=lead>=0xF0?4:lead>=0xE0?3:lead>=0xC0?2:1;
        if(length==1||i+std::size_t(length)>text.size()) { ++i; continue; }
        std::uint32_t c=lead&(length==2?0x1F:length==3?0x0F:0x07); bool valid=true;
        for(int k=1;k<length;++k) { const auto next=static_cast<unsigned char>(text[i+std::size_t(k)]); valid&=(next&0xC0)==0x80; c=(c<<6)|(next&0x3F); }
        i+=valid?std::size_t(length):1;
        // ImGui stores characters in 16 bits here, so only the Basic Multilingual Plane can be drawn.
        if(valid&&c<=0xFFFF&&!built_in(c)&&noted.insert(c).second) changed=true;
    }
}
void note(const char* text) { if(text) note(std::string_view(text)); }
bool take_changes() noexcept { const bool result=changed; changed=false; return result; }
std::vector<std::uint32_t> extra() { return {noted.begin(),noted.end()}; }
}
