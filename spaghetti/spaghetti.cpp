#include <csignal>
#ifndef WIN32
#include <sys/resource.h>
#endif

#include "spaghetti.h"
#include "commandhijack.h"

extern ENetHost* serverhost;

namespace spaghetti{

using namespace luabridge;

lua_State* L;
bool quit = false;
hashtable<const char*, ident_bind*>* idents;

ident_bind::ident_bind(const char* name){
    if(!idents) idents = new hashtable<const char*, ident_bind*>;
    (*idents)[name] = this;
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

void fini(){
    lua_cppcall([]{
        lua_getglobal(L, "hooks");
        lua_getfield(L, -1, "shuttingdown");
        if(lua_type(L, -1) != LUA_TNIL){
            lua_call(L, 0, 0);
            lua_pop(L, 1);
        } else lua_pop(L, 2);
    }, cppcalldump("Error calling hooks.shuttingdown: %s"));
    kicknonlocalclients();
    enet_host_flush(serverhost);
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
