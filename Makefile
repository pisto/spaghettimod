OPTFLAGS= -O3 -fomit-frame-pointer -ffast-math
override CXXFLAGS+= $(OPTFLAGS) -march=native -ggdb3 -Wall -fsigned-char -std=c++11
override CFLAGS+= $(OPTFLAGS) -march=native -ggdb3 -Wall

PLATFORM= $(shell uname -s | tr '[:lower:]' '[:upper:]')
PLATFORM_PREFIX= native

INCLUDES= -Ishared -Iengine -Ifpsgame -Ispaghetti -Ienet/include -Iinclude

MV=mv

ifneq (,$(findstring MINGW,$(PLATFORM)))
WINDRES= windres
ifneq (,$(findstring 64,$(PLATFORM)))
ifneq (,$(findstring CROSS,$(PLATFORM)))
  CXX=x86_64-w64-mingw32-g++
  WINDRES=x86_64-w64-mingw32-windres
endif
WINBIN=../bin64
override CXX+= -m64
override WINDRES+= -F pe-x86-64
else
ifneq (,$(findstring CROSS,$(PLATFORM)))
  CXX=i686-w64-mingw32-g++
  WINDRES=i686-w64-mingw32-windres
endif
WINBIN=../bin
override CXX+= -m32
override WINDRES+= -F pe-i386
endif
STD_LIBS= -static-libgcc -static-libstdc++
else	
ifneq (,$(findstring DARWIN,$(PLATFORM)))
ifneq (,$(findstring CROSS,$(PLATFORM)))
  TOOLCHAINTARGET= $(shell osxcross-conf | grep -m1 "TARGET=" | cut -b24-)
  TOOLCHAIN= x86_64-apple-$(TOOLCHAINTARGET)-
  AR= $(TOOLCHAIN)ar
  CXX= $(TOOLCHAIN)clang++
  CC= $(TOOLCHAIN)clang
endif
OSXMIN= 10.6
override CC+= -arch x86_64 -mmacosx-version-min=$(OSXMIN)
override CXX+= -arch x86_64 -mmacosx-version-min=$(OSXMIN)
endif
endif

ifneq (,$(findstring MINGW,$(PLATFORM)))
SERVER_INCLUDES= -DSTANDALONE $(INCLUDES)
SERVER_LIBS= -mwindows $(STD_LIBS) -Lenet/.libs -llua -lzlib1 -lenet -lws2_32 -lwinmm
else
LUAVERSION=5.2
SERVER_INCLUDES= -DSTANDALONE $(INCLUDES) `pkg-config --cflags lua$(LUAVERSION)`
SERVER_LIBS= -Lenet/.libs `pkg-config --libs lua$(LUAVERSION))` -lenet -lz -lm
endif
SERVER_OBJS= \
	shared/crypto.o \
	shared/stream.o \
	shared/tools.o \
	engine/command.o \
	engine/server.o \
	engine/worldio.o \
	fpsgame/entities.o \
	fpsgame/server.o \
	spaghetti/spaghetti.o

$(SERVER_OBJS): CXXFLAGS += $(SERVER_INCLUDES)

default: all

all: server

clean:
	-$(RM) $(SERVER_OBJS) sauer_server

ifneq (,$(findstring MINGW,$(PLATFORM)))
server: libenet $(SERVER_OBJS)
	$(WINDRES) -I vcpp -i vcpp/mingw.rc -J rc -o vcpp/mingw.res -O coff
	$(CXX) $(CXXFLAGS) -o $(WINBIN)/sauer_server.exe vcpp/mingw.res $(SERVER_OBJS) $(SERVER_LIBS)

else
server:	libenet $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o sauer_server $(SERVER_OBJS) $(SERVER_LIBS)  
	
endif

enet/.libs/libenet.a:
	$(MAKE) -C enet/ all

enet/Makefile:
	cd enet; ./configure CFLAGS="$(CFLAGS)" --enable-shared=no --enable-static=yes
       
libenet: enet/Makefile enet/.libs/libenet.a

