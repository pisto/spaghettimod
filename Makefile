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
CLIENT_PCH= shared/cube.h.gch

SERVER_LIBS= -Lenet -lenet -lz
SERVER_OBJS= \
	shared/tools-standalone.o \
	engine/server-standalone.o \
	fpsgame/fps-standalone.o

ifeq ($(shell uname -s),SunOS)
CLIENT_LIBS+= -lsocket -lnsl -lX11
SERVER_LIBS+= -lsocket -lnsl
endif

default: all

all: client server

enet/Makefile:
	cd enet; ./configure
	
libenet: enet/Makefile
	$(MAKE)	-C enet/ all

clean: enet/Makefile
	-$(RM) $(SERVER_OBJS) $(CLIENT_OBJS) sauer_server sauer_client
	$(MAKE)	-C enet/ clean

%.h.gch:
	$(CXX) $(CXXFLAGS) -o $@ $(subst .h.gch,.h,$@)

%-standalone.o:
	$(CXX) $(CXXFLAGS) -DSTANDALONE -c -o $@ $(subst -standalone.o,.cpp,$@)

client:	libenet $(CLIENT_PCH) $(CLIENT_OBJS)
	$(CXX) $(CXXFLAGS) -o sauer_client $(CLIENT_OBJS) $(CLIENT_LIBS)

server:	libenet $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) -o sauer_server $(SERVER_OBJS) $(SERVER_LIBS)  
	
install: all
	cp sauer_client	../bin_unix/$(PLATFORM_PREFIX)_client
	cp sauer_server	../bin_unix/$(PLATFORM_PREFIX)_server
	strip ../bin_unix/$(PLATFORM_PREFIX)_client
	strip ../bin_unix/$(PLATFORM_PREFIX)_server

depend:
	makedepend -Y -Ishared -Iengine -Ifpsgame $(subst .o,.cpp,$(CLIENT_OBJS))
	makedepend -a -o-standalone.o -Y -Ishared -Iengine -Ifpsgame $(subst -standalone.o,.cpp,$(SERVER_OBJS))
	makedepend -a -o.h.gch -Y -Ishared -Iengine -Ifpsgame shared/cube.h

# DO NOT DELETE

