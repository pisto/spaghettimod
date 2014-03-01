#ifndef SPAGHETTI_H_
#define SPAGHETTI_H_

#include <LuaBridge/LuaBridge.h>

namespace spaghetti{

extern lua_State* L;
extern bool quit;

void init();
void fini();

}

#endif /* SPAGHETTI_H_ */
