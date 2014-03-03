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


//debug
struct checktop{
    const int top = lua_gettop(L);
    ~checktop(){
        assert(top == lua_gettop(L));
    }
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
    lua_pushcfunction(L, [](lua_State* L){
        (*(std::function<void()>*)lua_touserdata(L, 1))();
        return 0;
    });
    lua_pushlightuserdata(L, &environment);
    int result = lua_pcall(L, 1, 0, -3);
    lua_remove(L, 1);
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

}

#endif /* SPAGHETTI_H_ */
