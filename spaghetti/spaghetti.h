#ifndef SPAGHETTI_H_
#define SPAGHETTI_H_

#include <typeinfo>
#include <functional>
#include <stdexcept>
#include <cassert>
#include <vector>
#include <cxxabi.h>
#include <lua.hpp>
#include "game.h"

namespace spaghetti{


extern lua_State* L;
extern bool quit;


void init();
void bindengine();
void bindcrypto();
void bindserver();
void fini(bool error);

namespace later{

struct latertoken{
    int lambdaindex;
    bool abs;
    ullong when, delay;
    ~latertoken();
};

latertoken* newlater(lua_State* L, bool abs);
void cleargame();
void cancel(latertoken& t);
void checkgame();
void check();
uint maxhang();
void fini();

}


template<typename T>
std::string classname(){
    const char* mangled = typeid(T).name();
    char* demangled;
    int status;
    demangled = abi::__cxa_demangle(mangled, 0, 0, &status);
    std::string ret = demangled ? demangled : mangled;
    free(demangled);
    return ret;
}
template<typename T> std::string classname(T&){ return classname<T>(); }


template<typename F, typename Err>
void lua_cppcall(const F& f, const Err& err){
    extern int stackdumperref;
    lua_rawgeti(L, LUA_REGISTRYINDEX, stackdumperref);
    int dumperpos = lua_gettop(L);
    lua_pushcfunction(L, [](lua_State* L){
        try{ (*(const F*)lua_touserdata(L, 1))(); return 0; }
        catch(const std::exception& e){
            lua_pushfstring(L, "exception %s: %s", classname(e).c_str(), e.what());
        }
        catch(...){
#if defined(LUA_USE_LONGJMP) || defined(LUA_USE_ULONGJMP)
            lua_pushstring(L, "C++ exception (not a std::exception)");
#else
            throw;
#endif
        }
        return lua_error(L);
    });
    lua_pushlightuserdata(L, const_cast<F*>(&f));
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


struct extra{
    extra(bool manualalloc = false): manualalloc(manualalloc){
        if(manualalloc || !L) return;
        init();
    }
    extra(const extra& o): manualalloc(o.manualalloc){
        if(o.ref != LUA_NOREF && L) lua_cppcall([this,&o]{
            lua_rawgeti(L, LUA_REGISTRYINDEX, o.ref);
            this->ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }, cppcalldump("Cannot create extra table: %s"));
    }
    extra& operator=(const extra& o){
        if(!L) return *this;
        fini();
        if(o.ref != LUA_NOREF) lua_cppcall([this,&o]{
            lua_rawgeti(L, LUA_REGISTRYINDEX, o.ref);
            this->ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }, cppcalldump("Cannot create extra table: %s"));
        return *this;
    }
    void init(){
        lua_cppcall([this]{
            lua_newtable(L);
            this->ref = luaL_ref(L, LUA_REGISTRYINDEX);
        }, cppcalldump("Cannot create extra table: %s"));
    }
    ~extra(){
        if(manualalloc || !L) return;
        fini();
    }
    void fini(){
        if(ref == LUA_NOREF) return;
        luaL_unref(L, LUA_REGISTRYINDEX, ref);
        ref = LUA_NOREF;
    }
private:
    friend struct ::luabridge::Stack<extra>;
    friend struct ::luabridge::Stack<extra*>;
    const bool manualalloc;
    int ref = LUA_NOREF;
    extra(int indx): manualalloc(false){
        lua_pushvalue(L, indx);
        ref = luaL_ref(L, LUA_REGISTRYINDEX);
    }
};


/*
 * Avoid creating a new string (malloc, hashing, etc) every time these are used.
 * If you add one here don't forget to initialize it in spaghetti.cpp!
 */
struct hotstring{
    enum{
        __index = NUMMSG, __newindex, __metatable,
        log, shuttingdown, noclients,
        hostcreate, extinfocreate, laninfocreate,
        ping, masterin, enetconnect, enetpacket, enetdisconnect, send,
        clientconnect, connected, clientdisconnect, specstate, pausegame,
        reinitai, botjoin, botleave,
        master, mastermode, trykick, kick, addban,
        tryauth, answerchallenge,
        martian_transport, martian_preconnectchan, martian, receivefile,
        sender, chan, p, ci, cq, cm, skip,
        worldstate_pos, worldstate_msg,
        jointimeout, exceeded,
        prechangemap, changemap,
        dodamage, damageeffects, damaged,
        preannounce, announce,
        worldupdate, preitemspawn, itemspawn,
        canspawnitem, spawntime, delayspawn, spawnstate, spawned,
        prepickup, pickup,
        presuicide, suicide,
        explode, shot,
        preintermission, intermission,
        savegamestate, restoregamestate,
        autoteam, joinspecstate, checkpausegame, checkmastermode,
        loaditems,
        servmodesetup, servmodeupdate, servmodedied,
        allowbroadcast,
        masterconnected, masterdisconnected,
        setupdemorecord, recordpacket, enddemorecord,
        maxhotstring
    };
    static void push(int str){
        extern hotstring hotstringref[hotstring::maxhotstring];
        lua_rawgeti(L, LUA_REGISTRYINDEX, hotstringref[str]._ref);
    }
    static const char* get(int str){
        extern hotstring hotstringref[hotstring::maxhotstring];
        return hotstringref[str].literal;
    }
    static int ref(int str){
        extern hotstring hotstringref[hotstring::maxhotstring];
        return hotstringref[str]._ref;
    }
private:
    friend void init();
    const char* literal = 0;
    int _ref = LUA_NOREF;
};

namespace hook{

    struct fieldproxy_erased{
        void* where;
        bool isconst;
        fieldproxy_erased(void* where, bool isconst = true): where(where), isconst(isconst){}
        virtual void get() = 0;
        virtual void set() = 0;
        template<bool reference, typename T> void setter(){
            if(isconst) luaL_error(L, "hook field %s is const", lua_tostring(L, 2));
            else *static_cast<T*>(where) = luabridge::Stack<typename std::conditional<reference, T&, T>::type>::get(L, 3);
        }
        template<bool reference, typename T> void getter(){
            luabridge::Stack<typename std::conditional<reference, T&, T>::type>::push(L, *static_cast<T*>(where));
        }
    };

#define enableif(cond)		typename std::enable_if<(cond), void>::type
    template<typename T, typename = void> struct fieldproxy;
    template<typename T> struct is_lua_chararray{ static constexpr bool value = false; };
    template<size_t len> struct is_lua_chararray<lua_array<char, len>>{ static constexpr bool value = true; };
    template<typename T> struct fieldproxy<T, enableif(!std::is_arithmetic<T>::value && !is_lua_chararray<T>::value)> : fieldproxy_erased{
        using fieldproxy_erased::fieldproxy_erased;
        void set(){
            setter<!std::is_pointer<T>::value, T>();
        }
        void get(){
            getter<!std::is_pointer<T>::value, T>();
        }
    };
    template<> struct fieldproxy<const char*> : fieldproxy_erased{
        using fieldproxy_erased::fieldproxy_erased;
        void set(){
            luaL_error(L, "hook field %s is a C string", lua_tostring(L, 2));
        }
        void get(){
            getter<false, const char*>();
        }
    };
    template<> struct fieldproxy<std::string> : fieldproxy_erased{
        using fieldproxy_erased::fieldproxy_erased;
        void set(){
            setter<false, std::string>();
        }
        void get(){
            getter<false, std::string>();
        }
    };
    template<size_t len> struct fieldproxy<lua_array<char, len>> : fieldproxy_erased{
        using fieldproxy_erased::fieldproxy_erased;
        void set(){
            setter<false, lua_array<char, len>>();
        }
        void get(){
            getter<false, lua_array<char, len>>();
        }
    };
    template<typename T> struct fieldproxy<T, enableif( std::is_arithmetic<T>::value)> : fieldproxy_erased{
        using fieldproxy_erased::fieldproxy_erased;
        void set(){
            setter<false, T>();
        }
        void get(){
            getter<false, T>();
        }
    };
#undef enableif

    template<typename Field>
    void setfield(int ref, Field& field){
        using Field_mutable = typename std::remove_const<Field>::type;
        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        new(lua_newuserdata(L, sizeof(fieldproxy<Field_mutable>))) fieldproxy<Field_mutable>(const_cast<Field_mutable*>(&field), std::is_const<Field>::value);
        lua_rawset(L, -3);
    }
    inline void setfield(std::vector<int>::iterator){}
    template<typename Field, typename... Rest>
    void setfield(std::vector<int>::iterator names, Field& field, Rest&... rest){
        setfield(*names, field);
        setfield(++names, rest...);
    }

    std::vector<int> parseargs(const char* literal);
    bool testinterest(int type);
    void initinfo();
    void finiinfo();

    template<int type, bool skippable, typename... Fields>
    typename std::conditional<skippable, bool, void>::type defaulthook(const char* literal, Fields&... args){
    //parseargs is called unprotected, but since it's part of the core initialization, failure must be catastrophic.
    static std::vector<int> names = parseargs(literal);
    assert(names.size() == sizeof...(args));
#ifndef __clang__
    //XXX gcc doesn't support variadic captures. Need an extra indirection: http://stackoverflow.com/a/17667880/1073006
    auto setfields = std::bind([](Fields&... args){
        setfield(names.begin(), args...);
    }, std::ref(args)...);
#else
#define setfields() setfield(names.begin(), args...)
#endif
        bool skip = false;
        lua_cppcall([&]{
            if(!testinterest(type)) return;
            initinfo();
            setfields();
            if(skippable) setfield(hotstring::ref(hotstring::skip), skip);
            finiinfo();
            lua_call(L, 1, 0);
        }, [](std::string& err){ conoutf(CON_ERROR, "Error calling hook[%d]: %s", type, err.c_str()); });
        return (typename std::conditional<skippable, bool, void>::type)skip;
    }

#undef setfields

    template<int type, typename... Fields>
    void constevent(const char* literal, Fields const &... fields){
        defaulthook<type, false>(literal, fields...);
    }

};

#define simpleevent(type, ...) hook::defaulthook<type, false>(#__VA_ARGS__, ##__VA_ARGS__)
#define simpleconstevent(type, ...) hook::constevent<type>(#__VA_ARGS__, ##__VA_ARGS__)
#define simplehook(type, ...) hook::defaulthook<type, true>(#__VA_ARGS__, ##__VA_ARGS__)


/*
 * Binary buffer helper. Makes lua able to read/write a buffer with strings, and resize it by setting the data length.
 * Ignore the blurb that comes next. Just use this boilerplate to bind a buffer (e.g., ENetBuffer):
 *
 * #define ebuff lua_buff_type(&ENetBuffer::data, &ENetBuffer::dataLength)
 * .beginClass<ENetBuffer>("ENetBuffer")
 *   .addProperty("dataLength", &ebuff::getLength, &ebuff::setLength)
 *   .addProperty("data", &ebuff::getBuffer, &ebuff::setBuffer)
 * .endClass()
 *
 */
template<typename S, typename B, typename L, B* S::*buffer, L S::*length>
struct lua_buff{
    static size_t getLength(const S* s){
        return size_t(s->*length);
    }
    static std::string getBuffer(const S* s){
        return std::string((const char*)(s->*buffer), getLength(s));
    }
    static void setBuffer(S* s, std::string newbuffer){
        memcpy(s->*buffer, newbuffer.c_str(), min(getLength(s), newbuffer.size()));
    }
};
//deduction
template<typename S, typename T> S mem_class(T S::*);
template<typename S, typename T> T mem_field(T S::*);
#define lua_buff_type(buffer, length)\
	lua_buff<decltype(mem_class(buffer)), typename std::remove_pointer<decltype(mem_field(buffer))>::type, decltype(mem_field(length)), buffer, length>


template<typename T> void bindVectorOf(const char* tablename){
    using V = vector<T>;
    getGlobalNamespace(L).beginNamespace(tablename)
        .beginClass<V>(("vector<" + classname<T>() + ">").c_str())
            .template addConstructor<void(*)()>()
#define add(m, ...) addFunction(#m, __VA_ARGS__ &V::m)
            .add(add, (T&(V::*)(const T&)))
            .add(inrange, (bool(V::*)(int) const))
            .add(pop)
            .add(last)
            .add(drop)
            .add(empty)
            .add(capacity)
            .add(length)
            .add(shrink)
            .add(setsize)
            .add(growbuf)
            .add(advance)
            .add(put, (void(V::*)(const T&)))
            .add(remove, (T(V::*)(int)))
            .add(insert, (T&(V::*)(int, const T&)))
            .addFunction("__arrayindex", &V::__arrayindex)
            .addFunction("__arraynewindex", &V::__arraynewindex)
#undef add
        .endClass()
    .endNamespace();
}


/*
 * Same purpose as lua_array, but avoid changing the definition of the C object, at the expense
 * of extra template and binding fun.
 */
template<typename T> struct lua_arrayproxy;
template<typename T> void bindArrayProxy(const char* table){
    using namespace luabridge;
    using proxy = lua_arrayproxy<T>;
    getGlobalNamespace(L).beginNamespace(table)
        .beginClass<proxy>(classname<proxy>().c_str())
            .addFunction("__arrayindex", &proxy::__arrayindex)
            .addFunction("__arraynewindex", &proxy::__arraynewindex)
        .endClass()
    .endNamespace();
}
template<typename T, size_t len> struct lua_arrayproxy<T[len]>{
    using type = T[len];
    type& where;
    lua_arrayproxy(type& where): where(where){}
    using value_type = typename std::conditional<std::is_scalar<T>::value, T, T&>::type;
    value_type __arrayindex(int i){
        if(size_t(i)>=len) luaL_error(spaghetti::L, "Index %d is out of array bounds (%d)", i, int(len));
        return where[i];
    }
    void __arraynewindex(int i, value_type val){
        if(size_t(i)>=len) luaL_error(spaghetti::L, "Index %d is out of array bounds (%d)", i, int(len));
        where[i] = val;
    }
    template<typename S, type S::*field> static lua_arrayproxy getter(const S* obj){
        return lua_arrayproxy(const_cast<type&>(obj->*field));
    }
};
template<typename T> struct lua_arrayproxy<T*>{
    using type = T*;
    T* where;
    lua_arrayproxy(T* where): where(where){}
    using value_type = typename std::conditional<std::is_scalar<T>::value, T, T&>::type;
    value_type __arrayindex(int i){
        if(i<0) luaL_error(spaghetti::L, "Index %d is negative", i);
        return where[i];
    }
    void __arraynewindex(int i, value_type val){
        if(i<0) luaL_error(spaghetti::L, "Index %d is negative", i);
        where[i] = val;
    }
    template<typename S, T* S::*field> static lua_arrayproxy getter(const S* obj){
        return const_cast<T*>(obj->*field);
    }
};

}

namespace luabridge{

template<> struct Stack<spaghetti::extra>{
    static void push(lua_State* L, const spaghetti::extra& e){
        lua_rawgeti(L, LUA_REGISTRYINDEX, e.ref);
    }
    static spaghetti::extra get(lua_State* L, int index){
        return index;
    }
};
template<> struct Stack<spaghetti::extra*>{
    static void push(lua_State* L, const spaghetti::extra* e){
        lua_rawgeti(L, LUA_REGISTRYINDEX, e->ref);
    }
};
template<> struct Stack<const spaghetti::extra*> : Stack<spaghetti::extra*> {};

/*
 * luabridge::Stack has no extra template argument for SFINAE magic,
 * need to explicitly instantiate it for all enum types, castable to int
 */
#define enumStack(T)\
template<> struct Stack<T>{\
	static void push(lua_State* L, T e){\
		lua_pushinteger(L, int(e));\
	}\
	static T get(lua_State* L, int index){\
	    int isnum;\
	    int val = int(lua_tointegerx(L, index, &isnum));\
	    if(!isnum) luaL_error(L, "Expected integer type");\
		return T(val);\
	}\
}

}

#endif /* SPAGHETTI_H_ */
