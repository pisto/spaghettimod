#include "spaghetti.h"
#include "cube.h"

namespace spaghetti{

using namespace luabridge;

lua_State* L;

void init(){
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
}

}
