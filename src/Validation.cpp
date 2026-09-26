#include "julretsu/GridCore.hpp"
#include <cmath>
#include <set>
#include <map>
#include <sstream>
#include <locale>
#include <limits>
namespace julretsu {
namespace {
bool date(std::string_view s) {
    if(s.size()!=10||s[4]!='-'||s[7]!='-')return false;
    for(unsigned i=0;i<10;++i)if(i!=4&&i!=7&&(s[i]<'0'||s[i]>'9'))return false;
    int y=0,m=0,d=0;
    for(int i=0;i<4;++i)y=y*10+s[i]-'0';
    m=(s[5]-'0')*10+s[6]-'0';d=(s[8]-'0')*10+s[9]-'0';
    if(y<1||m<1||m>12||d<1)return false;
    int days[]{31,28,31,30,31,30,31,31,30,31,30,31};
    return d<=days[m-1]+(m==2&&y%4==0&&(y%100!=0||y%400==0));
}
std::string unique_key(const Value& v) {
    double number=0;bool numeric=false;
    if(auto n=std::get_if<double>(&v)){number=*n;numeric=true;}
    else if(auto s=std::get_if<std::string>(&v);s&&!s->empty()) {
        auto parsed=parse_double(s->data(),s->data()+s->size(),number);
        numeric=parsed.ec==std::errc{}&&parsed.ptr==s->data()+s->size()&&std::isfinite(number);
    }
    if(numeric) {std::ostringstream out;out.imbue(std::locale::classic());out.precision(std::numeric_limits<double>::max_digits10);out<<(number==0?0:number);return "n:"+out.str();}
    return "t:"+display(v);
}
bool blank(const Value& v) {auto s=std::get_if<std::string>(&v);return std::holds_alternative<std::monostate>(v)||(s&&s->empty());}
}
bool valid_rule(const ValidationRule& r) {
    if(!r.first.valid()||!r.last.valid()||r.first.row>r.last.row||r.first.column>r.last.column||unsigned(r.kind)>unsigned(ValidationKind::List))return false;
    if(!std::isfinite(r.minimum)||!std::isfinite(r.maximum)||r.minimum>r.maximum)return false;
    if(!date(r.date_min)||!date(r.date_max)||r.date_min>r.date_max)return false;
    if(r.choices.size()>100||(r.kind==ValidationKind::List&&r.choices.empty()))return false;
    std::set<std::string> seen;
    for(const auto& s:r.choices)if(s.empty()||s.size()>256||s.find('\0')!=std::string::npos||!seen.insert(s).second)return false;
    return true;
}
std::vector<ValidationIssue> check_rules(const std::vector<ValidationRule>& rules,const std::function<Value(CellCoord)>& read) {
    std::vector<ValidationIssue> issues;
    for(const auto& rule:rules) {
        std::map<std::string,CellCoord> seen;
        for(unsigned r=rule.first.row;r<=rule.last.row;++r)for(unsigned c=rule.first.column;c<=rule.last.column;++c) {
            CellCoord coord{r,c};auto v=read(coord);const char* reason=nullptr;
            if(blank(v)) {if(rule.required)reason="A value is required.";}
            else {
                auto n=std::get_if<double>(&v);auto s=std::get_if<std::string>(&v);
                if(rule.kind==ValidationKind::Number||rule.kind==ValidationKind::WholeNumber) {
                    if(!n||*n<rule.minimum||*n>rule.maximum||(rule.kind==ValidationKind::WholeNumber&&std::floor(*n)!=*n))reason="Enter a number within the configured limits.";
                } else if(rule.kind==ValidationKind::Date) {
                    if(!s||!date(*s)||*s<rule.date_min||*s>rule.date_max)reason="Enter a valid date as YYYY-MM-DD within the configured limits.";
                } else if(rule.kind==ValidationKind::List) {
                    bool found=false;for(const auto& choice:rule.choices)if(display(v)==choice)found=true;
                    if(!found)reason="Choose one of the allowed values.";
                }
                if(rule.unique) {
                    // Treat numeric text and the corresponding displayed number as duplicates.
                    auto [it,inserted]=seen.emplace(unique_key(v),coord);
                    if(!inserted) {reason="This value is duplicated in the rule range.";issues.push_back({it->second,reason,rule.warning});}
                }
            }
            if(reason)issues.push_back({coord,reason,rule.warning});
        }
    }
    return issues;
}
}
