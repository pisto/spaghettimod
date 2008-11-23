CXXOPTFLAGS= -O3 -fomit-frame-pointer
INCLUDES= -Ishared -Iengine -Ifpsgame -Ienet/include -I/usr/X11R6/include `sdl-config --cflags`
CXXFLAGS= -Wall -fsigned-char $(CXXOPTFLAGS) $(INCLUDES)

PLATFORM_PREFIX=native

CLIENT_LIBS= -Lenet -lenet -L/usr/X11R6/lib `sdl-config --libs` -lSDL_image -lSDL_mixer -lz -lGL
ifeq ($(shell uname -s),Linux)
CLIENT_LIBS+= -lrt
endif
CLIENT_OBJS= \
	shared/tools.o \
	shared/geom.o \
	engine/3dgui.o \
	engine/bih.o \
	engine/blend.o \
	engine/blob.o \
	engine/client.o	\
	engine/command.o \
	engine/console.o \
	engine/cubeloader.o \
	engine/decal.o \
	engine/dynlight.o \
	engine/glare.o \
	engine/grass.o \
	engine/lightmap.o \
	engine/main.o \
	engine/material.o \
	engine/menus.o \
	engine/normal.o	\
	engine/octa.o \
	engine/octaedit.o \
	engine/octarender.o \
	engine/physics.o \
	engine/pvs.o \
	engine/rendergl.o \
	engine/rendermodel.o \
	engine/renderparticles.o \
	engine/rendersky.o \
	engine/rendertext.o \
	engine/renderva.o \
	engine/server.o	\
	engine/serverbrowser.o \
	engine/shader.o \
	engine/shadowmap.o \
	engine/sound.o \
	engine/texture.o \
	engine/water.o \
	engine/world.o \
	engine/worldio.o \
	fpsgame/fps.o

SERVER_LIBS= -Lenet -lenet -lz
SERVER_OBJS= \
	shared/tools-standalone.o \
	engine/server-standalone.o \
	fpsgame/fps-standalone.o

default: all

all: client server

enet/Makefile:
	cd enet; ./configure
	
libenet: enet/Makefile
	$(MAKE)	-C enet/ all

clean: enet/Makefile
	-$(RM) $(SERVER_OBJS) $(CLIENT_OBJS) sauer_server sauer_client
	$(MAKE)	-C enet/ clean

%-standalone.o:
	$(CXX) $(CXXFLAGS) -DSTANDALONE -c -o $@ $(subst -standalone.o,.cpp,$@)

client:	libenet $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o sauer_client $(CLIENT_OBJS) $(CLIENT_LIBS)

server:	libenet $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o sauer_server $(SERVER_OBJS) $(SERVER_LIBS)  
	
install: all
	cp sauer_client	../bin_unix/$(PLATFORM_PREFIX)_client
	cp sauer_server	../bin_unix/$(PLATFORM_PREFIX)_server
	strip ../bin_unix/$(PLATFORM_PREFIX)_client
	strip ../bin_unix/$(PLATFORM_PREFIX)_server

