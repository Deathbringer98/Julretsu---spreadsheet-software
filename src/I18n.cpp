#include "julretsu/I18n.hpp"
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <regex>
#include <unordered_map>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif
namespace julretsu {
namespace detail {
struct Translation { const char* english; const char* korean; const char* japanese; };
extern const Translation translation_table[];
extern const std::size_t translation_count;
}
namespace {
std::atomic<Language> current{Language::English};
const std::unordered_map<std::string_view,const detail::Translation*>& index() {
    static const auto table=[] {
        std::unordered_map<std::string_view,const detail::Translation*> map;
        for(std::size_t i=0;i<detail::translation_count;++i) map.emplace(detail::translation_table[i].english,&detail::translation_table[i]);
        return map;
    }();
    return table;
}
const char* pick(const detail::Translation& entry,Language language) {
    const char* text=language==Language::Korean?entry.korean:language==Language::Japanese?entry.japanese:entry.english;
    return text&&*text?text:nullptr;
}
const char* lookup(std::string_view english,Language language) {
    if(language==Language::English) return nullptr;
    const auto& map=index(); const auto found=map.find(english);
    return found==map.end()?nullptr:pick(*found->second,language);
}
#ifndef _WIN32
Language from_locale_name(std::string_view name) {
    if(name.starts_with("ko")) return Language::Korean;
    if(name.starts_with("ja")) return Language::Japanese;
    return Language::English;
}
#endif
}
void set_language(Language language) noexcept { current=language; }
Language language() noexcept { return current; }
const char* language_code(Language language) noexcept {
    return language==Language::Korean?"ko":language==Language::Japanese?"ja":"en";
}
const char* language_name(Language language) noexcept {
    return language==Language::Korean?"한국어":language==Language::Japanese?"日本語":"English";
}
Language language_from_code(std::string_view code) noexcept {
    for(auto language:all_languages) if(code==language_code(language)) return language;
    return Language::English;
}
Language system_language() {
#ifdef _WIN32
    const auto primary=PRIMARYLANGID(GetUserDefaultUILanguage());
    if(primary==LANG_KOREAN) return Language::Korean;
    if(primary==LANG_JAPANESE) return Language::Japanese;
    return Language::English;
#else
#ifdef __APPLE__
    // The first preferred language from System Settings; apps started from Finder get no LANG.
    Language preferred=Language::English; bool known=false;
    if(CFArrayRef languages=CFLocaleCopyPreferredLanguages()) {
        if(CFArrayGetCount(languages)>0) {
            char name[32]{};
            if(CFStringGetCString(static_cast<CFStringRef>(CFArrayGetValueAtIndex(languages,0)),name,sizeof name,kCFStringEncodingUTF8)) { preferred=from_locale_name(name); known=true; }
        }
        CFRelease(languages);
    }
    if(known) return preferred;
#endif
    for(const char* variable:{"LC_ALL","LC_MESSAGES","LANG"})
        if(const char* value=std::getenv(variable); value&&*value) return from_locale_name(value);
    return Language::English;
#endif
}
const char* tr(const char* english) {
    const auto language=current.load();
    if(!english||language==Language::English) return english;
    const std::string_view text(english); const auto id=text.find("##");
    const char* translated=lookup(id==std::string_view::npos?text:text.substr(0,id),language);
    if(!translated) return english;
    if(id==std::string_view::npos) return translated;
    // Labels such as "Bold##ribbon" keep their ID suffix; results are cached so the pointer stays valid.
    static std::unordered_map<std::string,std::string> labels[3];
    auto& cache=labels[std::size_t(language)];
    auto found=cache.find(english);
    if(found==cache.end()) found=cache.emplace(english,std::string(translated)+std::string(text.substr(id))).first;
    return found->second.c_str();
}
std::string tr_text(std::string_view english) {
    const auto language=current.load();
    if(language==Language::English) return std::string(english);
    if(const char* translated=lookup(english,language)) return translated;
    // Messages are often "Known prefix: detail", such as "Recovery copy failed: " followed by an error.
    const auto colon=english.find(": ");
    if(colon!=std::string_view::npos) if(const char* prefix=lookup(english.substr(0,colon+2),language)) return prefix+tr_text(english.substr(colon+2));
    return std::string(english);
}
std::string trf(const char* english_format,...) {
    const char* format=tr(english_format);
    va_list arguments; va_start(arguments,english_format);
    va_list copy; va_copy(copy,arguments);
    const int length=std::vsnprintf(nullptr,0,format,copy); va_end(copy);
    std::string out(length>0?std::size_t(length):0,'\0');
    if(length>0) std::vsnprintf(out.data(),out.size()+1,format,arguments);
    va_end(arguments);
    return out;
}
std::vector<std::string_view> translations(Language language) {
    std::vector<std::string_view> out;
    for(std::size_t i=0;i<detail::translation_count;++i) if(const char* text=pick(detail::translation_table[i],language)) out.emplace_back(text);
    return out;
}
std::vector<std::string> translation_problems() {
    // A translation must consume the same printf arguments in the same order. "%.0s" consumes a string
    // without printing it, which Korean and Japanese use for English plural endings.
    static const std::regex spec(R"(%[-+ #0]*(?:\*|\d+)?(?:\.(?:\*|\d+))?(hh|h|ll|l|z|j|t|L)?([diouxXfFeEgGaAcsp%]))");
    auto signature=[](const char* text) {
        std::string out; const std::string value(text);
        for(std::sregex_iterator it(value.begin(),value.end(),spec),end;it!=end;++it) if((*it)[2]!="%") out+=(*it)[1].str()+(*it)[2].str()+' ';
        return out;
    };
    std::vector<std::string> problems;
    for(std::size_t i=0;i<detail::translation_count;++i) {
        const auto& entry=detail::translation_table[i]; const auto expected=signature(entry.english);
        for(const char* translated:{entry.korean,entry.japanese}) if(translated&&*translated&&signature(translated)!=expected) problems.push_back(std::string(entry.english)+" -> "+translated);
    }
    return problems;
}
}
