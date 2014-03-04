#ifndef SPAGHETTI_H_
#define SPAGHETTI_H_

#include <typeinfo>
#include <functional>
#include <stdexcept>
#include <cassert>
#include <cxxabi.h>
#include <lua.hpp>
#include "cube.h"

namespace spaghetti{


extern lua_State* L;
extern bool quit;


void init();
void bindengine();
void bindserver();
void fini();


struct extra{
    int ref = LUA_NOREF;
    void init();
    void fini();
};


int stackdumper(lua_State* L);

template<typename F, typename Err>
void lua_cppcall(const F& f, const Err& err){
    //XXX gcc bug, cannot use auto and decltype
    std::function<void()> environment = [&f](){
        try{ f(); return; }
        catch(const std::exception& e){
            const char* mangled = typeid(e).name();
            char* demangled;
            int status;
            demangled = abi::__cxa_demangle(mangled, 0, 0, &status);
            lua_pushfstring(L, "exception %s: %s", demangled ? demangled : mangled, e.what());
            free(demangled);
        }
        catch(...){
            lua_pushstring(L, "C++ exception (not a std::exception)");
        }
        lua_error(L);
    };
    lua_pushcfunction(L, stackdumper);
    int dumperpos = lua_gettop(L);
    lua_pushcfunction(L, [](lua_State* L){
        (*(std::function<void()>*)lua_touserdata(L, 1))();
        return 0;
    });
    lua_pushlightuserdata(L, &environment);
    int result = lua_pcall(L, 1, 0, dumperpos);
    lua_remove(L, dumperpos);
    if(result == LUA_OK) return;
    std::string e = lua_tostring(L, -1);
    lua_pop(L, 1);
    err(e);
}

template<typename F>
void lua_cppcall(const F& f, bool thrw = true){
    if(thrw) lua_cppcall(f, [](std::string& err){ throw std::runtime_error(err); });
    else lua_cppcall(f, [](std::string&){});
}

inline std::function<void(std::string&)> cppcalldump(const char* fmt){
    return [fmt](std::string& err){ conoutf(CON_ERROR, fmt, err.c_str()); };
}


inline void pushargs(){}
template<typename Arg, typename... Rest>
void pushargs(Arg&& arg, Rest&&... rest){
    luabridge::push(L, std::forward<Arg>(arg));
    pushargs(std::forward<Rest>(rest)...);
}

template<typename... Args>
void callhook(const char* name, Args&&... args){
    //XXX gcc doesn't support variadic captures. Need an extra indirection: http://stackoverflow.com/a/17667880/1073006
    auto pusher = std::bind([](Args&&... args){ pushargs(std::forward<Args>(args)...); }, std::forward<Args>(args)...);
    lua_cppcall([name,&pusher]{
        lua_getglobal(L, "hooks");
        lua_getfield(L, -1, name);
        if(lua_type(L, -1) == LUA_TNIL){
            lua_pop(L, 2);
            return;
        }
        lua_remove(L, -2);
        pusher();
        lua_call(L, sizeof...(Args), 0);
    }, cppcalldump((std::string("Error calling hook ") + name + ": %s").c_str()));
}

}


namespace luabridge{

template<> struct Stack<spaghetti::extra>{
    static void push(lua_State* L, spaghetti::extra e){
        lua_rawgeti(L, LUA_REGISTRYINDEX, e.ref);
    }
    static spaghetti::extra get(lua_State* L, int index){
        return spaghetti::extra();
    }
};

}

#endif /* SPAGHETTI_H_ */
