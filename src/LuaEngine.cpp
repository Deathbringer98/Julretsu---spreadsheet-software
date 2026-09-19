#include "julretsu/LuaEngine.hpp"
#include <sol/sol.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <stdexcept>

namespace julretsu {
namespace {
struct Memory {
    std::size_t used{}, limit{};
    bool exhausted{};
};
void* allocate(void* ud,void* ptr,std::size_t old_size,std::size_t size) noexcept {
    auto& m=*static_cast<Memory*>(ud);
    if(!ptr) old_size=0;
    if(!size) { m.used-=old_size; std::free(ptr); return nullptr; }
    if(size>m.limit || m.used-old_size>m.limit-size) { m.exhausted=true; return nullptr; }
    auto* result=std::realloc(ptr,size);
    if(result) m.used=m.used-old_size+size;
    else m.exhausted=true;
    return result;
}
struct Run {
    const LuaLimits& limits;
    const CellReader& reader;
    bool macro{};
    std::size_t instructions{}, callbacks{}, output_bytes{};
    bool exhausted{};
    Batch batch;
    std::set<std::uint32_t> rows;
    std::optional<CellError> error;
    Value scratch;
};
Run& run(lua_State* state) { return **static_cast<Run**>(lua_getextraspace(state)); }
int raise(lua_State* state) {
    lua_pushliteral(state,"Julretsu host operation rejected");
    return lua_error(state);
}
void instruction_hook(lua_State* state,lua_Debug*) {
    auto& r=run(state);
    r.instructions+=100;
    if(r.instructions>r.limits.instructions) {
        r.exhausted=true;
        lua_pushliteral(state,"Instruction budget exceeded");
        lua_error(state);
    }
}
bool callback(Run& r) {
    if(++r.callbacks<=r.limits.callbacks) return true;
    r.error=CellError{ErrorCode::Limit,"Host callback budget exceeded",{}};
    return false;
}
// Every C++ temporary is destroyed before a potentially long-jumping Lua call.
// Values that Lua pushes from are owned by Run outside the protected call.
int read_cell(lua_State* state) {
    auto& r=run(state);
    bool ok=false;
    try {
        if(callback(r)) {
            if(lua_gettop(state)!=1||lua_type(state,1)!=LUA_TSTRING)
                r.error=CellError{ErrorCode::Value,"cell requires one A1 string",{}};
            else {
                std::size_t length{}; const char* text=lua_tolstring(state,1,&length);
                auto parsed=from_a1(std::string_view(text,length));
                if(auto error=std::get_if<CellError>(&parsed)) r.error=*error;
                else {
                    r.scratch=r.reader(std::get<CellCoord>(parsed));
                    if(auto error=std::get_if<CellError>(&r.scratch)) r.error=*error;
                    else ok=true;
                }
            }
        }
    } catch(...) { r.error=CellError{ErrorCode::Limit,"Host read allocation failure",{}}; }
    if(!ok) return raise(state);
    if(auto p=std::get_if<double>(&r.scratch)) lua_pushnumber(state,*p);
    else if(auto p=std::get_if<bool>(&r.scratch)) lua_pushboolean(state,*p);
    else if(auto p=std::get_if<std::string>(&r.scratch)) lua_pushlstring(state,p->data(),p->size());
    else lua_pushnumber(state,0); // Blank cell convention matches native arithmetic.
    return 1;
}
int write_cell(lua_State* state) {
    auto& r=run(state); bool ok=false;
    try {
        if(callback(r)) {
            if(!r.macro||lua_gettop(state)!=2||lua_type(state,1)!=LUA_TSTRING)
                r.error=CellError{ErrorCode::Value,"set requires an address and scalar value in a macro",{}};
            else {
                std::size_t length{}; const char* address=lua_tolstring(state,1,&length);
                auto parsed=from_a1(std::string_view(address,length));
                if(auto error=std::get_if<CellError>(&parsed)) r.error=*error;
                else {
                    auto coord=std::get<CellCoord>(parsed); Input value; bool scalar=true;
                    switch(lua_type(state,2)) {
                    case LUA_TNIL: break;
                    case LUA_TBOOLEAN: value=bool(lua_toboolean(state,2)); break;
                    case LUA_TNUMBER: {
                        const auto number=lua_tonumber(state,2);
                        if(std::isfinite(number)) value=double(number); else scalar=false;
                        break;
                    }
                    case LUA_TSTRING: {
                        const char* text=lua_tolstring(state,2,&length);
                        if(length>r.limits.output_bytes || r.output_bytes+length>r.limits.output_bytes) scalar=false;
                        else { r.output_bytes+=length; value=std::string(text,length); }
                        break;
                    }
                    default:scalar=false;
                    }
                    if(!scalar) r.error=CellError{ErrorCode::Value,"set supports finite scalars, bounded text, or nil",coord};
                    else if(r.batch.cells.size()>=r.limits.writes) r.error=CellError{ErrorCode::Limit,"Macro write budget exceeded",coord};
                    else {
                        r.rows.insert(coord.row);
                        if(r.rows.size()>r.limits.rows) r.error=CellError{ErrorCode::Limit,"Macro row budget exceeded",coord};
                        else { r.batch.cells.push_back({coord,std::move(value)}); ok=true; }
                    }
                }
            }
        }
    } catch(...) { r.error=CellError{ErrorCode::Limit,"Host write allocation failure",{}}; }
    if(!ok) return raise(state);
    return 0;
}
void copy_global(lua_State* state,int env,const char* name) {
    lua_getglobal(state,name); lua_setfield(state,env,name);
}
void library(lua_State* state,int env,const char* global,const char* const* names) {
    lua_newtable(state);
    lua_getglobal(state,global);
    for(auto p=names;*p;++p) { lua_getfield(state,-1,*p); lua_setfield(state,-3,*p); }
    lua_pop(state,1); lua_setfield(state,env,global);
}
int bootstrap(lua_State* state) {
    luaL_requiref(state,"_G",luaopen_base,1); lua_pop(state,1);
    luaL_requiref(state,LUA_MATHLIBNAME,luaopen_math,1); lua_pop(state,1);
    luaL_requiref(state,LUA_STRLIBNAME,luaopen_string,1); lua_pop(state,1);
    // Remove the string metatable: otherwise ("x").dump/rep would bypass the allowlist.
    lua_pushliteral(state,""); lua_pushnil(state); lua_setmetatable(state,-2); lua_pop(state,1);
    lua_newtable(state); const int env=lua_gettop(state);
    for(const char* name:{"assert","error","type","tonumber","ipairs","select"}) copy_global(state,env,name);
    const char* math[]{"abs","ceil","floor","max","min","sqrt","fmod","sin","cos","tan","log","exp","pi",nullptr};
    const char* strings[]{"len","sub","byte","char","lower","upper",nullptr};
    library(state,env,"math",math); library(state,env,"string",strings);
    lua_pushcfunction(state,read_cell); lua_setfield(state,env,"cell");
    if(run(state).macro) { lua_pushcfunction(state,write_cell); lua_setfield(state,env,"set"); }
    return 1;
}
Value execute(std::string_view source,Run& context) {
    if(source.size()>context.limits.source_bytes) return CellError{ErrorCode::Limit,"Script source byte limit",{}};
    Memory memory{0,context.limits.memory_bytes,false};
    using StateOwner=std::unique_ptr<lua_State,decltype(&lua_close)>;
    StateOwner owner(lua_newstate(allocate,&memory),lua_close);
    if(!owner) return CellError{ErrorCode::Limit,"Unable to create bounded Lua state",{}};
    sol::state_view view(owner.get()); // Borrowed sol2 view; Lua state is uniquely owned.
    auto* state=view.lua_state();
    *static_cast<Run**>(lua_getextraspace(state))=&context;
    lua_pushcfunction(state,bootstrap);
    int status=lua_pcall(state,0,1,0);
    if(status==LUA_OK) {
        status=luaL_loadbufferx(state,source.data(),source.size(),"Julretsu","t");
        if(status==LUA_OK) {
            lua_pushvalue(state,1); lua_setupvalue(state,2,1);
            lua_sethook(state,instruction_hook,LUA_MASKCOUNT,100);
            status=lua_pcall(state,0,1,0);
        }
    }
    if(memory.exhausted||context.exhausted)
        return CellError{ErrorCode::Limit,memory.exhausted?"Lua memory budget exceeded":"Lua instruction budget exceeded",{}};
    if(context.error) return *context.error;
    if(status!=LUA_OK) {
        const char* message=lua_tostring(state,-1);
        return CellError{status==LUA_ERRSYNTAX?ErrorCode::Parse:ErrorCode::Value,
                         message?std::string(message).substr(0,512):"Lua runtime error",{}};
    }
    switch(lua_type(state,-1)) {
    case LUA_TNIL:return {};
    case LUA_TBOOLEAN:return bool(lua_toboolean(state,-1));
    case LUA_TNUMBER: {
        auto value=double(lua_tonumber(state,-1));
        return std::isfinite(value)?Value(value):Value(CellError{ErrorCode::Num,"Lua returned non-finite number",{}});
    }
    case LUA_TSTRING: {
        std::size_t length{}; const char* value=lua_tolstring(state,-1,&length);
        if(length>context.limits.output_bytes) return CellError{ErrorCode::Limit,"Lua output byte limit",{}};
        return std::string(value,length);
    }
    default:return CellError{ErrorCode::Value,"Lua must return a number, Boolean, string, or nil",{}};
    }
}
}
LuaEngine::LuaEngine(LuaLimits limits):limits_(limits) {
    if(limits.memory_bytes<64*1024||limits.memory_bytes>64*1024*1024||
       limits.instructions==0||limits.instructions>2'000'000||limits.source_bytes>65536||
       limits.output_bytes>1'048'576||limits.callbacks>10000||limits.writes>10000||limits.rows>10000)
        throw std::invalid_argument("Lua limits exceed supported bounds");
}
Value LuaEngine::evaluate(std::span<const Value> args,std::size_t budget) const {
    const ExtensionContext context{[](CellCoord c)->Value { return CellError{ErrorCode::Unsupported,"No sheet read context",c}; }};
    return evaluate_with_context(args,budget,context);
}
Value LuaEngine::evaluate_with_context(std::span<const Value> args,std::size_t budget,const ExtensionContext& context) const {
    if(args.size()!=1||!std::holds_alternative<std::string>(args[0]))
        return CellError{ErrorCode::Value,"LUA requires exactly one script string",{}};
    auto limits=limits_; limits.instructions=std::min(limits.instructions,budget);
    Run r{limits,context.read,false,0,0,0,false,{}, {}, {}, {}};
    return execute(std::get<std::string>(args[0]),r);
}
MacroResult LuaEngine::run_macro(std::string_view source,Sheet& sheet) const {
    const CellReader reader=[&](CellCoord c){return sheet.read(c);};
    Run r{limits_,reader,true,0,0,0,false,{}, {}, {}, {}};
    MacroResult result;
    auto value=execute(source,r);
    if(auto error=std::get_if<CellError>(&value)) { result.error=*error; return result; }
    result.writes=r.batch.cells.size(); result.commit=sheet.apply(r.batch);
    if(!result.commit.accepted) result.error=result.commit.error;
    return result;
}
}
