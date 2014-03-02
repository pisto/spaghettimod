CXXFLAGS= -O3 -fomit-frame-pointer -ffast-math
override CXXFLAGS+= -Wall -fsigned-char -std=c++11

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
WINLIB=lib64
WINBIN=../bin64
override CXX+= -m64
override WINDRES+= -F pe-x86-64
else
ifneq (,$(findstring CROSS,$(PLATFORM)))
  CXX=i686-w64-mingw32-g++
  WINDRES=i686-w64-mingw32-windres
endif
WINLIB=lib
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
SERVER_LIBS= -mwindows $(STD_LIBS) -L$(WINBIN) -L$(WINLIB) -llua -lzlib1 -lenet -lws2_32 -lwinmm
else
SERVER_INCLUDES= -DSTANDALONE $(INCLUDES)
SERVER_LIBS= -Lenet -llua -lenet -lz
endif
SERVER_OBJS= \
	shared/crypto-standalone.o \
	shared/stream-standalone.o \
	shared/tools-standalone.o \
	engine/command-standalone.o \
	engine/server-standalone.o \
	engine/worldio-standalone.o \
	fpsgame/entities-standalone.o \
	fpsgame/server-standalone.o \
	spaghetti/spaghetti.o

$(SERVER_OBJS): CXXFLAGS += $(SERVER_INCLUDES)

default: all

all: server

clean:
	-$(RM) $(SERVER_OBJS) sauer_server

%-standalone.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

ifneq (,$(findstring MINGW,$(PLATFORM)))
server: $(SERVER_OBJS)
	$(WINDRES) -I vcpp -i vcpp/mingw.rc -J rc -o vcpp/mingw.res -O coff
	$(CXX) $(CXXFLAGS) -o $(WINBIN)/sauer_server.exe vcpp/mingw.res $(SERVER_OBJS) $(SERVER_LIBS)

else
server:	libenet $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o sauer_server $(SERVER_OBJS) $(SERVER_LIBS)  
	
endif

enet/libenet.a:
	$(MAKE) -C enet CC='$(CC)' AR='$(AR)'
libenet: enet/libenet.a

depend:
	makedepend -a -o-standalone.o -Y -DSTANDALONE $(INCLUDES) $(SERVER_OBJS:-standalone.o=.cpp)

# DO NOT DELETE

