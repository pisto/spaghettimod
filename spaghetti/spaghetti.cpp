#include "cube.h"
#include "spaghetti.h"
#include "commandhijack.h"
#include <csignal>
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

    try{
        using namespace bridge_instantiations;
        {
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
    }
    catch(const std::exception& e){ fatal("Error while binding to lua: %s", e.what()); }

    try{ getGlobal(L, "dofile")("script/bootstrap.lua"); }
    catch(const std::exception& e){
        conoutf(CON_ERROR, "Error invoking bootstrap.lua: %s\nIt's unlikely that the server will function properly.", e.what());
    }

}

void fini(){
    try{ getGlobal(L, "beginshutdown")(); } catch(...){}
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
