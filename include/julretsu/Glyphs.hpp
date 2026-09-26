#pragma once
#include <cstdint>
#include <string_view>
#include <vector>
namespace julretsu::glyphs {
// Korean and Japanese fonts hold tens of thousands of characters, far too many to put in one font
// texture. Text that is shown is noted here, and the font texture is rebuilt with any new characters.
[[nodiscard]] constexpr bool built_in(std::uint32_t c) noexcept {
    return (c>=0x20&&c<=0x24F)||(c>=0x2010&&c<=0x205E)||(c>=0x20A0&&c<=0x20BF)||c==0x2122||(c>=0x2190&&c<=0x2193)||c==0xFFFD;
}
void note(std::string_view utf8);
void note(const char* utf8);
// True once after new characters were noted.
[[nodiscard]] bool take_changes() noexcept;
// Every noted character outside the built-in ranges, sorted.
[[nodiscard]] std::vector<std::uint32_t> extra();
}
