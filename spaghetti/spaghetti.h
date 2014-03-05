#ifndef SPAGHETTI_H_
#define SPAGHETTI_H_

#include <typeinfo>
#include <functional>
#include <stdexcept>
#include <cassert>
#include <vector>
#include <cxxabi.h>
#include <lua.hpp>
#include "cube.h"

namespace spaghetti{


extern lua_State* L;
extern bool quit;


void init();
void bindengine();
void bindserver();
void fini(bool error);


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


struct extra{
    int ref = LUA_NOREF;
    void init(){
        lua_cppcall([this]{
            lua_newtable(L);
            this->ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }, cppcalldump("Cannot create extra table: %s"));
    }
    void fini(){
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
    }
};


struct packetfilter{

    template<typename Field> packetfilter& operator()(const char* name, Field& field){
        addfield(name, field);
        return *this;
    }

    static bool testinterest(int type);
    static void object();

    /*
     * Utility function to be used with the skippacket(type, fields...) macro.
     * Also it is a template on how to use packetfilter (just ignore the literal parsing stuff).
     */
    template<int type, typename... Fields>
    static bool defaultfilter(const char* literal, Fields&... fields){
        static bool initialized = false;
        static std::vector<std::string> names;
        if(!initialized){
            parsestringliteral(literal, names);
            assert(names.size() == sizeof...(fields));
            initialized = true;
        }
        //XXX cfr callhook
        auto fieldspusher = std::bind([](Fields&... fields){ addfield(names.begin(), fields...); }, fields...);
        bool skip = false;
        lua_cppcall([&]{
            if(!testinterest(type)) return;
            object();
            fieldspusher();
            addfield("skip", skip);
            lua_call(L, 1, 0);
        }, [](std::string& err){ conoutf(CON_ERROR, "Error calling pf[%d]: %s", type, err.c_str()); });
        return skip;
    }

private:

    static void parsestringliteral(const char* literal, std::vector<std::string>& names);

    template<typename T> static void addfield(const char* name, T& field);

    static void addfield(std::vector<std::string>::iterator){}
    template<typename Field, typename... Rest>
    static void addfield(std::vector<std::string>::iterator names, Field& field, Rest&... rest){
        addfield(names->c_str(), field);
        addfield(++names, rest...);
    }

};

#define skippacket(type, ...) packetfilter::defaultfilter<type>(#__VA_ARGS__, ##__VA_ARGS__)

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
