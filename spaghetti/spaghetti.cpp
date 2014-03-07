#include <csignal>
#ifndef WIN32
#include <sys/resource.h>
#endif

#include "spaghetti.h"
#include "commandhijack.h"

extern ENetHost* serverhost;

namespace server{
    struct clientinfo;
}

namespace spaghetti{

using namespace luabridge;

lua_State* L;
bool quit = false;
hotstring hotstringref[hotstring::maxhotstring];
static int hook_getterref = LUA_NOREF, hook_setterref = LUA_NOREF;
int stackdumperref = LUA_NOREF;

hashtable<const char*, ident_bind*>* idents;

ident_bind::ident_bind(const char* name){
    if(!idents) idents = new hashtable<const char*, ident_bind*>;
    (*idents)[name] = this;
}


bool hook::testinterest(int type){
    lua_pushglobaltable(L);
    hotstring::push(hotstring::hooks);
    lua_gettable(L, -2);
    if(type < NUMMSG) lua_pushinteger(L, type);
    else hotstring::push(type);
    lua_gettable(L, -2);
    if(lua_type(L, -1) != LUA_TNIL){
        lua_replace(L, -3);
        lua_pop(L, 1);
        return true;
    }
    lua_pop(L, 3);
    return false;
}

void hook::parsestringliteral(const char* literal, std::vector<fieldname>& names){
    std::string allnames = literal;
    while(true){
        auto namebegin = allnames.find_first_not_of(", ");
        if(namebegin == std::string::npos) break;
        auto nameend = allnames.find_first_of(", ", namebegin);
        auto namelen = (nameend == std::string::npos ? allnames.size() : nameend) - namebegin;
        std::string name = allnames.substr(namebegin, namelen);
        //this is regarded as part of the initialization, so it is not protected
        lua_pushstring(L, name.c_str());
        names.emplace_back(std::move(name), luaL_ref(L, LUA_REGISTRYINDEX));
        allnames = allnames.substr(namelen + namebegin);
    }
}


struct ref{
    virtual int get() = 0;
    virtual void set() = 0;
};

template<typename T> void hook::addfield(char const* name, T& where, int nameref){
    struct luabridgeref : ref{
        T& where;
        luabridgeref(T& where): where(where){}
        int get(){
            push(L, where);
            return 1;
        }
        void set(){
            where = Stack<T>::get(L, -1);
        }
    };
    lua_getmetatable(L, -1);
    if(!name) lua_rawgeti(L, LUA_REGISTRYINDEX, nameref);
    else lua_pushstring(L, name);
    new(lua_newuserdata(L, sizeof(luabridgeref))) luabridgeref(where);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}
#define addfield(T) template void packetfilter::addfield(const char*, T&, int)
addfield(bool);
addfield(int);
addfield(float);
addfield(lua_string);
addfield(server::clientinfo*);
#undef addfield

static int hook_getter(lua_State* L){
    lua_getmetatable(L, 1);
    lua_replace(L, 1);
    lua_rawget(L, 1);
    if(lua_type(L, -1) == LUA_TNIL) return 1;
    return ((ref*)lua_touserdata(L, -1))->get();
}
static int hook_setter(lua_State* L){
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    if(lua_type(L, -1) == LUA_TNIL){
        lua_pop(L, 2);
        lua_rawset(L, 1);
    }
    else{
        lua_pushvalue(L, 3);
        ((ref*)lua_touserdata(L, -2))->set();
    }
    return 0;
}
void hook::object(){
    lua_newtable(L);
    lua_newtable(L);
    hotstring::push(hotstring::__index);
    lua_rawgeti(L, LUA_REGISTRYINDEX, hook_getterref);
    lua_rawset(L, -3);
    hotstring::push(hotstring::__newindex);
    lua_rawgeti(L, LUA_REGISTRYINDEX, hook_setterref);
    lua_rawset(L, -3);
    hotstring::push(hotstring::__metatable);
    lua_pushboolean(L, false);
    lua_rawset(L, -3);
    lua_setmetatable(L, -2);
}



template<typename T> using get = T(*)();
template<typename T> using set = void(*)(T);

void init(){

#ifndef WIN32
    rlimit limit;
    if(getrlimit(RLIMIT_CORE, &limit)) conoutf(CON_WARN, "failed to get ulimit -c.");
    else{
        limit.rlim_cur=limit.rlim_max;
        if(setrlimit(RLIMIT_CORE, &limit)) conoutf(CON_WARN, "failed to set ulimit -c.");
    }
#endif

    auto quitter = [](int){ quit = true; };
    auto noop = [](int){};
    signal(SIGTERM, quitter);
    signal(SIGHUP,  noop);
    signal(SIGINT,  quitter);
    signal(SIGUSR1, noop);
    signal(SIGUSR2, noop);

    L = luaL_newstate();
    if(!L) fatal("Cannot create lua state");
    luaL_openlibs(L);

#define addhotstring(str) lua_pushstring(L, #str); hotstringref[hotstring::str].literal = #str; hotstringref[hotstring::str]._ref = luaL_ref(L, LUA_REGISTRYINDEX)
    addhotstring(__index);
    addhotstring(__newindex);
    addhotstring(__metatable);
    addhotstring(hooks);
    addhotstring(skip);
    addhotstring(tick);
    addhotstring(shuttingdown);
#undef addhotstring
    lua_pushcfunction(L, hook_getter);
    hook_getterref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushcfunction(L, hook_setter);
    hook_setterref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushcfunction(L, [](lua_State* L){
        if(!lua_checkstack(L, 2)) return 1;
        luaL_traceback(L, L, NULL, 2);
        const char* trace = luaL_gsub(L, lua_tostring(L, -1), "\n", "\n\t");
        lua_remove(L, -2);
        lua_pushfstring(L, "%s\n{\n\t%s\n}", lua_tostring(L, 1), trace);
        return 1;
    });
    stackdumperref = luaL_ref(L, LUA_REGISTRYINDEX);

    bindengine();
    bindserver();
    //cubescript
    enumeratekt((*idents), const char*, name, ident_bind*, id, id->bind(name));
    //spaghetti
    getGlobalNamespace(L).beginNamespace("spaghetti")
        .addProperty("quit", get<bool>([]{ return quit; }), set<bool>([](bool v){
            if(spaghetti::quit && !v) luaL_error(L, "Cannot abort a quit");
            quit = v;
        }))
    .endNamespace();

    lua_cppcall([]{
        if(luaL_loadfile(L, "script/bootstrap.lua")){
            conoutf(CON_ERROR, "Cannot open script/bootstrap.lua: %s\nIt's unlikely that the server will function properly.", lua_tostring(L, -1));
            return;
        }
        lua_call(L, 0, 0);
    }, cppcalldump("Error running script/bootstrap.lua: %s\nIt's unlikely that the server will function properly."));
}

void fini(const bool servererror){
    simplehook(hotstring::shuttingdown, servererror);
    if(!servererror){
        kicknonlocalclients();
        enet_host_flush(serverhost);
    }
    lua_close(L);
    L = 0;
    DELETEP(idents);
}



}

using namespace spaghetti;

void setvar(const char* name, int i, bool dofunc, bool doclamp){
    (*idents)[name]->erased_setter(&i, dofunc, doclamp);
}

void setfvar(const char* name, float i, bool dofunc, bool doclamp){
    (*idents)[name]->erased_setter(&i, dofunc, doclamp);
}

void setsvar(const char* name, const char* val, bool dofunc){
    (*idents)[name]->erased_setter(val, dofunc);
}

//debug function
#include <sstream>

std::string __debugstack(){
    using spaghetti::L;
    std::stringstream ostr;
    int top = lua_gettop(L);
    for(int i = top; i; i--){
        int t = lua_type(L, i);
        switch(t){
        case LUA_TSTRING: ostr << i << ":\"" << lua_tostring(L, i) << '|'; break;
        case LUA_TBOOLEAN: ostr << i << ":b" << lua_toboolean(L, i) << '|'; break;
        case LUA_TNUMBER: ostr << i << ":" << lua_tonumber(L, i) << '|'; break;
        case LUA_TNIL: ostr << i << ":" << '|'; break;
        default: (ostr << i << ":").write(lua_typename(L, t), 2) << '|'; break;
        }
    }
    return ostr.str();
}