shared/tools.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
shared/tools.o: shared/command.h shared/iengine.h shared/igame.h
shared/geom.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
shared/geom.o: shared/command.h shared/iengine.h shared/igame.h
engine/3dgui.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/3dgui.o: shared/command.h shared/iengine.h shared/igame.h
engine/3dgui.o: engine/engine.h engine/world.h engine/octa.h
engine/3dgui.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/3dgui.o: engine/model.h engine/textedit.h
engine/bih.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/bih.o: shared/command.h shared/iengine.h shared/igame.h
engine/bih.o: engine/engine.h engine/world.h engine/octa.h engine/lightmap.h
engine/bih.o: engine/bih.h engine/texture.h engine/model.h
engine/blend.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/blend.o: shared/command.h shared/iengine.h shared/igame.h
engine/blend.o: engine/engine.h engine/world.h engine/octa.h
engine/blend.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/blend.o: engine/model.h
engine/blob.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/blob.o: shared/command.h shared/iengine.h shared/igame.h
engine/blob.o: engine/engine.h engine/world.h engine/octa.h engine/lightmap.h
engine/blob.o: engine/bih.h engine/texture.h engine/model.h
engine/client.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/client.o: shared/command.h shared/iengine.h shared/igame.h
engine/client.o: engine/engine.h engine/world.h engine/octa.h
engine/client.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/client.o: engine/model.h
engine/command.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/command.o: shared/command.h shared/iengine.h shared/igame.h
engine/command.o: engine/engine.h engine/world.h engine/octa.h
engine/command.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/command.o: engine/model.h
engine/console.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/console.o: shared/command.h shared/iengine.h shared/igame.h
engine/console.o: engine/engine.h engine/world.h engine/octa.h
engine/console.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/console.o: engine/model.h
engine/cubeloader.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/cubeloader.o: shared/command.h shared/iengine.h shared/igame.h
engine/cubeloader.o: engine/engine.h engine/world.h engine/octa.h
engine/cubeloader.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/cubeloader.o: engine/model.h
engine/decal.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/decal.o: shared/command.h shared/iengine.h shared/igame.h
engine/decal.o: engine/engine.h engine/world.h engine/octa.h
engine/decal.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/decal.o: engine/model.h
engine/dynlight.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/dynlight.o: shared/command.h shared/iengine.h shared/igame.h
engine/dynlight.o: engine/engine.h engine/world.h engine/octa.h
engine/dynlight.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/dynlight.o: engine/model.h
engine/glare.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/glare.o: shared/command.h shared/iengine.h shared/igame.h
engine/glare.o: engine/engine.h engine/world.h engine/octa.h
engine/glare.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/glare.o: engine/model.h engine/rendertarget.h
engine/grass.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/grass.o: shared/command.h shared/iengine.h shared/igame.h
engine/grass.o: engine/engine.h engine/world.h engine/octa.h
engine/grass.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/grass.o: engine/model.h
engine/lightmap.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/lightmap.o: shared/command.h shared/iengine.h shared/igame.h
engine/lightmap.o: engine/engine.h engine/world.h engine/octa.h
engine/lightmap.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/lightmap.o: engine/model.h
engine/main.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/main.o: shared/command.h shared/iengine.h shared/igame.h
engine/main.o: engine/engine.h engine/world.h engine/octa.h engine/lightmap.h
engine/main.o: engine/bih.h engine/texture.h engine/model.h
engine/material.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/material.o: shared/command.h shared/iengine.h shared/igame.h
engine/material.o: engine/engine.h engine/world.h engine/octa.h
engine/material.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/material.o: engine/model.h
engine/menus.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/menus.o: shared/command.h shared/iengine.h shared/igame.h
engine/menus.o: engine/engine.h engine/world.h engine/octa.h
engine/menus.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/menus.o: engine/model.h
engine/normal.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/normal.o: shared/command.h shared/iengine.h shared/igame.h
engine/normal.o: engine/engine.h engine/world.h engine/octa.h
engine/normal.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/normal.o: engine/model.h
engine/octa.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/octa.o: shared/command.h shared/iengine.h shared/igame.h
engine/octa.o: engine/engine.h engine/world.h engine/octa.h engine/lightmap.h
engine/octa.o: engine/bih.h engine/texture.h engine/model.h
engine/octaedit.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/octaedit.o: shared/command.h shared/iengine.h shared/igame.h
engine/octaedit.o: engine/engine.h engine/world.h engine/octa.h
engine/octaedit.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/octaedit.o: engine/model.h
engine/octarender.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/octarender.o: shared/command.h shared/iengine.h shared/igame.h
engine/octarender.o: engine/engine.h engine/world.h engine/octa.h
engine/octarender.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/octarender.o: engine/model.h
engine/physics.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/physics.o: shared/command.h shared/iengine.h shared/igame.h
engine/physics.o: engine/engine.h engine/world.h engine/octa.h
engine/physics.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/physics.o: engine/model.h
engine/pvs.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/pvs.o: shared/command.h shared/iengine.h shared/igame.h
engine/pvs.o: engine/engine.h engine/world.h engine/octa.h engine/lightmap.h
engine/pvs.o: engine/bih.h engine/texture.h engine/model.h
engine/rendergl.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/rendergl.o: shared/command.h shared/iengine.h shared/igame.h
engine/rendergl.o: engine/engine.h engine/world.h engine/octa.h
engine/rendergl.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/rendergl.o: engine/model.h
engine/rendermodel.o: shared/cube.h shared/tools.h shared/geom.h
engine/rendermodel.o: shared/ents.h shared/command.h shared/iengine.h
engine/rendermodel.o: shared/igame.h engine/engine.h engine/world.h
engine/rendermodel.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/rendermodel.o: engine/texture.h engine/model.h engine/ragdoll.h
engine/rendermodel.o: engine/animmodel.h engine/vertmodel.h
engine/rendermodel.o: engine/skelmodel.h engine/md2.h engine/md3.h
engine/rendermodel.o: engine/md5.h engine/obj.h
engine/renderparticles.o: shared/cube.h shared/tools.h shared/geom.h
engine/renderparticles.o: shared/ents.h shared/command.h shared/iengine.h
engine/renderparticles.o: shared/igame.h engine/engine.h engine/world.h
engine/renderparticles.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/renderparticles.o: engine/texture.h engine/model.h
engine/renderparticles.o: engine/rendertarget.h engine/depthfx.h
engine/renderparticles.o: engine/explosion.h engine/lensflare.h
engine/renderparticles.o: engine/lightning.h
engine/rendersky.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/rendersky.o: shared/command.h shared/iengine.h shared/igame.h
engine/rendersky.o: engine/engine.h engine/world.h engine/octa.h
engine/rendersky.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/rendersky.o: engine/model.h
engine/rendertext.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/rendertext.o: shared/command.h shared/iengine.h shared/igame.h
engine/rendertext.o: engine/engine.h engine/world.h engine/octa.h
engine/rendertext.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/rendertext.o: engine/model.h
engine/renderva.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/renderva.o: shared/command.h shared/iengine.h shared/igame.h
engine/renderva.o: engine/engine.h engine/world.h engine/octa.h
engine/renderva.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/renderva.o: engine/model.h
engine/server.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/server.o: shared/command.h shared/iengine.h shared/igame.h
engine/server.o: engine/engine.h engine/world.h engine/octa.h
engine/server.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/server.o: engine/model.h
engine/serverbrowser.o: shared/cube.h shared/tools.h shared/geom.h
engine/serverbrowser.o: shared/ents.h shared/command.h shared/iengine.h
engine/serverbrowser.o: shared/igame.h engine/engine.h engine/world.h
engine/serverbrowser.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/serverbrowser.o: engine/texture.h engine/model.h
engine/shader.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/shader.o: shared/command.h shared/iengine.h shared/igame.h
engine/shader.o: engine/engine.h engine/world.h engine/octa.h
engine/shader.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/shader.o: engine/model.h
engine/shadowmap.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/shadowmap.o: shared/command.h shared/iengine.h shared/igame.h
engine/shadowmap.o: engine/engine.h engine/world.h engine/octa.h
engine/shadowmap.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/shadowmap.o: engine/model.h engine/rendertarget.h
engine/sound.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/sound.o: shared/command.h shared/iengine.h shared/igame.h
engine/sound.o: engine/engine.h engine/world.h engine/octa.h
engine/sound.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/sound.o: engine/model.h
engine/texture.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/texture.o: shared/command.h shared/iengine.h shared/igame.h
engine/texture.o: engine/engine.h engine/world.h engine/octa.h
engine/texture.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/texture.o: engine/model.h engine/scale.h
engine/water.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/water.o: shared/command.h shared/iengine.h shared/igame.h
engine/water.o: engine/engine.h engine/world.h engine/octa.h
engine/water.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/water.o: engine/model.h
engine/world.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/world.o: shared/command.h shared/iengine.h shared/igame.h
engine/world.o: engine/engine.h engine/world.h engine/octa.h
engine/world.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/world.o: engine/model.h
engine/worldio.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
engine/worldio.o: shared/command.h shared/iengine.h shared/igame.h
engine/worldio.o: engine/engine.h engine/world.h engine/octa.h
engine/worldio.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/worldio.o: engine/model.h
fpsgame/fps.o: shared/cube.h shared/tools.h shared/geom.h shared/ents.h
fpsgame/fps.o: shared/command.h shared/iengine.h shared/igame.h
fpsgame/fps.o: fpsgame/game.h shared/hash.h fpsgame/fpsserver.h
fpsgame/fps.o: fpsgame/capture.h fpsgame/ctf.h fpsgame/auth.h
fpsgame/fps.o: fpsgame/extinfo.h shared/crypto.h fpsgame/weapon.h
fpsgame/fps.o: fpsgame/monster.h fpsgame/movable.h fpsgame/scoreboard.h
fpsgame/fps.o: fpsgame/fpsrender.h fpsgame/entities.h fpsgame/client.h