clean-enet:
	-$(MAKE) -C enet/ distclean

depend:
	makedepend -a -Y -DSTANDALONE $(INCLUDES) $(SERVER_OBJS:.o=.cpp)

# DO NOT DELETE

shared/crypto.o: shared/cube.h enet/include/enet/enet.h
shared/crypto.o: enet/include/enet/unix.h enet/include/enet/types.h
shared/crypto.o: enet/include/enet/protocol.h enet/include/enet/list.h
shared/crypto.o: enet/include/enet/callbacks.h shared/tools.h shared/geom.h
shared/crypto.o: shared/ents.h shared/command.h spaghetti/commandhijack.h
shared/crypto.o: include/LuaBridge/LuaBridge.h
shared/crypto.o: include/LuaBridge/detail/LuaHelpers.h
shared/crypto.o: include/LuaBridge/detail/TypeTraits.h
shared/crypto.o: include/LuaBridge/detail/TypeList.h
shared/crypto.o: include/LuaBridge/detail/FuncTraits.h
shared/crypto.o: include/LuaBridge/detail/Constructor.h
shared/crypto.o: include/LuaBridge/detail/Stack.h
shared/crypto.o: include/LuaBridge/detail/ClassInfo.h
shared/crypto.o: include/LuaBridge/detail/Userdata.h
shared/crypto.o: include/LuaBridge/detail/CFunctions.h
shared/crypto.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
shared/crypto.o: shared/igame.h spaghetti/spaghetti.h fpsgame/game.h
shared/crypto.o: fpsgame/ai.h
shared/stream.o: shared/cube.h enet/include/enet/enet.h
shared/stream.o: enet/include/enet/unix.h enet/include/enet/types.h
shared/stream.o: enet/include/enet/protocol.h enet/include/enet/list.h
shared/stream.o: enet/include/enet/callbacks.h shared/tools.h shared/geom.h
shared/stream.o: shared/ents.h shared/command.h spaghetti/commandhijack.h
shared/stream.o: include/LuaBridge/LuaBridge.h
shared/stream.o: include/LuaBridge/detail/LuaHelpers.h
shared/stream.o: include/LuaBridge/detail/TypeTraits.h
shared/stream.o: include/LuaBridge/detail/TypeList.h
shared/stream.o: include/LuaBridge/detail/FuncTraits.h
shared/stream.o: include/LuaBridge/detail/Constructor.h
shared/stream.o: include/LuaBridge/detail/Stack.h
shared/stream.o: include/LuaBridge/detail/ClassInfo.h
shared/stream.o: include/LuaBridge/detail/Userdata.h
shared/stream.o: include/LuaBridge/detail/CFunctions.h
shared/stream.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
shared/stream.o: shared/igame.h
shared/tools.o: shared/cube.h enet/include/enet/enet.h
shared/tools.o: enet/include/enet/unix.h enet/include/enet/types.h
shared/tools.o: enet/include/enet/protocol.h enet/include/enet/list.h
shared/tools.o: enet/include/enet/callbacks.h shared/tools.h shared/geom.h
shared/tools.o: shared/ents.h shared/command.h spaghetti/commandhijack.h
shared/tools.o: include/LuaBridge/LuaBridge.h
shared/tools.o: include/LuaBridge/detail/LuaHelpers.h
shared/tools.o: include/LuaBridge/detail/TypeTraits.h
shared/tools.o: include/LuaBridge/detail/TypeList.h
shared/tools.o: include/LuaBridge/detail/FuncTraits.h
shared/tools.o: include/LuaBridge/detail/Constructor.h
shared/tools.o: include/LuaBridge/detail/Stack.h
shared/tools.o: include/LuaBridge/detail/ClassInfo.h
shared/tools.o: include/LuaBridge/detail/Userdata.h
shared/tools.o: include/LuaBridge/detail/CFunctions.h
shared/tools.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
shared/tools.o: shared/igame.h
engine/command.o: engine/engine.h shared/cube.h enet/include/enet/enet.h
engine/command.o: enet/include/enet/unix.h enet/include/enet/types.h
engine/command.o: enet/include/enet/protocol.h enet/include/enet/list.h
engine/command.o: enet/include/enet/callbacks.h shared/tools.h shared/geom.h
engine/command.o: shared/ents.h shared/command.h spaghetti/commandhijack.h
engine/command.o: include/LuaBridge/LuaBridge.h
engine/command.o: include/LuaBridge/detail/LuaHelpers.h
engine/command.o: include/LuaBridge/detail/TypeTraits.h
engine/command.o: include/LuaBridge/detail/TypeList.h
engine/command.o: include/LuaBridge/detail/FuncTraits.h
engine/command.o: include/LuaBridge/detail/Constructor.h
engine/command.o: include/LuaBridge/detail/Stack.h
engine/command.o: include/LuaBridge/detail/ClassInfo.h
engine/command.o: include/LuaBridge/detail/Userdata.h
engine/command.o: include/LuaBridge/detail/CFunctions.h
engine/command.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
engine/command.o: shared/igame.h engine/world.h
engine/server.o: engine/engine.h shared/cube.h enet/include/enet/enet.h
engine/server.o: enet/include/enet/unix.h enet/include/enet/types.h
engine/server.o: enet/include/enet/protocol.h enet/include/enet/list.h
engine/server.o: enet/include/enet/callbacks.h shared/tools.h shared/geom.h
engine/server.o: shared/ents.h shared/command.h spaghetti/commandhijack.h
engine/server.o: include/LuaBridge/LuaBridge.h
engine/server.o: include/LuaBridge/detail/LuaHelpers.h
engine/server.o: include/LuaBridge/detail/TypeTraits.h
engine/server.o: include/LuaBridge/detail/TypeList.h
engine/server.o: include/LuaBridge/detail/FuncTraits.h
engine/server.o: include/LuaBridge/detail/Constructor.h
engine/server.o: include/LuaBridge/detail/Stack.h
engine/server.o: include/LuaBridge/detail/ClassInfo.h
engine/server.o: include/LuaBridge/detail/Userdata.h
engine/server.o: include/LuaBridge/detail/CFunctions.h
engine/server.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
engine/server.o: shared/igame.h engine/world.h spaghetti/spaghetti.h
engine/server.o: fpsgame/game.h fpsgame/ai.h
engine/worldio.o: engine/engine.h shared/cube.h enet/include/enet/enet.h
engine/worldio.o: enet/include/enet/unix.h enet/include/enet/types.h
engine/worldio.o: enet/include/enet/protocol.h enet/include/enet/list.h
engine/worldio.o: enet/include/enet/callbacks.h shared/tools.h shared/geom.h
engine/worldio.o: shared/ents.h shared/command.h spaghetti/commandhijack.h
engine/worldio.o: include/LuaBridge/LuaBridge.h
engine/worldio.o: include/LuaBridge/detail/LuaHelpers.h
engine/worldio.o: include/LuaBridge/detail/TypeTraits.h
engine/worldio.o: include/LuaBridge/detail/TypeList.h
engine/worldio.o: include/LuaBridge/detail/FuncTraits.h
engine/worldio.o: include/LuaBridge/detail/Constructor.h
engine/worldio.o: include/LuaBridge/detail/Stack.h
engine/worldio.o: include/LuaBridge/detail/ClassInfo.h
engine/worldio.o: include/LuaBridge/detail/Userdata.h
engine/worldio.o: include/LuaBridge/detail/CFunctions.h
engine/worldio.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
engine/worldio.o: shared/igame.h engine/world.h
fpsgame/entities.o: fpsgame/game.h shared/cube.h enet/include/enet/enet.h
fpsgame/entities.o: enet/include/enet/unix.h enet/include/enet/types.h
fpsgame/entities.o: enet/include/enet/protocol.h enet/include/enet/list.h
fpsgame/entities.o: enet/include/enet/callbacks.h shared/tools.h
fpsgame/entities.o: shared/geom.h shared/ents.h shared/command.h
fpsgame/entities.o: spaghetti/commandhijack.h include/LuaBridge/LuaBridge.h
fpsgame/entities.o: include/LuaBridge/detail/LuaHelpers.h
fpsgame/entities.o: include/LuaBridge/detail/TypeTraits.h
fpsgame/entities.o: include/LuaBridge/detail/TypeList.h
fpsgame/entities.o: include/LuaBridge/detail/FuncTraits.h
fpsgame/entities.o: include/LuaBridge/detail/Constructor.h
fpsgame/entities.o: include/LuaBridge/detail/Stack.h
fpsgame/entities.o: include/LuaBridge/detail/ClassInfo.h
fpsgame/entities.o: include/LuaBridge/detail/Userdata.h
fpsgame/entities.o: include/LuaBridge/detail/CFunctions.h
fpsgame/entities.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
fpsgame/entities.o: shared/igame.h fpsgame/ai.h
fpsgame/server.o: fpsgame/game.h shared/cube.h enet/include/enet/enet.h
fpsgame/server.o: enet/include/enet/unix.h enet/include/enet/types.h
fpsgame/server.o: enet/include/enet/protocol.h enet/include/enet/list.h
fpsgame/server.o: enet/include/enet/callbacks.h shared/tools.h shared/geom.h
fpsgame/server.o: shared/ents.h shared/command.h spaghetti/commandhijack.h
fpsgame/server.o: include/LuaBridge/LuaBridge.h
fpsgame/server.o: include/LuaBridge/detail/LuaHelpers.h
fpsgame/server.o: include/LuaBridge/detail/TypeTraits.h
fpsgame/server.o: include/LuaBridge/detail/TypeList.h
fpsgame/server.o: include/LuaBridge/detail/FuncTraits.h
fpsgame/server.o: include/LuaBridge/detail/Constructor.h
fpsgame/server.o: include/LuaBridge/detail/Stack.h
fpsgame/server.o: include/LuaBridge/detail/ClassInfo.h
fpsgame/server.o: include/LuaBridge/detail/Userdata.h
fpsgame/server.o: include/LuaBridge/detail/CFunctions.h
fpsgame/server.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
fpsgame/server.o: shared/igame.h fpsgame/ai.h spaghetti/spaghetti.h
fpsgame/server.o: fpsgame/capture.h fpsgame/ctf.h fpsgame/collect.h
fpsgame/server.o: fpsgame/extinfo.h fpsgame/aiman.h
spaghetti/spaghetti.o: spaghetti/spaghetti.h fpsgame/game.h shared/cube.h
spaghetti/spaghetti.o: enet/include/enet/enet.h enet/include/enet/unix.h
spaghetti/spaghetti.o: enet/include/enet/types.h enet/include/enet/protocol.h
spaghetti/spaghetti.o: enet/include/enet/list.h enet/include/enet/callbacks.h
spaghetti/spaghetti.o: shared/tools.h shared/geom.h shared/ents.h
spaghetti/spaghetti.o: shared/command.h spaghetti/commandhijack.h
spaghetti/spaghetti.o: include/LuaBridge/LuaBridge.h
spaghetti/spaghetti.o: include/LuaBridge/detail/LuaHelpers.h
spaghetti/spaghetti.o: include/LuaBridge/detail/TypeTraits.h
spaghetti/spaghetti.o: include/LuaBridge/detail/TypeList.h
spaghetti/spaghetti.o: include/LuaBridge/detail/FuncTraits.h
spaghetti/spaghetti.o: include/LuaBridge/detail/Constructor.h
spaghetti/spaghetti.o: include/LuaBridge/detail/Stack.h
spaghetti/spaghetti.o: include/LuaBridge/detail/ClassInfo.h
spaghetti/spaghetti.o: include/LuaBridge/detail/Userdata.h
spaghetti/spaghetti.o: include/LuaBridge/detail/CFunctions.h
spaghetti/spaghetti.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
spaghetti/spaghetti.o: shared/igame.h fpsgame/ai.h

