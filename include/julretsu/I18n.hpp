#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace julretsu {
// Interface languages. English text doubles as the lookup key, so untranslated text stays readable.
enum class Language : std::uint8_t { English, Korean, Japanese };
inline constexpr Language all_languages[]{Language::English,Language::Korean,Language::Japanese};
void set_language(Language language) noexcept;
[[nodiscard]] Language language() noexcept;
[[nodiscard]] const char* language_code(Language language) noexcept; // "en", "ko", "ja"
[[nodiscard]] const char* language_name(Language language) noexcept; // in its own script, e.g. "한국어"
[[nodiscard]] Language language_from_code(std::string_view code) noexcept; // unknown codes give English
[[nodiscard]] Language system_language(); // the operating system's interface language, if supported

// Translation of a fixed English string. A "##id" suffix is kept, so ImGui labels keep working.
[[nodiscard]] const char* tr(const char* english);
// Translation of stored or composed English text: an exact match, or "Known prefix: rest" translated in parts.
[[nodiscard]] std::string tr_text(std::string_view english);
// printf-style formatting with a translated format string. The English format is checked by the compiler.
#if defined(__GNUC__) || defined(__clang__)
[[nodiscard]] std::string trf(const char* english_format,...) __attribute__((format(printf,1,2)));
#else
[[nodiscard]] std::string trf(const char* english_format,...);
#endif

// Every translated string of a language, used to preload its glyphs.
[[nodiscard]] std::vector<std::string_view> translations(Language language);
// Format strings whose translation does not consume the same printf arguments. Empty means all are safe.
[[nodiscard]] std::vector<std::string> translation_problems();
}
