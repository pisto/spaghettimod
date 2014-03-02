#include "cube.h"
#include "spaghetti.h"
#include "commandhijack.h"
#include <csignal>
#include <LuaBridge/LuaBridge.h>
#ifndef WIN32
#include <sys/resource.h>
#endif

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

namespace bridge_instantiations{

#define INSTANTIATE
#include "enetbind.h"
#undef INSTANTIATE

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

    {
        using namespace bridge_instantiations;
        //::, including enet
        auto eng = getGlobalNamespace(L).beginNamespace("engine");
        eng
            #include "enetbind.h"
        ;
    }
    {
        //server::
        using namespace server;
        auto srv = getGlobalNamespace(L).beginNamespace("server");
        srv.addFunction("sendservmsg", &sendservmsg);
    }
    {
        //cubescript
        auto cs = getGlobalNamespace(L).beginNamespace("cs");
        enumeratekt((*idents), const char*, name, ident_bind*, id, id->bind(name, cs));
        cs.endNamespace();
    }
    {
        //spaghetti
        auto cs = getGlobalNamespace(L).beginNamespace("spaghetti");
        cs.addProperty("quit", get<bool>([]{ return quit; }), set<bool>([](bool v){
            if(spaghetti::quit && !v) luaL_error(L, "Cannot abort a quit");
            quit = v;
        }));
        cs.endNamespace();
    }

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
