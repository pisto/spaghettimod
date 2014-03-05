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

hashtable<const char*, ident_bind*>* idents;

ident_bind::ident_bind(const char* name){
    if(!idents) idents = new hashtable<const char*, ident_bind*>;
    (*idents)[name] = this;
}


bool packetfilter::testinterest(int type){
    lua_getglobal(L, "pf");
    lua_pushinteger(L, type);
    lua_gettable(L, -2);
    if(lua_type(L, -1) != LUA_TNIL){
        lua_remove(L, -2);
        return true;
    }
    lua_pop(L, 2);
    return false;
}

void packetfilter::parsestringliteral(const char* literal, std::vector<std::string>& names){
    std::string allnames = literal;
    while(true){
        auto namebegin = allnames.find_first_not_of(", ");
        if(namebegin == std::string::npos) break;
        auto nameend = allnames.find_first_of(", ", namebegin);
        auto namelen = (nameend == std::string::npos ? allnames.size() : nameend) - namebegin;
        names.emplace_back(allnames.substr(namebegin, namelen));
        allnames = allnames.substr(namelen + namebegin);
    }
}


struct ref{
    virtual int get() = 0;
    virtual void set() = 0;
};

template<typename T> void packetfilter::addfield(char const* name, T& where){
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
    lua_pushstring(L, name);
    new(lua_newuserdata(L, sizeof(luabridgeref))) luabridgeref(where);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}
#define addfield(T) template void packetfilter::addfield(const char*, T&)
addfield(bool);
addfield(int);
addfield(float);
addfield(lua_string);
addfield(server::clientinfo*);
#undef addfield

void packetfilter::object(){
    lua_newtable(L);    //object
    lua_newtable(L);    //metatable
    lua_pushcfunction(L, [](lua_State*){
        //getter
        lua_getmetatable(L, 1);
        lua_replace(L, 1);
        lua_rawget(L, 1);
        if(lua_type(L, -1) == LUA_TNIL) return 1;
        return ((ref*)lua_touserdata(L, -1))->get();
    });
    lua_pushcfunction(L, [](lua_State*){
        //setter
        lua_getmetatable(L, 1);
        lua_pushvalue(L, 2);
        lua_rawget(L, -2);
        if(lua_type(L, -1) == LUA_TNIL){
            lua_pop(L, 3);
            lua_rawset(L, 1);
        }
        else{
            lua_pushvalue(L, 3);
            ((ref*)lua_touserdata(L, -2))->set();
        }
        return 0;
    });
    lua_setfield(L, -3, "__newindex");
    lua_setfield(L, -2, "__index");
    lua_pushboolean(L, false);
    lua_setfield(L, -2, "__metatable");
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
            lua_pop(L, 1);
            return;
        }
        lua_call(L, 0, 0);
    }, cppcalldump("Error running script/bootstrap.lua: %s\nIt's unlikely that the server will function properly."));
}

void fini(bool error){
    callhook(hotstring::shuttingdown, error);
    if(!error){
        kicknonlocalclients();
        enet_host_flush(serverhost);
    }
    lua_close(L);
    L = 0;
    DELETEP(idents);
}



int stackdumper(lua_State* L){
    if(!lua_checkstack(L, 2)) return 1;
    luaL_traceback(L, L, NULL, 2);
    const char* trace = luaL_gsub(L, lua_tostring(L, -1), "\n", "\n\t");
    lua_remove(L, -2);
    lua_pushfstring(L, "%s\n{\n\t%s\n}", lua_tostring(L, 1), trace);
    return 1;
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
