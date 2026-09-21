#include "julretsu/Json.hpp"
#include "julretsu/Types.hpp"
#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <stdexcept>
namespace julretsu {
const Json* Json::find(std::string_view key) const {
    if(auto o=object()) for(const auto& [name,item]:*o) if(name==key) return &item;
    return nullptr;
}
namespace {
struct Parser {
    std::string_view text; std::size_t at=0, max_depth;
    [[noreturn]] void fail(const char* why) const { throw std::runtime_error(std::string("Invalid JSON: ")+why); }
    void space() { while(at<text.size()&&(text[at]==' '||text[at]=='\t'||text[at]=='\n'||text[at]=='\r')) ++at; }
    bool take(char c) { space(); if(at<text.size()&&text[at]==c) { ++at; return true; } return false; }
    void expect(char c) { if(!take(c)) fail("unexpected character"); }
    bool word(std::string_view w) { if(text.substr(at,w.size())==w) { at+=w.size(); return true; } return false; }
    static void utf8(std::string& out,unsigned code) {
        if(code<0x80) out+=char(code);
        else if(code<0x800) { out+=char(0xc0|(code>>6)); out+=char(0x80|(code&63)); }
        else if(code<0x10000) { out+=char(0xe0|(code>>12)); out+=char(0x80|((code>>6)&63)); out+=char(0x80|(code&63)); }
        else { out+=char(0xf0|(code>>18)); out+=char(0x80|((code>>12)&63)); out+=char(0x80|((code>>6)&63)); out+=char(0x80|(code&63)); }
    }
    unsigned hex4() {
        if(at+4>text.size()) fail("short unicode escape");
        unsigned n{}; auto [end,e]=std::from_chars(text.data()+at,text.data()+at+4,n,16);
        if(e!=std::errc{}||end!=text.data()+at+4) fail("bad unicode escape");
        at+=4; return n;
    }
    std::string string() {
        expect('"'); std::string out;
        while(true) {
            if(at>=text.size()) fail("unterminated string");
            const char c=text[at++];
            if(c=='"') return out;
            if(static_cast<unsigned char>(c)<0x20) fail("control character in string");
            if(c!='\\') { out+=c; continue; }
            if(at>=text.size()) fail("bad escape");
            switch(text[at++]) {
            case '"': out+='"'; break; case '\\': out+='\\'; break; case '/': out+='/'; break;
            case 'b': out+='\b'; break; case 'f': out+='\f'; break; case 'n': out+='\n'; break;
            case 'r': out+='\r'; break; case 't': out+='\t'; break;
            case 'u': {
                unsigned code=hex4();
                if(code>=0xd800&&code<=0xdbff) {
                    if(!word("\\u")) fail("unpaired surrogate");
                    const unsigned low=hex4(); if(low<0xdc00||low>0xdfff) fail("unpaired surrogate");
                    code=0x10000+((code-0xd800)<<10)+(low-0xdc00);
                } else if(code>=0xdc00&&code<=0xdfff) fail("unpaired surrogate");
                utf8(out,code); break;
            }
            default: fail("bad escape");
            }
        }
    }
    Json value(std::size_t depth) {
        if(depth>max_depth) fail("nesting too deep");
        space(); if(at>=text.size()) fail("unexpected end");
        const char c=text[at];
        if(c=='{') {
            ++at; JsonObject o; if(take('}')) return o;
            do { space(); auto key=string(); expect(':'); o.emplace_back(std::move(key),value(depth+1)); } while(take(','));
            expect('}'); return o;
        }
        if(c=='[') {
            ++at; JsonArray a; if(take(']')) return a;
            do a.push_back(value(depth+1)); while(take(','));
            expect(']'); return a;
        }
        if(c=='"') return string();
        if(word("true")) return true;
        if(word("false")) return false;
        if(word("null")) return nullptr;
        std::size_t end=at; while(end<text.size()&&std::string_view("+-.0123456789eE").find(text[end])!=std::string_view::npos) ++end;
        double n{}; auto [stop,e]=parse_double(text.data()+at,text.data()+end,n);
        if(end==at||e!=std::errc{}||stop!=text.data()+end||!std::isfinite(n)) fail("bad number");
        at=end; return n;
    }
};
void quote(std::string& out,std::string_view s) {
    out+='"';
    for(unsigned char c:s) {
        if(c=='"') out+="\\\""; else if(c=='\\') out+="\\\\";
        else if(c=='\n') out+="\\n"; else if(c=='\r') out+="\\r"; else if(c=='\t') out+="\\t";
        else if(c<0x20) { char b[8]; std::snprintf(b,sizeof b,"\\u%04x",c); out+=b; }
        else out+=char(c);
    }
    out+='"';
}
void write(std::string& out,const Json& j) {
    if(std::holds_alternative<std::nullptr_t>(j.value)) out+="null";
    else if(auto b=std::get_if<bool>(&j.value)) out+=*b?"true":"false";
    else if(auto n=std::get_if<double>(&j.value)) {
        std::array<char,64> buffer{}; auto [end,e]=std::to_chars(buffer.data(),buffer.data()+buffer.size(),*n);
        if(e!=std::errc{}||!std::isfinite(*n)) throw std::runtime_error("Cannot write JSON number."); out.append(buffer.data(),end);
    }
    else if(auto s=j.string()) quote(out,*s);
    else if(auto a=j.array()) { out+='['; for(std::size_t i=0;i<a->size();++i) { if(i) out+=','; write(out,(*a)[i]); } out+=']'; }
    else if(auto o=j.object()) { out+='{'; for(std::size_t i=0;i<o->size();++i) { if(i) out+=','; quote(out,(*o)[i].first); out+=':'; write(out,(*o)[i].second); } out+='}'; }
}
}
Json parse_json(std::string_view text,std::size_t max_depth) {
    Parser p{text,0,max_depth}; auto result=p.value(0); p.space();
    if(p.at!=text.size()) p.fail("trailing characters");
    return result;
}
std::string dump_json(const Json& value) { std::string out; write(out,value); return out; }
}