shared/crypto-standalone.o: shared/cube.h enet/include/enet/enet.h
shared/crypto-standalone.o: enet/include/enet/unix.h
shared/crypto-standalone.o: enet/include/enet/types.h
shared/crypto-standalone.o: enet/include/enet/protocol.h
shared/crypto-standalone.o: enet/include/enet/list.h
shared/crypto-standalone.o: enet/include/enet/callbacks.h shared/tools.h
shared/crypto-standalone.o: shared/geom.h shared/ents.h shared/command.h
shared/crypto-standalone.o: spaghetti/commandhijack.h
shared/crypto-standalone.o: include/LuaBridge/LuaBridge.h
shared/crypto-standalone.o: include/LuaBridge/detail/LuaHelpers.h
shared/crypto-standalone.o: include/LuaBridge/detail/TypeTraits.h
shared/crypto-standalone.o: include/LuaBridge/detail/TypeList.h
shared/crypto-standalone.o: include/LuaBridge/detail/FuncTraits.h
shared/crypto-standalone.o: include/LuaBridge/detail/Constructor.h
shared/crypto-standalone.o: include/LuaBridge/detail/Stack.h
shared/crypto-standalone.o: include/LuaBridge/detail/ClassInfo.h
shared/crypto-standalone.o: include/LuaBridge/detail/LuaException.h
shared/crypto-standalone.o: include/LuaBridge/detail/LuaRef.h
shared/crypto-standalone.o: include/LuaBridge/detail/Iterator.h
shared/crypto-standalone.o: include/LuaBridge/detail/Userdata.h
shared/crypto-standalone.o: include/LuaBridge/detail/CFunctions.h
shared/crypto-standalone.o: include/LuaBridge/detail/Namespace.h
shared/crypto-standalone.o: shared/iengine.h shared/igame.h
shared/stream-standalone.o: shared/cube.h enet/include/enet/enet.h
shared/stream-standalone.o: enet/include/enet/unix.h
shared/stream-standalone.o: enet/include/enet/types.h
shared/stream-standalone.o: enet/include/enet/protocol.h
shared/stream-standalone.o: enet/include/enet/list.h
shared/stream-standalone.o: enet/include/enet/callbacks.h shared/tools.h
shared/stream-standalone.o: shared/geom.h shared/ents.h shared/command.h
shared/stream-standalone.o: spaghetti/commandhijack.h
shared/stream-standalone.o: include/LuaBridge/LuaBridge.h
shared/stream-standalone.o: include/LuaBridge/detail/LuaHelpers.h
shared/stream-standalone.o: include/LuaBridge/detail/TypeTraits.h
shared/stream-standalone.o: include/LuaBridge/detail/TypeList.h
shared/stream-standalone.o: include/LuaBridge/detail/FuncTraits.h
shared/stream-standalone.o: include/LuaBridge/detail/Constructor.h
shared/stream-standalone.o: include/LuaBridge/detail/Stack.h
shared/stream-standalone.o: include/LuaBridge/detail/ClassInfo.h
shared/stream-standalone.o: include/LuaBridge/detail/LuaException.h
shared/stream-standalone.o: include/LuaBridge/detail/LuaRef.h
shared/stream-standalone.o: include/LuaBridge/detail/Iterator.h
shared/stream-standalone.o: include/LuaBridge/detail/Userdata.h
shared/stream-standalone.o: include/LuaBridge/detail/CFunctions.h
shared/stream-standalone.o: include/LuaBridge/detail/Namespace.h
shared/stream-standalone.o: shared/iengine.h shared/igame.h
shared/tools-standalone.o: shared/cube.h enet/include/enet/enet.h
shared/tools-standalone.o: enet/include/enet/unix.h enet/include/enet/types.h
shared/tools-standalone.o: enet/include/enet/protocol.h
shared/tools-standalone.o: enet/include/enet/list.h
shared/tools-standalone.o: enet/include/enet/callbacks.h shared/tools.h
shared/tools-standalone.o: shared/geom.h shared/ents.h shared/command.h
shared/tools-standalone.o: spaghetti/commandhijack.h
shared/tools-standalone.o: include/LuaBridge/LuaBridge.h
shared/tools-standalone.o: include/LuaBridge/detail/LuaHelpers.h
shared/tools-standalone.o: include/LuaBridge/detail/TypeTraits.h
shared/tools-standalone.o: include/LuaBridge/detail/TypeList.h
shared/tools-standalone.o: include/LuaBridge/detail/FuncTraits.h
shared/tools-standalone.o: include/LuaBridge/detail/Constructor.h
shared/tools-standalone.o: include/LuaBridge/detail/Stack.h
shared/tools-standalone.o: include/LuaBridge/detail/ClassInfo.h
shared/tools-standalone.o: include/LuaBridge/detail/LuaException.h
shared/tools-standalone.o: include/LuaBridge/detail/LuaRef.h
shared/tools-standalone.o: include/LuaBridge/detail/Iterator.h
shared/tools-standalone.o: include/LuaBridge/detail/Userdata.h
shared/tools-standalone.o: include/LuaBridge/detail/CFunctions.h
shared/tools-standalone.o: include/LuaBridge/detail/Namespace.h
shared/tools-standalone.o: shared/iengine.h shared/igame.h
engine/command-standalone.o: engine/engine.h shared/cube.h
engine/command-standalone.o: enet/include/enet/enet.h
engine/command-standalone.o: enet/include/enet/unix.h
engine/command-standalone.o: enet/include/enet/types.h
engine/command-standalone.o: enet/include/enet/protocol.h
engine/command-standalone.o: enet/include/enet/list.h
engine/command-standalone.o: enet/include/enet/callbacks.h shared/tools.h
engine/command-standalone.o: shared/geom.h shared/ents.h shared/command.h
engine/command-standalone.o: spaghetti/commandhijack.h
engine/command-standalone.o: include/LuaBridge/LuaBridge.h
engine/command-standalone.o: include/LuaBridge/detail/LuaHelpers.h
engine/command-standalone.o: include/LuaBridge/detail/TypeTraits.h
engine/command-standalone.o: include/LuaBridge/detail/TypeList.h
engine/command-standalone.o: include/LuaBridge/detail/FuncTraits.h
engine/command-standalone.o: include/LuaBridge/detail/Constructor.h
engine/command-standalone.o: include/LuaBridge/detail/Stack.h
engine/command-standalone.o: include/LuaBridge/detail/ClassInfo.h
engine/command-standalone.o: include/LuaBridge/detail/LuaException.h
engine/command-standalone.o: include/LuaBridge/detail/LuaRef.h
engine/command-standalone.o: include/LuaBridge/detail/Iterator.h
engine/command-standalone.o: include/LuaBridge/detail/Userdata.h
engine/command-standalone.o: include/LuaBridge/detail/CFunctions.h
engine/command-standalone.o: include/LuaBridge/detail/Namespace.h
engine/command-standalone.o: shared/iengine.h shared/igame.h engine/world.h
engine/server-standalone.o: engine/engine.h shared/cube.h
engine/server-standalone.o: enet/include/enet/enet.h enet/include/enet/unix.h
engine/server-standalone.o: enet/include/enet/types.h
engine/server-standalone.o: enet/include/enet/protocol.h
engine/server-standalone.o: enet/include/enet/list.h
engine/server-standalone.o: enet/include/enet/callbacks.h shared/tools.h
engine/server-standalone.o: shared/geom.h shared/ents.h shared/command.h
engine/server-standalone.o: spaghetti/commandhijack.h
engine/server-standalone.o: include/LuaBridge/LuaBridge.h
engine/server-standalone.o: include/LuaBridge/detail/LuaHelpers.h
engine/server-standalone.o: include/LuaBridge/detail/TypeTraits.h
engine/server-standalone.o: include/LuaBridge/detail/TypeList.h
engine/server-standalone.o: include/LuaBridge/detail/FuncTraits.h
engine/server-standalone.o: include/LuaBridge/detail/Constructor.h
engine/server-standalone.o: include/LuaBridge/detail/Stack.h
engine/server-standalone.o: include/LuaBridge/detail/ClassInfo.h
engine/server-standalone.o: include/LuaBridge/detail/LuaException.h
engine/server-standalone.o: include/LuaBridge/detail/LuaRef.h
engine/server-standalone.o: include/LuaBridge/detail/Iterator.h
engine/server-standalone.o: include/LuaBridge/detail/Userdata.h
engine/server-standalone.o: include/LuaBridge/detail/CFunctions.h
engine/server-standalone.o: include/LuaBridge/detail/Namespace.h
engine/server-standalone.o: shared/iengine.h shared/igame.h engine/world.h
engine/server-standalone.o: spaghetti/spaghetti.h
engine/worldio-standalone.o: engine/engine.h shared/cube.h
engine/worldio-standalone.o: enet/include/enet/enet.h
engine/worldio-standalone.o: enet/include/enet/unix.h
engine/worldio-standalone.o: enet/include/enet/types.h
engine/worldio-standalone.o: enet/include/enet/protocol.h
engine/worldio-standalone.o: enet/include/enet/list.h
engine/worldio-standalone.o: enet/include/enet/callbacks.h shared/tools.h
engine/worldio-standalone.o: shared/geom.h shared/ents.h shared/command.h
engine/worldio-standalone.o: spaghetti/commandhijack.h
engine/worldio-standalone.o: include/LuaBridge/LuaBridge.h
engine/worldio-standalone.o: include/LuaBridge/detail/LuaHelpers.h
engine/worldio-standalone.o: include/LuaBridge/detail/TypeTraits.h
engine/worldio-standalone.o: include/LuaBridge/detail/TypeList.h
engine/worldio-standalone.o: include/LuaBridge/detail/FuncTraits.h
engine/worldio-standalone.o: include/LuaBridge/detail/Constructor.h
engine/worldio-standalone.o: include/LuaBridge/detail/Stack.h
engine/worldio-standalone.o: include/LuaBridge/detail/ClassInfo.h
engine/worldio-standalone.o: include/LuaBridge/detail/LuaException.h
engine/worldio-standalone.o: include/LuaBridge/detail/LuaRef.h
engine/worldio-standalone.o: include/LuaBridge/detail/Iterator.h
engine/worldio-standalone.o: include/LuaBridge/detail/Userdata.h
engine/worldio-standalone.o: include/LuaBridge/detail/CFunctions.h
engine/worldio-standalone.o: include/LuaBridge/detail/Namespace.h
engine/worldio-standalone.o: shared/iengine.h shared/igame.h engine/world.h
fpsgame/entities-standalone.o: fpsgame/game.h shared/cube.h
fpsgame/entities-standalone.o: enet/include/enet/enet.h
fpsgame/entities-standalone.o: enet/include/enet/unix.h
fpsgame/entities-standalone.o: enet/include/enet/types.h
fpsgame/entities-standalone.o: enet/include/enet/protocol.h
fpsgame/entities-standalone.o: enet/include/enet/list.h
fpsgame/entities-standalone.o: enet/include/enet/callbacks.h shared/tools.h
fpsgame/entities-standalone.o: shared/geom.h shared/ents.h shared/command.h
fpsgame/entities-standalone.o: spaghetti/commandhijack.h
fpsgame/entities-standalone.o: include/LuaBridge/LuaBridge.h
fpsgame/entities-standalone.o: include/LuaBridge/detail/LuaHelpers.h
fpsgame/entities-standalone.o: include/LuaBridge/detail/TypeTraits.h
fpsgame/entities-standalone.o: include/LuaBridge/detail/TypeList.h
fpsgame/entities-standalone.o: include/LuaBridge/detail/FuncTraits.h
fpsgame/entities-standalone.o: include/LuaBridge/detail/Constructor.h
fpsgame/entities-standalone.o: include/LuaBridge/detail/Stack.h
fpsgame/entities-standalone.o: include/LuaBridge/detail/ClassInfo.h
fpsgame/entities-standalone.o: include/LuaBridge/detail/LuaException.h
fpsgame/entities-standalone.o: include/LuaBridge/detail/LuaRef.h
fpsgame/entities-standalone.o: include/LuaBridge/detail/Iterator.h
fpsgame/entities-standalone.o: include/LuaBridge/detail/Userdata.h
fpsgame/entities-standalone.o: include/LuaBridge/detail/CFunctions.h
fpsgame/entities-standalone.o: include/LuaBridge/detail/Namespace.h
fpsgame/entities-standalone.o: shared/iengine.h shared/igame.h fpsgame/ai.h
fpsgame/server-standalone.o: fpsgame/game.h shared/cube.h
fpsgame/server-standalone.o: enet/include/enet/enet.h
fpsgame/server-standalone.o: enet/include/enet/unix.h
fpsgame/server-standalone.o: enet/include/enet/types.h
fpsgame/server-standalone.o: enet/include/enet/protocol.h
fpsgame/server-standalone.o: enet/include/enet/list.h
fpsgame/server-standalone.o: enet/include/enet/callbacks.h shared/tools.h
fpsgame/server-standalone.o: shared/geom.h shared/ents.h shared/command.h
fpsgame/server-standalone.o: spaghetti/commandhijack.h
fpsgame/server-standalone.o: include/LuaBridge/LuaBridge.h
fpsgame/server-standalone.o: include/LuaBridge/detail/LuaHelpers.h
fpsgame/server-standalone.o: include/LuaBridge/detail/TypeTraits.h
fpsgame/server-standalone.o: include/LuaBridge/detail/TypeList.h
fpsgame/server-standalone.o: include/LuaBridge/detail/FuncTraits.h
fpsgame/server-standalone.o: include/LuaBridge/detail/Constructor.h
fpsgame/server-standalone.o: include/LuaBridge/detail/Stack.h
fpsgame/server-standalone.o: include/LuaBridge/detail/ClassInfo.h
fpsgame/server-standalone.o: include/LuaBridge/detail/LuaException.h
fpsgame/server-standalone.o: include/LuaBridge/detail/LuaRef.h
fpsgame/server-standalone.o: include/LuaBridge/detail/Iterator.h
fpsgame/server-standalone.o: include/LuaBridge/detail/Userdata.h
fpsgame/server-standalone.o: include/LuaBridge/detail/CFunctions.h
fpsgame/server-standalone.o: include/LuaBridge/detail/Namespace.h
fpsgame/server-standalone.o: shared/iengine.h shared/igame.h fpsgame/ai.h
fpsgame/server-standalone.o: spaghetti/spaghetti.h fpsgame/capture.h
fpsgame/server-standalone.o: fpsgame/ctf.h fpsgame/collect.h
fpsgame/server-standalone.o: fpsgame/extinfo.h fpsgame/aiman.h
