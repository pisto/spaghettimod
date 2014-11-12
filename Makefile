PLATFORM:= $(shell ./enet/config.guess)
SHELL:= /bin/bash

CPUINFO:= -march=native
OPTFLAGS:= -O3 -fomit-frame-pointer -ffast-math -fno-finite-math-only
DEBUG:= -ggdb3
CC:= gcc
CXX:= g++

ifneq (, $(findstring 64,$(PLATFORM)))
override CPUINFO+= -m64 
else
override CPUINFO+= -m32 
endif

ifneq (, $(findstring mingw,$(PLATFORM)))
WIN:= 1
override OPTFLAGS:= -fno-tree-vectorize
else ifneq (,$(findstring darwin,$(PLATFORM)))
MAC:= 1
CC:= clang
CXX:= clang++
OSXMIN:= 10.7
override CPUINFO+= -arch x86_64 -mmacosx-version-min=$(OSXMIN)
override CXXFLAGS+= -stdlib=libc++
else
LINUX:= 1
endif

PKGCONFIG:= $(shell which $(PLATFORM)-pkg-config 2>/dev/null || echo pkg-config)
OBJCOPY:= $(shell which $(PLATFORM)-objcopy 2>/dev/null || echo objcopy)
STRIP:= $(shell which $(PLATFORM)-strip 2>/dev/null || echo strip)
CC:= $(shell which $(PLATFORM)-$(CC) 2>/dev/null || echo $(CC))
CXX:= $(shell which $(PLATFORM)-$(CXX) 2>/dev/null || echo $(CXX))
ifdef WIN
WINDRES:= $(shell which $(PLATFORM)-windres 2>/dev/null || echo windres)
endif

ifndef LUACFLAGS
ifndef LUALDFLAGS
ifndef LUAVERSION
LUAVERSION:= $(shell for i in {jit,5.2,52,"",5.1,51}; do $(PKGCONFIG) --exists lua$$i && echo $$i && exit; done; echo error)
ifeq (error,$(LUAVERSION))
$(error Cannot determine LUAVERSION trying {jit,5.2,52,"",5.1,51}, please provide on command line)
endif
$(info Selected LUAVERSION=$(LUAVERSION))
LUACFLAGS:=$(shell $(PKGCONFIG) --cflags lua$(LUAVERSION))
LUALDFLAGS:=$(shell $(PKGCONFIG) --libs lua$(LUAVERSION))
endif
endif
endif


ifdef WIN
override LDFLAGS+= -mwindows -static-libgcc -static-libstdc++
override LIBS+= -lz -lenet -lws2_32 -lwinmm
else
override LIBS+= -lenet -lz -lm
endif

override CFLAGS+= $(OPTFLAGS) $(CPUINFO) $(DEBUG) -Wall
override CXXFLAGS+= $(OPTFLAGS) $(CPUINFO) $(DEBUG) -Wall -fsigned-char -std=c++11 -Ishared -Iengine -Ifpsgame -Ispaghetti -Ienet/include -Iinclude $(LUACFLAGS) -DSTANDALONE
override LDFLAGS+= -Lenet/.libs $(LUALDFLAGS)

export CC
export CXX
export CPPFLAGS:= $(CFLAGS)
export CFLAGS


SERVER_OBJS= \
	shared/crypto.o \
	shared/stream.o \
	shared/tools.o \
	engine/command.o \
	engine/server.o \
	engine/worldio.o \
	fpsgame/entities.o \
	fpsgame/server.o \
	spaghetti/spaghetti.o \
	spaghetti/later.o

default: all

all: server

clean:
	-$(RM) $(SERVER_OBJS) sauer_server* vcpp/mingw.res

ifdef WIN
vcpp/mingw.res:
	$(WINDRES) -I vcpp -i vcpp/mingw.rc -J rc -o vcpp/mingw.res -O coff

server: libenet $(SERVER_OBJS) vcpp/mingw.res
	$(CXX) $(CXXFLAGS) -o sauer_server.exe vcpp/mingw.res $(SERVER_OBJS) $(LDFLAGS) $(LIBS)

else
server:	libenet $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o sauer_server $(SERVER_OBJS) $(LDFLAGS) $(LIBS)
ifdef LINUX
ifneq ($(DEBUG),)
	$(OBJCOPY) --compress-debug-sections --only-keep-debug sauer_server sauer_server.debug
	$(STRIP) sauer_server
	$(OBJCOPY) --add-gnu-debuglink=sauer_server.debug sauer_server
else
	$(STRIP) sauer_server
endif
endif

endif

enet/.libs/libenet.a:
	$(MAKE) -C enet/ all

enet/Makefile:
	cd enet; find . -type f -exec touch -r . {} \;; ./configure --host=$(PLATFORM) --enable-shared=no --enable-static=yes

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
