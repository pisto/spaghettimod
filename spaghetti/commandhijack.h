#ifndef _COMMANDHIJACK_H
#define _COMMANDHIJACK_H

#undef COMMANDN
#undef _VAR
#undef _VARF
#undef _HVAR
#undef _HVARF
#undef _FVAR
#undef _FVARF
#undef _SVAR
#undef _SVARF
#undef ICOMMANDNS

#include <string>
#include <LuaBridge/LuaBridge.h>

namespace spaghetti{

extern lua_State* L;

using namespace luabridge;

struct ident_bind{
    ident_bind(const char* name);
    virtual void bind(const char* name) = 0;
    virtual void erased_setter(const void* val, bool dofunc, bool doclamp = false){}
    virtual ~ident_bind(){}
};

template<typename F, F f>
struct command_bind : ident_bind{
    using ident_bind::ident_bind;
    void bind(const char* name){
        getGlobalNamespace(L).beginNamespace("cs").addFunction(name, f).endNamespace();
    }
};

template<typename T, T& cur, const T& min, const T& max, void(*body)()>
struct settable_bind : ident_bind{
    using ident_bind::ident_bind;
    void bind(const char* name){
        if(min>=max) luabridge::getGlobalNamespace(L).beginNamespace("cs").addProperty(name, get).endNamespace();
        else getGlobalNamespace(L).beginNamespace("cs").addProperty(name, get, set).endNamespace();
    }
    void erased_setter(const void* val, bool dofunc, bool doclamp){
        cur = doclamp ? clamp(*(const T*)val, min, max) : *(const T*)val;
        if(dofunc) body();
    }
private:
    static T get(){ return cur; }
    static void set(T val){
        cur = clamp(val, min, max);
        body();
    }
};

template<std::string& storage, void(*body)()>
struct string_bind: ident_bind{
    using ident_bind::ident_bind;
    void bind(const char* name){
        getGlobalNamespace(L).beginNamespace("cs").addProperty(name, get, set).endNamespace();
    }
    operator const char*(){
        return storage.c_str();
    }
    const char* operator=(const char* val){
        return (storage = val).c_str();
    }
    void erased_setter(const void* val, bool dofunc, bool){
        storage = (const char*)val;
        if(dofunc) body();
    }
private:
    static std::string get(){ return storage; }
    static void set(std::string val){
        storage = std::move(val);
        body();
    }
};

}

#define COMMANDN(name, fun, nargs) static spaghetti::command_bind<decltype(&fun), &fun> name##_bind(#name);

#define _VARSETTABLE(name, global, min, cur, max, body, type)\
    type global = cur;\
    namespace{\
        extern const type global##_min = min, global##_max = max;\
        void global##_body(){ body; }\
        spaghetti::settable_bind<type, global, global##_min, global##_max, &global##_body> global##_bind(#name);\
    }
#define  _VARF(name, global, min, cur, max, body, persist) _VARSETTABLE(name, global, min, cur, max, body, int)
#define _HVARF(name, global, min, cur, max, body, persist) _VARSETTABLE(name, global, min, cur, max, body, int)
#define _FVARF(name, global, min, cur, max, body, persist) _VARSETTABLE(name, global, min, cur, max, body, float)
#define  _VAR(name, global, min, cur, max, persist)  _VARF(name, global, min, cur, max, ,persist)
#define _HVAR(name, global, min, cur, max, persist) _HVARF(name, global, min, cur, max, ,persist)
#define _FVAR(name, global, min, cur, max, persist) _FVARF(name, global, min, cur, max, ,persist)

#define _SVARF(name, global, cur, body, persist)\
    std::string global##_storage = cur;\
    static void global##_body(){ body; }\
    static spaghetti::string_bind<global##_storage, &global##_body> global(#name);
#define _SVAR(name, global, cur, persist) _SVARF(name, global, cur, ,persist)

#define ICOMMANDNS(name, cmdname, nargs, proto, b)\
    static void cmdname##_body proto{ b; }\
    static spaghetti::command_bind<decltype(&cmdname##_body), &cmdname##_body> cmdname##_bind(name);

#endif