shared/tools-standalone.o: shared/cube.h shared/tools.h shared/geom.h
shared/tools-standalone.o: shared/ents.h shared/command.h shared/iengine.h
shared/tools-standalone.o: shared/igame.h
engine/server-standalone.o: shared/cube.h shared/tools.h shared/geom.h
engine/server-standalone.o: shared/ents.h shared/command.h shared/iengine.h
engine/server-standalone.o: shared/igame.h engine/engine.h engine/world.h
engine/server-standalone.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/server-standalone.o: engine/texture.h engine/model.h
fpsgame/fps-standalone.o: shared/cube.h shared/tools.h shared/geom.h
fpsgame/fps-standalone.o: shared/ents.h shared/command.h shared/iengine.h
fpsgame/fps-standalone.o: shared/igame.h fpsgame/game.h shared/hash.h
fpsgame/fps-standalone.o: fpsgame/fpsserver.h fpsgame/capture.h fpsgame/ctf.h
fpsgame/fps-standalone.o: fpsgame/auth.h fpsgame/extinfo.h shared/crypto.h
fpsgame/fps-standalone.o: fpsgame/weapon.h fpsgame/monster.h
fpsgame/fps-standalone.o: fpsgame/movable.h fpsgame/scoreboard.h
fpsgame/fps-standalone.o: fpsgame/fpsrender.h fpsgame/entities.h
fpsgame/fps-standalone.o: fpsgame/client.h

shared/cube.h.gch: shared/tools.h shared/geom.h shared/ents.h
shared/cube.h.gch: shared/command.h shared/iengine.h shared/igame.h