shared/crypto.o: shared/cube.h enet/include/enet/enet.h
shared/crypto.o: enet/include/enet/unix.h enet/include/enet/types.h
shared/crypto.o: enet/include/enet/protocol.h enet/include/enet/list.h
shared/crypto.o: enet/include/enet/callbacks.h shared/tools.h shared/geom.h
shared/crypto.o: shared/ents.h shared/command.h spaghetti/commandhijack.h
shared/crypto.o: include/LuaBridge/LuaBridge.h
shared/crypto.o: include/LuaBridge/detail/LuaHelpers.h
shared/crypto.o: include/LuaBridge/detail/TypeTraits.h
shared/crypto.o: include/LuaBridge/detail/TypeList.h
shared/crypto.o: include/LuaBridge/detail/FuncTraits.h
shared/crypto.o: include/LuaBridge/detail/Constructor.h
shared/crypto.o: include/LuaBridge/detail/Stack.h
shared/crypto.o: include/LuaBridge/detail/ClassInfo.h
shared/crypto.o: include/LuaBridge/detail/Userdata.h
shared/crypto.o: include/LuaBridge/detail/CFunctions.h
shared/crypto.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
shared/crypto.o: shared/igame.h spaghetti/spaghetti.h fpsgame/game.h
shared/crypto.o: fpsgame/ai.h
shared/stream.o: shared/cube.h enet/include/enet/enet.h
shared/stream.o: enet/include/enet/unix.h enet/include/enet/types.h
shared/stream.o: enet/include/enet/protocol.h enet/include/enet/list.h
shared/stream.o: enet/include/enet/callbacks.h shared/tools.h shared/geom.h
shared/stream.o: shared/ents.h shared/command.h spaghetti/commandhijack.h
shared/stream.o: include/LuaBridge/LuaBridge.h
shared/stream.o: include/LuaBridge/detail/LuaHelpers.h
shared/stream.o: include/LuaBridge/detail/TypeTraits.h
shared/stream.o: include/LuaBridge/detail/TypeList.h
shared/stream.o: include/LuaBridge/detail/FuncTraits.h
shared/stream.o: include/LuaBridge/detail/Constructor.h
shared/stream.o: include/LuaBridge/detail/Stack.h
shared/stream.o: include/LuaBridge/detail/ClassInfo.h
shared/stream.o: include/LuaBridge/detail/Userdata.h
shared/stream.o: include/LuaBridge/detail/CFunctions.h
shared/stream.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
shared/stream.o: shared/igame.h
shared/tools.o: shared/cube.h enet/include/enet/enet.h
shared/tools.o: enet/include/enet/unix.h enet/include/enet/types.h
shared/tools.o: enet/include/enet/protocol.h enet/include/enet/list.h
shared/tools.o: enet/include/enet/callbacks.h shared/tools.h shared/geom.h
shared/tools.o: shared/ents.h shared/command.h spaghetti/commandhijack.h
shared/tools.o: include/LuaBridge/LuaBridge.h
shared/tools.o: include/LuaBridge/detail/LuaHelpers.h
shared/tools.o: include/LuaBridge/detail/TypeTraits.h
shared/tools.o: include/LuaBridge/detail/TypeList.h
shared/tools.o: include/LuaBridge/detail/FuncTraits.h
shared/tools.o: include/LuaBridge/detail/Constructor.h
shared/tools.o: include/LuaBridge/detail/Stack.h
shared/tools.o: include/LuaBridge/detail/ClassInfo.h
shared/tools.o: include/LuaBridge/detail/Userdata.h
shared/tools.o: include/LuaBridge/detail/CFunctions.h
shared/tools.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
shared/tools.o: shared/igame.h
engine/command.o: engine/engine.h shared/cube.h enet/include/enet/enet.h
engine/command.o: enet/include/enet/unix.h enet/include/enet/types.h
engine/command.o: enet/include/enet/protocol.h enet/include/enet/list.h
engine/command.o: enet/include/enet/callbacks.h shared/tools.h shared/geom.h
engine/command.o: shared/ents.h shared/command.h spaghetti/commandhijack.h
engine/command.o: include/LuaBridge/LuaBridge.h
engine/command.o: include/LuaBridge/detail/LuaHelpers.h
engine/command.o: include/LuaBridge/detail/TypeTraits.h
engine/command.o: include/LuaBridge/detail/TypeList.h
engine/command.o: include/LuaBridge/detail/FuncTraits.h
engine/command.o: include/LuaBridge/detail/Constructor.h
engine/command.o: include/LuaBridge/detail/Stack.h
engine/command.o: include/LuaBridge/detail/ClassInfo.h
engine/command.o: include/LuaBridge/detail/Userdata.h
engine/command.o: include/LuaBridge/detail/CFunctions.h
engine/command.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
engine/command.o: shared/igame.h engine/world.h
engine/server.o: engine/engine.h shared/cube.h enet/include/enet/enet.h
engine/server.o: enet/include/enet/unix.h enet/include/enet/types.h
engine/server.o: enet/include/enet/protocol.h enet/include/enet/list.h
engine/server.o: enet/include/enet/callbacks.h shared/tools.h shared/geom.h
engine/server.o: shared/ents.h shared/command.h spaghetti/commandhijack.h
engine/server.o: include/LuaBridge/LuaBridge.h
engine/server.o: include/LuaBridge/detail/LuaHelpers.h
engine/server.o: include/LuaBridge/detail/TypeTraits.h
engine/server.o: include/LuaBridge/detail/TypeList.h
engine/server.o: include/LuaBridge/detail/FuncTraits.h
engine/server.o: include/LuaBridge/detail/Constructor.h
engine/server.o: include/LuaBridge/detail/Stack.h
engine/server.o: include/LuaBridge/detail/ClassInfo.h
engine/server.o: include/LuaBridge/detail/Userdata.h
engine/server.o: include/LuaBridge/detail/CFunctions.h
engine/server.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
engine/server.o: shared/igame.h engine/world.h spaghetti/spaghetti.h
engine/server.o: fpsgame/game.h fpsgame/ai.h
engine/worldio.o: engine/engine.h shared/cube.h enet/include/enet/enet.h
engine/worldio.o: enet/include/enet/unix.h enet/include/enet/types.h
engine/worldio.o: enet/include/enet/protocol.h enet/include/enet/list.h
engine/worldio.o: enet/include/enet/callbacks.h shared/tools.h shared/geom.h
engine/worldio.o: shared/ents.h shared/command.h spaghetti/commandhijack.h
engine/worldio.o: include/LuaBridge/LuaBridge.h
engine/worldio.o: include/LuaBridge/detail/LuaHelpers.h
engine/worldio.o: include/LuaBridge/detail/TypeTraits.h
engine/worldio.o: include/LuaBridge/detail/TypeList.h
engine/worldio.o: include/LuaBridge/detail/FuncTraits.h
engine/worldio.o: include/LuaBridge/detail/Constructor.h
engine/worldio.o: include/LuaBridge/detail/Stack.h
engine/worldio.o: include/LuaBridge/detail/ClassInfo.h
engine/worldio.o: include/LuaBridge/detail/Userdata.h
engine/worldio.o: include/LuaBridge/detail/CFunctions.h
engine/worldio.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
engine/worldio.o: shared/igame.h engine/world.h
fpsgame/entities.o: fpsgame/game.h shared/cube.h enet/include/enet/enet.h
fpsgame/entities.o: enet/include/enet/unix.h enet/include/enet/types.h
fpsgame/entities.o: enet/include/enet/protocol.h enet/include/enet/list.h
fpsgame/entities.o: enet/include/enet/callbacks.h shared/tools.h
fpsgame/entities.o: shared/geom.h shared/ents.h shared/command.h
fpsgame/entities.o: spaghetti/commandhijack.h include/LuaBridge/LuaBridge.h
fpsgame/entities.o: include/LuaBridge/detail/LuaHelpers.h
fpsgame/entities.o: include/LuaBridge/detail/TypeTraits.h
fpsgame/entities.o: include/LuaBridge/detail/TypeList.h
fpsgame/entities.o: include/LuaBridge/detail/FuncTraits.h
fpsgame/entities.o: include/LuaBridge/detail/Constructor.h
fpsgame/entities.o: include/LuaBridge/detail/Stack.h
fpsgame/entities.o: include/LuaBridge/detail/ClassInfo.h
fpsgame/entities.o: include/LuaBridge/detail/Userdata.h
fpsgame/entities.o: include/LuaBridge/detail/CFunctions.h
fpsgame/entities.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
fpsgame/entities.o: shared/igame.h fpsgame/ai.h
fpsgame/server.o: fpsgame/game.h shared/cube.h enet/include/enet/enet.h
fpsgame/server.o: enet/include/enet/unix.h enet/include/enet/types.h
fpsgame/server.o: enet/include/enet/protocol.h enet/include/enet/list.h
fpsgame/server.o: enet/include/enet/callbacks.h shared/tools.h shared/geom.h
fpsgame/server.o: shared/ents.h shared/command.h spaghetti/commandhijack.h
fpsgame/server.o: include/LuaBridge/LuaBridge.h
fpsgame/server.o: include/LuaBridge/detail/LuaHelpers.h
fpsgame/server.o: include/LuaBridge/detail/TypeTraits.h
fpsgame/server.o: include/LuaBridge/detail/TypeList.h
fpsgame/server.o: include/LuaBridge/detail/FuncTraits.h
fpsgame/server.o: include/LuaBridge/detail/Constructor.h
fpsgame/server.o: include/LuaBridge/detail/Stack.h
fpsgame/server.o: include/LuaBridge/detail/ClassInfo.h
fpsgame/server.o: include/LuaBridge/detail/Userdata.h
fpsgame/server.o: include/LuaBridge/detail/CFunctions.h
fpsgame/server.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
fpsgame/server.o: shared/igame.h fpsgame/ai.h spaghetti/spaghetti.h
fpsgame/server.o: fpsgame/capture.h fpsgame/ctf.h fpsgame/collect.h
fpsgame/server.o: fpsgame/extinfo.h fpsgame/aiman.h
spaghetti/spaghetti.o: spaghetti/spaghetti.h fpsgame/game.h shared/cube.h
spaghetti/spaghetti.o: enet/include/enet/enet.h enet/include/enet/unix.h
spaghetti/spaghetti.o: enet/include/enet/types.h enet/include/enet/protocol.h
spaghetti/spaghetti.o: enet/include/enet/list.h enet/include/enet/callbacks.h
spaghetti/spaghetti.o: shared/tools.h shared/geom.h shared/ents.h
spaghetti/spaghetti.o: shared/command.h spaghetti/commandhijack.h
spaghetti/spaghetti.o: include/LuaBridge/LuaBridge.h
spaghetti/spaghetti.o: include/LuaBridge/detail/LuaHelpers.h
spaghetti/spaghetti.o: include/LuaBridge/detail/TypeTraits.h
spaghetti/spaghetti.o: include/LuaBridge/detail/TypeList.h
spaghetti/spaghetti.o: include/LuaBridge/detail/FuncTraits.h
spaghetti/spaghetti.o: include/LuaBridge/detail/Constructor.h
spaghetti/spaghetti.o: include/LuaBridge/detail/Stack.h
spaghetti/spaghetti.o: include/LuaBridge/detail/ClassInfo.h
spaghetti/spaghetti.o: include/LuaBridge/detail/Userdata.h
spaghetti/spaghetti.o: include/LuaBridge/detail/CFunctions.h
spaghetti/spaghetti.o: include/LuaBridge/detail/Namespace.h shared/iengine.h
spaghetti/spaghetti.o: shared/igame.h fpsgame/ai.h
