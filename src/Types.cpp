#include "julretsu/Types.hpp"
#include <algorithm>
#include <charconv>
#include <sstream>
namespace julretsu {
CoordResult from_a1(std::string_view s) {
    auto invalid = [] { return CellError{ErrorCode::Ref, "Invalid or out-of-bounds A1 reference", {}}; };
    if(s.empty()) return invalid();
    std::uint32_t col=0, row=0;
    std::size_t i=0;
    for(; i<s.size(); ++i) {
        char c=s[i];
        if(c>='a' && c<='z') c=static_cast<char>(c-'a'+'A');
        if(c<'A' || c>'Z') break;
        auto digit=static_cast<std::uint32_t>(c-'A'+1);
        if(col>(max_columns-digit)/26) return invalid();
        col=col*26+digit;
    }
    if(col==0 || i==s.size() || s[i]=='0') return invalid();
    for(;i<s.size();++i) {
        if(s[i]<'0'||s[i]>'9') return invalid();
        auto digit=static_cast<std::uint32_t>(s[i]-'0');
        if(row>(max_rows-digit)/10) return invalid();
        row=row*10+digit;
    }
    if(row==0 || col>max_columns || row>max_rows) return invalid();
    return CellCoord{row-1,col-1};
}
std::variant<std::string,CellError> to_a1(CellCoord c) {
    if(!c.valid()) return CellError{ErrorCode::Ref,"Coordinate out of bounds",c};
    std::string result; auto n=c.column+1;
    while(n) { result.push_back(static_cast<char>('A'+(n-1)%26)); n=(n-1)/26; }
    std::reverse(result.begin(),result.end());
    return result+std::to_string(c.row+1);
}
std::string_view error_display(ErrorCode c) noexcept {
    switch(c) {
    case ErrorCode::Ref:return "#REF!"; case ErrorCode::Cycle:return "#CYCLE!";
    case ErrorCode::CycleDependency:return "#CYCLE-DEPENDENCY!";
    case ErrorCode::Name:return "#NAME?"; case ErrorCode::Value:return "#VALUE!";
    case ErrorCode::DivZero:return "#DIV/0!"; case ErrorCode::Num:return "#NUM!";
    case ErrorCode::Parse:return "#PARSE!"; case ErrorCode::Limit:return "#LIMIT!";
    case ErrorCode::Unsupported:return "#UNSUPPORTED!";
    }
    return "#ERROR!";
}
std::string display(const Value& v) {
    if(auto p=std::get_if<double>(&v)) { std::ostringstream s; s.precision(15); s<<*p; return s.str(); }
    if(auto p=std::get_if<bool>(&v)) return *p?"TRUE":"FALSE";
    if(auto p=std::get_if<std::string>(&v)) return *p;
    if(auto p=std::get_if<CellError>(&v)) return std::string(error_display(p->code));
    return {};
}
}
