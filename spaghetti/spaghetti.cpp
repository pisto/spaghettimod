#include <csignal>
#ifndef _WIN32
#include <sys/resource.h>
#endif

#include "spaghetti.h"
#include "commandhijack.h"

extern ENetHost* serverhost;

namespace server{
    struct clientinfo;
    struct shotevent;
    struct explodeevent;
    struct savedscore;
}

namespace spaghetti{

using namespace luabridge;

lua_State* L;
bool quit = false;
hotstring hotstringref[hotstring::maxhotstring];
static int hook_getterref = LUA_NOREF, hook_setterref = LUA_NOREF;
int stackdumperref = LUA_NOREF, hooksref = LUA_NOREF;

hashtable<const char*, ident_bind*>* idents;

ident_bind::ident_bind(const char* name){
    if(!idents) idents = new hashtable<const char*, ident_bind*>;
    (*idents)[name] = this;
}


bool hook::testinterest(int type){
    lua_rawgeti(L, LUA_REGISTRYINDEX, hooksref);
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

void hook::parsestringliteral(const char* literal, std::vector<int>& names){
    std::string allnames = literal;
    while(true){
        auto namebegin = allnames.find_first_not_of(", ");
        if(namebegin == std::string::npos) break;
        auto nameend = allnames.find_first_of(", ", namebegin);
        auto namelen = (nameend == std::string::npos ? allnames.size() : nameend) - namebegin;
        std::string name = allnames.substr(namebegin, namelen);
        //this is regarded as part of the initialization, so it is not protected
        lua_pushstring(L, name.c_str());
        names.emplace_back(luaL_ref(L, LUA_REGISTRYINDEX));
        allnames = allnames.substr(namelen + namebegin);
    }
}


struct ref{
    virtual int get() = 0;
    virtual void set() = 0;
};

template<typename T> void hook::addfield(int nameref, T& where){
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
    lua_rawgeti(L, LUA_REGISTRYINDEX, nameref);
    new(lua_newuserdata(L, sizeof(luabridgeref))) luabridgeref(where);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}
template<typename T> void hook::addfield(int nameref, T const & where){
    lua_rawgeti(L, LUA_REGISTRYINDEX, nameref);
    luabridge::push(L, where);
    lua_rawset(L, -3);
}
template<> void hook::addfield(int nameref, const char* const& where){
    lua_rawgeti(L, LUA_REGISTRYINDEX, nameref);
    lua_pushstring(L, where);
    lua_rawset(L, -3);
}
template<> void hook::addfield(int nameref, const char*& where){ addfield(nameref, (const char* const&)where); }
#define addfield(T) template void hook::addfield(int, T&); template void hook::addfield(int, T const &)
addfield(bool);
addfield(int);
addfield(uint);
addfield(float);
addfield(lua_string);
addfield(ENetPacket*);
using lua_string_maxtrans = ::lua_array<char, MAXTRANS>;
addfield(lua_string_maxtrans);
addfield(packetbuf*);
addfield(server::clientinfo*);
addfield(server::shotevent*);
addfield(server::explodeevent*);
addfield(server::savedscore*);
addfield(std::string);
#undef addfield
template<typename T> void addfieldptr(int nameref, T& where){
    lua_rawgeti(L, LUA_REGISTRYINDEX, nameref);
    luabridge::push(L, &where);
    lua_rawset(L, -3);
}
#define addfieldptr(T)\
    template<> void hook::addfield(int nameref, T& where){ addfieldptr(nameref, where); }\
    template<> void hook::addfield(int nameref, const T& where){ addfieldptr(nameref, where); }
addfieldptr(ENetAddress);
addfieldptr(ENetEvent);
addfieldptr(packetbuf);
addfieldptr(ucharbuf);
addfieldptr(vec);
addfieldptr(selinfo);
addfieldptr(entity);
addfieldptr(server::clientinfo);
addfieldptr(server::server_entity);
using vector_server_entity = vector<server::server_entity>;
addfieldptr(vector_server_entity);
using vector_sermodeitem = vector<server::servmodeitem>;
addfieldptr(vector_sermodeitem);
addfieldptr(server::savedscore);
#undef addfieldptr

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
hook hook::object(){
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
    return {};
}



void init(){

#ifndef _WIN32
    rlimit limit;
    if(getrlimit(RLIMIT_CORE, &limit)) conoutf(CON_WARN, "failed to get ulimit -c.");
    else{
        limit.rlim_cur=limit.rlim_max;
        if(setrlimit(RLIMIT_CORE, &limit)) conoutf(CON_WARN, "failed to set ulimit -c.");
    }

    auto noop = [](int){};
    signal(SIGHUP,  noop);
    signal(SIGUSR1, noop);
    signal(SIGUSR2, noop);
#endif

    auto quitter = [](int){ quit = true; };
    signal(SIGTERM, quitter);
    signal(SIGINT,  quitter);

    L = luaL_newstate();
    if(!L) fatal("Cannot create lua state");
    luaL_openlibs(L);

#define hot(str) lua_pushstring(L, #str); hotstringref[hotstring::str].literal = #str; hotstringref[hotstring::str]._ref = luaL_ref(L, LUA_REGISTRYINDEX)
    hot(__index); hot(__newindex); hot(__metatable);
    hot(log); hot(shuttingdown); hot(noclients);
    hot(hostcreate); hot(extinfocreate); hot(laninfocreate);
    hot(ping); hot(masterin); hot(enetevent); hot(send);
    hot(clientconnect); hot(connected); hot(clientdisconnect); hot(notalive);
    hot(reinitai); hot(botjoin); hot(botleave);
    hot(master); hot(kick); hot(addban);
    hot(martian_transport); hot(martian_preconnectchan); hot(martian); hot(receivefile);
    hot(sender); hot(chan); hot(p); hot(ci); hot(cq); hot(cm); hot(skip);
    hot(worldstate_pos); hot(worldstate_msg);
    hot(jointimeout); hot(exceeded);
    hot(prechangemap); hot(changemap);
    hot(dodamage); hot(damageeffects); hot(damaged);
    hot(preannounce); hot(announce);
    hot(worldupdate); hot(preitemspawn); hot(itemspawn);
    hot(prepickup); hot(pickup);
    hot(canspawnitem); hot(spawntime); hot(delayspawn); hot(spawnstate);
    hot(presuicide); hot(suicide);
    hot(explode); hot(shot);
    hot(preintermission); hot(intermission);
    hot(autoteam);
    hot(savegamestate); hot(restoregamestate);
    hot(loaditems);
    hot(servmodesetup); hot(servmodeupdate);
    hot(allowbroadcast);
    hot(masterconnected); hot(masterdisconnected);
    hot(setupdemorecord); hot(recordpacket); hot(enddemorecord);
#undef hot
    lua_pushcfunction(L, hook_getter);
    hook_getterref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushcfunction(L, hook_setter);
    hook_setterref = luaL_ref(L, LUA_REGISTRYINDEX);

    bindengine();
    bindcrypto();
    bindserver();
    //cubescript
    enumeratekt((*idents), const char*, name, ident_bind*, id, id->bind(name));
    //spaghetti
    bindArrayProxy<int*>("spaghetti");
    getGlobalNamespace(L).beginNamespace("spaghetti")
        .addProperty("quit", +[]{ return quit; }, +[](bool v){
            if(spaghetti::quit && !v) luaL_error(L, "Cannot abort a quit");
            quit = v;
        })
        .addFunction("_Exit", +[](int status){ std::_Exit(status); })
        .beginClass<later::latertoken>("latertoken")
        .endClass()
        .addFunction("later", +[](lua_State* L){ return later::newlater(L, true); })
        .addFunction("latergame", +[](lua_State* L){ return later::newlater(L, false); })
        .addFunction("cancel", later::cancel)
    .endNamespace();
    lua_getglobal(L, "spaghetti");
    lua_pushstring(L, "hooks");
    lua_newtable(L);
    lua_pushvalue(L, -1);
    hooksref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_rawset(L, -3);
    lua_pushstring(L, "stackdumper");
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "traceback");
    lua_remove(L, -2);
    lua_pushcclosure(L, [](lua_State* L){
        if(lua_gettop(L) == 0) lua_pushstring(L, "<no error message>");
        lua_pushstring(L, " {\n");
        lua_concat(L, 2);
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_insert(L, 1);
        lua_pushinteger(L, 2);
        lua_call(L, 2, 1);
        luaL_gsub(L, lua_tostring(L, -1), "\n", "\n\t");
        lua_remove(L, 1);
        lua_pushstring(L, "\n}");
        lua_concat(L, 2);
        return 1;
    }, 1);
    lua_pushvalue(L, -1);
    stackdumperref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_rawset(L, -3);
    lua_pop(L, 1);

    lua_cppcall([]{
        if(luaL_loadfile(L, "script/bootstrap.lua")){
            conoutf(CON_ERROR, "Cannot open script/bootstrap.lua: %s\nIt's unlikely that the server will function properly.", lua_tostring(L, -1));
            return;
        }
        lua_call(L, 0, 0);
    }, cppcalldump("Error running script/bootstrap.lua: %s\nIt's unlikely that the server will function properly."));
}

void fini(bool servererror){
    simpleconstevent(hotstring::shuttingdown, servererror);
    later::fini();
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
