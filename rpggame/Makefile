CXXFLAGS= -O3 -fomit-frame-pointer
override CXXFLAGS+= -Wall -fsigned-char

PLATFORM= $(shell uname -s)
PLATFORM_PREFIX= native

INCLUDES= -I../shared -I../engine -I../rpggame -I../enet/include

STRIP=
ifeq (,$(findstring -g,$(CXXFLAGS)))
ifeq (,$(findstring -pg,$(CXXFLAGS)))
  STRIP=strip
endif
endif

MV=mv

ifneq (,$(findstring MINGW,$(PLATFORM)))
WINDRES= windres
CLIENT_INCLUDES= $(INCLUDES) -I../include
CLIENT_LIBS= -mwindows -L../lib -lSDL -lSDL_image -lSDL_mixer -lzdll -lopengl32 -lenet -lws2_32 -lwinmm
else	
CLIENT_INCLUDES= $(INCLUDES) -I/usr/X11R6/include `sdl-config --cflags`
CLIENT_LIBS= -L../enet -lenet -L/usr/X11R6/lib `sdl-config --libs` -lSDL_image -lSDL_mixer -lz -lGL
endif
ifeq ($(PLATFORM),Linux)
CLIENT_LIBS+= -lrt
endif
CLIENT_OBJS= \
	../shared/crypto.o \
	../shared/geom.o \
	../shared/stream.o \
	../shared/tools.o \
	../shared/zip.o \
	../engine/3dgui.o \
	../engine/bih.o \
	../engine/blend.o \
	../engine/blob.o \
	../engine/client.o	\
	../engine/command.o \
	../engine/console.o \
	../engine/cubeloader.o \
	../engine/decal.o \
	../engine/dynlight.o \
	../engine/glare.o \
	../engine/grass.o \
	../engine/lightmap.o \
	../engine/main.o \
	../engine/material.o \
	../engine/menus.o \
	../engine/movie.o \
	../engine/normal.o	\
	../engine/octa.o \
	../engine/octaedit.o \
	../engine/octarender.o \
	../engine/physics.o \
	../engine/pvs.o \
	../engine/rendergl.o \
	../engine/rendermodel.o \
	../engine/renderparticles.o \
	../engine/rendersky.o \
	../engine/rendertext.o \
	../engine/renderva.o \
	../engine/server.o	\
	../engine/serverbrowser.o \
	../engine/shader.o \
	../engine/shadowmap.o \
	../engine/sound.o \
	../engine/texture.o \
	../engine/water.o \
	../engine/world.o \
	../engine/worldio.o \
	../rpggame/ent.o \
	../rpggame/entities.o \
	../rpggame/objset.o \
	../rpggame/rpg.o
ifneq (,$(findstring MINGW,$(PLATFORM)))
CLIENT_OBJS+= ../vcpp/SDL_win32_main.o
endif
CLIENT_PCH= ../shared/cube.h.gch ../engine.h.gch ../rpggame/rpg.h.gch

ifeq ($(PLATFORM),SunOS)
CLIENT_LIBS+= -lsocket -lnsl -lX11
endif

default: all

all: client

../enet/Makefile:
	cd ../enet; ./configure
	
libenet: ../enet/Makefile
	$(MAKE)	-C ../enet/ all

clean-enet: enet/Makefile
	$(MAKE) -C ../enet/ clean

clean:
	-$(RM) $(CLIENT_PCH) $(CLIENT_OBJS) rpg_client

%.h.gch: %.h
	$(CXX) $(CXXFLAGS) -o $@.tmp $(subst .h.gch,.h,$@)
	$(MV) $@.tmp $@

$(CLIENT_OBJS): CXXFLAGS += $(CLIENT_INCLUDES)
$(filter ../shared/%,$(CLIENT_OBJS)): $(filter ../shared/%,$(CLIENT_PCH))
$(filter ../engine/%,$(CLIENT_OBJS)): $(filter ../engine/%,$(CLIENT_PCH))
$(filter ../rpggame/%,$(CLIENT_OBJS)): $(filter ../rpggame/%,$(CLIENT_PCH))

ifneq (,$(findstring MINGW,$(PLATFORM)))
../vcpp/%.o:
	$(CXX) $(CXXFLAGS) -c -o $@ $(subst .o,.c,$@) 

client: $(CLIENT_OBJS)
	$(WINDRES) -I ../vcpp -i ../vcpp/mingw.rc -J rc -o ../vcpp/mingw.res -O coff 
	$(CXX) $(CXXFLAGS) -o ../../bin/rpg.exe ../vcpp/mingw.res $(CLIENT_OBJS) $(CLIENT_LIBS)

install: all
else
client:	libenet $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o rpg_client $(CLIENT_OBJS) $(CLIENT_LIBS)
endif

depend:
	makedepend -Y -I../shared -I../engine -I../rpggame $(subst .o,.cpp,$(CLIENT_OBJS))
	makedepend -a -o.h.gch -Y -I../shared -I../engine -I../rpggame $(subst .h.gch,.h,$(CLIENT_PCH))

