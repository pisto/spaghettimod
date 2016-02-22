#ifndef __CUBE_H__
#define __CUBE_H__

#define _FILE_OFFSET_BITS 64

#ifdef __GNUC__
#define gamma __gamma
#endif

#ifdef WIN32
#define _USE_MATH_DEFINES
#endif
#include <cmath>

#ifdef __GNUC__
#undef gamma
#endif

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstdarg>
#include <climits>
#include <cassert>
#include <ctime>
#include <array>
#include <type_traits>
#include <lua.hpp>

namespace spaghetti
{
    extern lua_State *L;
}

template<typename T, size_t len>
struct lua_array : std::array<T, len>
{
    using up = std::array<T, len>;
    using value_type = typename std::conditional<std::is_scalar<T>::value, T, T&>::type;
    using up::up;
    value_type __arrayindex(int i){
        if(size_t(i)>=len) luaL_error(spaghetti::L, "Index %d is out of array bounds (%d)", i, int(len));
        return static_cast<up&>(*this)[i];
    }
    void __arraynewindex(int i, value_type val){
        if(size_t(i)>=len) luaL_error(spaghetti::L, "Index %d is out of array bounds (%d)", i, int(len));
        static_cast<up&>(*this)[i] = val;
    }
    T& operator[](int i){ return static_cast<up&>(*this)[i]; }
    const T& operator[](int i) const { return static_cast<const up&>(*this)[i]; }
    operator T*(){
        return up::data();
    }
    operator const T*() const{
        return up::data();
    }
    ~lua_array(){}
};

#ifdef WIN32
  #define WIN32_LEAN_AND_MEAN
  #ifdef _WIN32_WINNT
  #undef _WIN32_WINNT
  #endif
  #define _WIN32_WINNT 0x0500
  #include "windows.h"
  #ifndef _WINDOWS
    #define _WINDOWS
  #endif
  #ifndef __GNUC__
    #include <eh.h>
    #include <dbghelp.h>
    #include <intrin.h>
  #endif
#endif

#ifndef STANDALONE
  #ifdef __APPLE__
    #include "SDL2/SDL.h"
    #define GL_GLEXT_LEGACY
    #define __glext_h_
    #include <OpenGL/gl.h>
    #define main SDL_main
  #else
    #include <SDL.h>
    #include <SDL_opengl.h>
  #endif
#endif

#include <enet/enet.h>

namespace luabridge{

template <class T> struct Stack;
template<> struct Stack<ENetPacket*>{
    static void push(lua_State* L, ENetPacket* p);
    static ENetPacket* get(lua_State* L, int index);
};

}

#include <zlib.h>

#include "tools.h"
#include "geom.h"
#include "ents.h"
#include "command.h"

#ifndef STANDALONE
#include "glexts.h"
#include "glemu.h"
#endif

#include "iengine.h"
#include "igame.h"

#endif

