#include "spaghetti.h"
#include <forward_list>

namespace server{
extern int gamemillis;
}

namespace spaghetti{
namespace later{

latertoken::~latertoken(){
    if(lambdaindex != LUA_NOREF) luaL_unref(L, LUA_REGISTRYINDEX, lambdaindex);
}

std::forward_list<latertoken*> abs, game;
ullong longtotalmillis = 0;
latertoken* currentlambda = 0;

void insert(latertoken* t, std::forward_list<latertoken*>& list){
    auto it = list.before_begin(), prev = (it++, list.before_begin());
    for(; it != list.end(); it++, prev++) if(t->when < (*it)->when){
        list.insert_after(prev, t);
        return;
    }
    list.insert_after(prev, t);
}

static latertoken neverhappening{LUA_NOREF};
latertoken* newlater(lua_State* L, bool abs){
    auto fdelay = luaL_checknumber(L, 1);
    luaL_argcheck(L, fdelay > 0, 1, "invalid delay");
    if(std::isinf(fdelay)) return &neverhappening;
    ullong delay = std::ceil(fdelay);
    lua_pushvalue(L, 2);
    int lambdaindex = luaL_ref(L, LUA_REGISTRYINDEX);
    latertoken* l = 0;
    try{
        insert(l = new latertoken{lambdaindex, abs, delay + (abs ? longtotalmillis : server::gamemillis), delay * (ullong)lua_toboolean(L, 3)}, abs ? later::abs : game);
        return l;
    }
    catch(...){
        if(l) delete l; else luaL_unref(L, LUA_REGISTRYINDEX, lambdaindex);
    }
    luaL_error(L, "OOM when adding a new later");
    return 0;
}

void cleargame(){
    for(auto t: game) delete t;
    game.clear();
    if(currentlambda && !currentlambda->abs) currentlambda->delay = 0;
}

void cancel(latertoken& t){
    if(&t == &neverhappening) return;
    if(&t == currentlambda){
        currentlambda->delay = 0;
        return;
    }
    auto& list = t.abs ? abs : game;
    for(auto it = list.before_begin(), prev = (it++, list.before_begin()); it != list.end(); it++, prev++) if(&t == *it){
        list.erase_after(prev);
        delete &t;
        break;
    }
}

void check(ullong now, std::forward_list<latertoken*>& list){
    while(!list.empty() && list.front()->when <= now){
        auto l = currentlambda = list.front();
        list.pop_front();
        lua_cppcall([l,&list]{
            lua_rawgeti(L, LUA_REGISTRYINDEX, l->lambdaindex);
            lua_call(L, 0, 0);
            if(!l->delay){
                delete l;
                return;
            }
            l->when += l->delay;
            insert(l, list);
        }, [l](std::string& err){
            conoutf(CON_ERROR, "One later resulted in an error%s: %s", l->delay ? " and has been cancelled" : "", err.c_str());
            delete l;
        });
        currentlambda = 0;
    }
}

void checkgame(){
    check(server::gamemillis, game);
}

void check(){
    longtotalmillis += elapsedtime;
    check(longtotalmillis, abs);
}

uint maxhang(){
    return min(5ULL, abs.empty() ? 5ULL : (*abs.begin())->when - longtotalmillis);
}

void fini(){
    for(auto& laterlist: {later::game, later::abs}) for(auto l: laterlist) delete l;
    later::game.clear();
    later::abs.clear();
}

}
}
