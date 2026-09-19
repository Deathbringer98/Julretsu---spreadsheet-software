#include "julretsu/Formula.hpp"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <new>
#include <set>
#include <stdexcept>

namespace julretsu {
namespace {
CellError error(ErrorCode code,std::string text) { return {code,std::move(text),{}}; }
enum class TokenKind { End, Number, String, Word, Symbol };
struct Token { TokenKind kind{}; std::string text; double number{}; };
class Parser {
    std::string_view source_;
    const Limits& limits_;
    std::size_t pos_{}, depth_{};
    Token token_;
    ParsedFormula result_;
    std::set<CellCoord> refs_;
    [[noreturn]] void fail(ErrorCode c,const std::string& s) const {
        throw error(c,s+" at byte "+std::to_string(pos_));
    }
    void next() {
        while(pos_<source_.size() && (source_[pos_]==' '||source_[pos_]=='\t'||source_[pos_]=='\r'||source_[pos_]=='\n')) ++pos_;
        token_={};
        if(pos_==source_.size()) return;
        char c=source_[pos_];
        if((c>='0'&&c<='9')||c=='.') {
            const char* first=source_.data()+pos_;
            auto [end,ec]=std::from_chars(first,source_.data()+source_.size(),token_.number);
            if(ec!=std::errc{} || end==first || !std::isfinite(token_.number)) fail(ErrorCode::Num,"Invalid numeric literal");
            pos_=static_cast<std::size_t>(end-source_.data()); token_.kind=TokenKind::Number; return;
        }
        if(c=='"') {
            ++pos_; token_.kind=TokenKind::String;
            while(pos_<source_.size()) {
                char d=source_[pos_++];
                if(d=='"') {
                    if(pos_<source_.size() && source_[pos_]=='"') { ++pos_; token_.text.push_back('"'); }
                    else return;
                } else token_.text.push_back(d);
            }
            fail(ErrorCode::Parse,"Unterminated string");
        }
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_') {
            token_.kind=TokenKind::Word;
            while(pos_<source_.size()) {
                char d=source_[pos_];
                if(!((d>='a'&&d<='z')||(d>='A'&&d<='Z')||(d>='0'&&d<='9')||d=='_')) break;
                token_.text.push_back(d>='a'&&d<='z'?static_cast<char>(d-'a'+'A'):d); ++pos_;
            } return;
        }
        token_.kind=TokenKind::Symbol; token_.text.push_back(c); ++pos_;
        if(pos_<source_.size() && ((c=='<'&&(source_[pos_]=='='||source_[pos_]=='>'))||(c=='>'&&source_[pos_]=='=')||(c=='!'&&source_[pos_]=='=')))
            token_.text.push_back(source_[pos_++]);
    }
    bool take(std::string_view s) {
        if(token_.kind!=TokenKind::Symbol || token_.text!=s) return false;
        next(); return true;
    }
    void expect(std::string_view s) { if(!take(s)) fail(ErrorCode::Parse,"Expected "+std::string(s)); }
    std::size_t add(AstNode node) {
        for(auto child:node.children) node.height=std::max(node.height,result_.nodes[child].height+1);
        if(result_.nodes.size()>=limits_.ast_nodes||node.height>limits_.parse_depth) fail(ErrorCode::Limit,"Formula AST limit");
        result_.nodes.push_back(std::move(node)); return result_.nodes.size()-1;
    }
    void ref(CellCoord c) {
        refs_.insert(c);
        if(refs_.size()>limits_.dependencies) fail(ErrorCode::Limit,"Formula dependency limit");
    }
    CellCoord coordinate(const std::string& s) {
        auto parsed=from_a1(s);
        if(auto e=std::get_if<CellError>(&parsed)) fail(e->code,e->context);
        return std::get<CellCoord>(parsed);
    }
    static int precedence(const Token& t) {
        if(t.kind!=TokenKind::Symbol) return 0;
        if(t.text=="="||t.text=="<>"||t.text=="!="||t.text=="<"||t.text==">"||t.text=="<="||t.text==">=") return 1;
        if(t.text=="+"||t.text=="-") return 2;
        if(t.text=="*"||t.text=="/") return 3;
        return 0;
    }
    std::size_t expression(int minimum=1) {
        if(++depth_>limits_.parse_depth) fail(ErrorCode::Limit,"Parse depth limit");
        auto left=primary();
        while(precedence(token_)>=minimum) {
            auto op=token_.text; auto p=precedence(token_); next();
            auto right=expression(p+1);
            AstNode node; node.kind=NodeKind::Binary; node.op=std::move(op); node.children={left,right}; left=add(std::move(node));
        }
        --depth_; return left;
    }
    std::size_t primary() {
        if(token_.kind==TokenKind::Symbol && (token_.text=="+"||token_.text=="-")) {
            auto op=token_.text; next();
            auto child=expression(4); AstNode node; node.kind=NodeKind::Unary; node.op=op; node.children={child}; return add(std::move(node));
        }
        if(take("(")) { auto n=expression(); expect(")"); return n; }
        AstNode node;
        if(token_.kind==TokenKind::Number) { node.literal=token_.number; next(); return add(std::move(node)); }
        if(token_.kind==TokenKind::String) { node.literal=token_.text; next(); return add(std::move(node)); }
        if(token_.kind!=TokenKind::Word) fail(ErrorCode::Parse,"Expected expression");
        auto word=token_.text; next();
        if(take("(")) {
            node.kind=NodeKind::Call; node.op=word;
            if(!take(")")) {
                do { node.children.push_back(expression()); } while(take(","));
                expect(")");
            }
            return add(std::move(node));
        }
        if(word=="TRUE"||word=="FALSE") { node.literal=(word=="TRUE"); return add(std::move(node)); }
        node.kind=NodeKind::Reference; node.first=coordinate(word); ref(node.first);
        if(take(":")) {
            if(token_.kind!=TokenKind::Word) fail(ErrorCode::Ref,"Expected range end");
            auto end=coordinate(token_.text); next();
            node.kind=NodeKind::Range;
            node.last={std::max(node.first.row,end.row),std::max(node.first.column,end.column)};
            node.first={std::min(node.first.row,end.row),std::min(node.first.column,end.column)};
            auto count=std::uint64_t(node.last.row-node.first.row+1)*(node.last.column-node.first.column+1);
            if(count>limits_.range_cells) fail(ErrorCode::Limit,"Range expansion limit");
            for(auto r=node.first.row;r<=node.last.row;++r)
                for(auto c=node.first.column;c<=node.last.column;++c) ref({r,c});
        }
        return add(std::move(node));
    }
public:
    Parser(std::string_view source,const Limits& limits):source_(source),limits_(limits) {}
    ParsedFormula run() {
        try {
            if(source_.size()>limits_.formula_bytes) fail(ErrorCode::Limit,"Formula byte limit");
            if(source_.empty()||source_[0]!='=') fail(ErrorCode::Parse,"Formula must start with =");
            pos_=1; next(); result_.root=expression();
            if(token_.kind!=TokenKind::End) fail(ErrorCode::Parse,"Trailing input");
            result_.precedents.assign(refs_.begin(),refs_.end());
        } catch(const CellError& e) { result_={}; result_.error=e; }
        return std::move(result_);
    }
};
Value numeric(const Value& v) {
    if(std::holds_alternative<std::monostate>(v)) return 0.0;
    if(auto b=std::get_if<bool>(&v)) return *b?1.0:0.0;
    if(std::holds_alternative<double>(v)||std::holds_alternative<CellError>(v)) return v;
    return error(ErrorCode::Value,"Text cannot be used as a number");
}
class Evaluator {
    const ParsedFormula& formula_;
    const CellReader& read_;
    const Limits& limits_;
    const FormulaExtension* extension_;
    const ExtensionContext* context_;
    std::size_t work_{};
    void tick() { if(++work_>limits_.evaluation_work) throw error(ErrorCode::Limit,"Evaluation work limit"); }
    Value reference(CellCoord c) {
        tick(); auto v=read_(c);
        if(auto e=std::get_if<CellError>(&v);e && e->code==ErrorCode::Cycle) { e->code=ErrorCode::CycleDependency; e->context="Depends on a cycle"; }
        return v;
    }
    Value finite(double d) { return std::isfinite(d)?Value(d):Value(error(ErrorCode::Num,"Non-finite result")); }
    Value eval(std::size_t index) {
        tick(); const auto& n=formula_.nodes[index];
        switch(n.kind) {
        case NodeKind::Literal:return n.literal;
        case NodeKind::Reference:return reference(n.first);
        case NodeKind::Range:return error(ErrorCode::Value,"Range requires an aggregate function");
        case NodeKind::Unary: {
            auto a=numeric(eval(n.children[0])); if(auto p=std::get_if<double>(&a)) return finite(n.op=="-"?-*p:*p); return a;
        }
        case NodeKind::Binary: {
            auto a=eval(n.children[0]); if(std::holds_alternative<CellError>(a)) return a;
            auto b=eval(n.children[1]); if(std::holds_alternative<CellError>(b)) return b;
            if(n.op=="+"||n.op=="-"||n.op=="*"||n.op=="/") {
                a=numeric(a); b=numeric(b);
                if(std::holds_alternative<CellError>(a)) return a;
                if(std::holds_alternative<CellError>(b)) return b;
                auto x=std::get<double>(a),y=std::get<double>(b);
                if(n.op=="+") return finite(x+y); if(n.op=="-") return finite(x-y); if(n.op=="*") return finite(x*y);
                if(y==0) return error(ErrorCode::DivZero,"Division by zero"); return finite(x/y);
            }
            int compare=0;
            if(auto x=std::get_if<std::string>(&a)) {
                auto y=std::get_if<std::string>(&b); if(!y) return error(ErrorCode::Value,"Cannot compare text with a non-text value");
                compare=x->compare(*y);
            } else {
                a=numeric(a); b=numeric(b);
                if(std::holds_alternative<CellError>(a)) return a; if(std::holds_alternative<CellError>(b)) return b;
                auto left=std::get<double>(a),right=std::get<double>(b); compare=left<right?-1:(left>right?1:0);
            }
            if(n.op=="=") return compare==0; if(n.op=="<>"||n.op=="!=") return compare!=0;
            if(n.op=="<") return compare<0; if(n.op==">") return compare>0; if(n.op=="<=") return compare<=0; return compare>=0;
        }
        case NodeKind::Call:break;
        }
        if(n.op=="IF") {
            if(n.children.size()!=3) return error(ErrorCode::Value,"IF requires three arguments");
            auto condition=eval(n.children[0]);
            if(std::holds_alternative<CellError>(condition)) return condition;
            if(!std::holds_alternative<double>(condition)&&!std::holds_alternative<bool>(condition))
                return error(ErrorCode::Value,"IF condition must be Boolean or numeric");
            return eval(n.children[std::get<double>(numeric(condition))!=0?1:2]);
        }
        if(n.op=="SUM"||n.op=="AVERAGE") {
            if(n.children.empty()) return error(ErrorCode::Value,"Aggregate requires at least one argument");
            double sum=0; std::size_t count=0;
            auto consume=[&](const Value& v) {
                if(auto e=std::get_if<CellError>(&v)) throw *e;
                if(auto d=std::get_if<double>(&v)) { sum+=*d; ++count; if(!std::isfinite(sum)) throw error(ErrorCode::Num,"Aggregate overflow"); }
            };
            for(auto child:n.children) {
                const auto& arg=formula_.nodes[child];
                if(arg.kind==NodeKind::Range) {
                    for(auto r=arg.first.row;r<=arg.last.row;++r)
                        for(auto c=arg.first.column;c<=arg.last.column;++c) consume(reference({r,c}));
                } else consume(eval(child));
            }
            if(n.op=="AVERAGE"&&!count) return error(ErrorCode::DivZero,"AVERAGE has no numeric values");
            return finite(n.op=="SUM"?sum:sum/static_cast<double>(count));
        }
        if(n.op=="LUA") {
            if(!extension_) return error(ErrorCode::Unsupported,"Lua is not enabled for this sheet");
            std::vector<Value> args; args.reserve(n.children.size());
            for(auto child:n.children) { auto v=eval(child); if(std::holds_alternative<CellError>(v)) return v; args.push_back(std::move(v)); }
            auto v=context_ ? extension_->evaluate_with_context(args,limits_.evaluation_work-work_,*context_)
                            : extension_->evaluate(args,limits_.evaluation_work-work_);
            if(auto d=std::get_if<double>(&v);d&&!std::isfinite(*d)) return error(ErrorCode::Num,"Extension returned non-finite number");
            if(auto s=std::get_if<std::string>(&v);s&&s->size()>limits_.text_bytes) return error(ErrorCode::Limit,"Extension output limit");
            return v;
        }
        return error(ErrorCode::Name,"Unknown function: "+n.op);
    }
public:
    Evaluator(const ParsedFormula& f,const CellReader& r,const Limits& l,const FormulaExtension* e,const ExtensionContext* context)
        :formula_(f),read_(r),limits_(l),extension_(e),context_(context) {}
    Value run() {
        if(formula_.error) return *formula_.error;
        try { return eval(formula_.root); }
        catch(const CellError& e) { return e; }
        catch(const std::bad_alloc&) { throw; }
        catch(const std::exception& e) { return error(ErrorCode::Value,std::string("Extension failure: ")+e.what()); }
        catch(...) { return error(ErrorCode::Value,"Extension threw an unknown exception"); }
    }
};
}
ParsedFormula parse_formula(std::string_view source,const Limits& limits) { return Parser(source,limits).run(); }
Value evaluate_formula(const ParsedFormula& f,const CellReader& read,const Limits& l,const FormulaExtension* ext,const ExtensionContext* context) {
    return Evaluator(f,read,l,ext,context).run();
}
}
