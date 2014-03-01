#include "spaghetti.h"
#include "cube.h"
#include <csignal>

extern ENetHost* serverhost;

namespace spaghetti{

using namespace luabridge;

lua_State* L;
bool quit = false;

void init(){

    auto quitter = [](int){ quit = true; };
    signal(SIGTERM, quitter);
    signal(SIGHUP,  quitter);
    signal(SIGINT,  quitter);
    signal(SIGUSR1, quitter);
    signal(SIGUSR2, quitter);

    L = luaL_newstate();
    if(!L) fatal("Cannot create lua state");
    luaL_openlibs(L);

    try{
    getGlobalNamespace(L)
            .beginClass<ENetAddress>("ENetAddress")
                    .addData("host", &ENetAddress::host)
                    .addData("port", &ENetAddress::port)
            .endClass()
    ;
    }
    catch(const std::exception& e){ fatal("Error while binding to lua: %s", e.what()); }

    try{ getGlobal(L, "dofile")("script/bootstrap.lua"); }
    catch(const std::exception& e){
        conoutf(CON_ERROR, "Error invoking bootstrap.lua: %s\nIt's unlikely that the server will function properly.", e.what());
    }

}

void fini(){
    kicknonlocalclients();
    enet_host_flush(serverhost);
    lua_close(L);
    L = 0;
}

}
