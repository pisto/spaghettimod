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

depend:
	makedepend -Y -Ishared -Iengine -Ifpsgame $(subst .o,.cpp,$(CLIENT_OBJS))
	makedepend -a -o-standalone.o -Y -Ishared -Iengine -Ifpsgame $(subst -standalone.o,.cpp,$(SERVER_OBJS))

# DO NOT DELETE

shared/tools.o: shared/pch.h shared/tools.h
shared/geom.o: shared/pch.h shared/tools.h shared/geom.h
engine/3dgui.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/3dgui.o: shared/geom.h shared/ents.h shared/command.h shared/iengine.h
engine/3dgui.o: shared/igame.h engine/world.h engine/octa.h engine/lightmap.h
engine/3dgui.o: engine/bih.h engine/texture.h engine/model.h
engine/3dgui.o: engine/textedit.h
engine/bih.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/bih.o: shared/geom.h shared/ents.h shared/command.h shared/iengine.h
engine/bih.o: shared/igame.h engine/world.h engine/octa.h engine/lightmap.h
engine/bih.o: engine/bih.h engine/texture.h engine/model.h
engine/blend.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/blend.o: shared/geom.h shared/ents.h shared/command.h shared/iengine.h
engine/blend.o: shared/igame.h engine/world.h engine/octa.h engine/lightmap.h
engine/blend.o: engine/bih.h engine/texture.h engine/model.h
engine/blob.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/blob.o: shared/geom.h shared/ents.h shared/command.h shared/iengine.h
engine/blob.o: shared/igame.h engine/world.h engine/octa.h engine/lightmap.h
engine/blob.o: engine/bih.h engine/texture.h engine/model.h
engine/client.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/client.o: shared/geom.h shared/ents.h shared/command.h
engine/client.o: shared/iengine.h shared/igame.h engine/world.h engine/octa.h
engine/client.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/client.o: engine/model.h
engine/command.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/command.o: shared/geom.h shared/ents.h shared/command.h
engine/command.o: shared/iengine.h shared/igame.h engine/world.h
engine/command.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/command.o: engine/texture.h engine/model.h
engine/console.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/console.o: shared/geom.h shared/ents.h shared/command.h
engine/console.o: shared/iengine.h shared/igame.h engine/world.h
engine/console.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/console.o: engine/texture.h engine/model.h
engine/cubeloader.o: shared/pch.h engine/engine.h shared/cube.h
engine/cubeloader.o: shared/tools.h shared/geom.h shared/ents.h
engine/cubeloader.o: shared/command.h shared/iengine.h shared/igame.h
engine/cubeloader.o: engine/world.h engine/octa.h engine/lightmap.h
engine/cubeloader.o: engine/bih.h engine/texture.h engine/model.h
engine/decal.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/decal.o: shared/geom.h shared/ents.h shared/command.h shared/iengine.h
engine/decal.o: shared/igame.h engine/world.h engine/octa.h engine/lightmap.h
engine/decal.o: engine/bih.h engine/texture.h engine/model.h
engine/dynlight.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/dynlight.o: shared/geom.h shared/ents.h shared/command.h
engine/dynlight.o: shared/iengine.h shared/igame.h engine/world.h
engine/dynlight.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/dynlight.o: engine/texture.h engine/model.h
engine/glare.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/glare.o: shared/geom.h shared/ents.h shared/command.h shared/iengine.h
engine/glare.o: shared/igame.h engine/world.h engine/octa.h engine/lightmap.h
engine/glare.o: engine/bih.h engine/texture.h engine/model.h
engine/glare.o: engine/rendertarget.h
engine/grass.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/grass.o: shared/geom.h shared/ents.h shared/command.h shared/iengine.h
engine/grass.o: shared/igame.h engine/world.h engine/octa.h engine/lightmap.h
engine/grass.o: engine/bih.h engine/texture.h engine/model.h
engine/lightmap.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/lightmap.o: shared/geom.h shared/ents.h shared/command.h
engine/lightmap.o: shared/iengine.h shared/igame.h engine/world.h
engine/lightmap.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/lightmap.o: engine/texture.h engine/model.h
engine/main.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/main.o: shared/geom.h shared/ents.h shared/command.h shared/iengine.h
engine/main.o: shared/igame.h engine/world.h engine/octa.h engine/lightmap.h
engine/main.o: engine/bih.h engine/texture.h engine/model.h
engine/material.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/material.o: shared/geom.h shared/ents.h shared/command.h
engine/material.o: shared/iengine.h shared/igame.h engine/world.h
engine/material.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/material.o: engine/texture.h engine/model.h
engine/menus.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/menus.o: shared/geom.h shared/ents.h shared/command.h shared/iengine.h
engine/menus.o: shared/igame.h engine/world.h engine/octa.h engine/lightmap.h
engine/menus.o: engine/bih.h engine/texture.h engine/model.h
engine/normal.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/normal.o: shared/geom.h shared/ents.h shared/command.h
engine/normal.o: shared/iengine.h shared/igame.h engine/world.h engine/octa.h
engine/normal.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/normal.o: engine/model.h
engine/octa.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/octa.o: shared/geom.h shared/ents.h shared/command.h shared/iengine.h
engine/octa.o: shared/igame.h engine/world.h engine/octa.h engine/lightmap.h
engine/octa.o: engine/bih.h engine/texture.h engine/model.h
engine/octaedit.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/octaedit.o: shared/geom.h shared/ents.h shared/command.h
engine/octaedit.o: shared/iengine.h shared/igame.h engine/world.h
engine/octaedit.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/octaedit.o: engine/texture.h engine/model.h
engine/octarender.o: shared/pch.h engine/engine.h shared/cube.h
engine/octarender.o: shared/tools.h shared/geom.h shared/ents.h
engine/octarender.o: shared/command.h shared/iengine.h shared/igame.h
engine/octarender.o: engine/world.h engine/octa.h engine/lightmap.h
engine/octarender.o: engine/bih.h engine/texture.h engine/model.h
engine/physics.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/physics.o: shared/geom.h shared/ents.h shared/command.h
engine/physics.o: shared/iengine.h shared/igame.h engine/world.h
engine/physics.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/physics.o: engine/texture.h engine/model.h
engine/pvs.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/pvs.o: shared/geom.h shared/ents.h shared/command.h shared/iengine.h
engine/pvs.o: shared/igame.h engine/world.h engine/octa.h engine/lightmap.h
engine/pvs.o: engine/bih.h engine/texture.h engine/model.h
engine/rendergl.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/rendergl.o: shared/geom.h shared/ents.h shared/command.h
engine/rendergl.o: shared/iengine.h shared/igame.h engine/world.h
engine/rendergl.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/rendergl.o: engine/texture.h engine/model.h
engine/rendermodel.o: shared/pch.h engine/engine.h shared/cube.h
engine/rendermodel.o: shared/tools.h shared/geom.h shared/ents.h
engine/rendermodel.o: shared/command.h shared/iengine.h shared/igame.h
engine/rendermodel.o: engine/world.h engine/octa.h engine/lightmap.h
engine/rendermodel.o: engine/bih.h engine/texture.h engine/model.h
engine/rendermodel.o: engine/ragdoll.h engine/animmodel.h engine/vertmodel.h
engine/rendermodel.o: engine/skelmodel.h engine/md2.h engine/md3.h
engine/rendermodel.o: engine/md5.h engine/obj.h
engine/renderparticles.o: shared/pch.h engine/engine.h shared/cube.h
engine/renderparticles.o: shared/tools.h shared/geom.h shared/ents.h
engine/renderparticles.o: shared/command.h shared/iengine.h shared/igame.h
engine/renderparticles.o: engine/world.h engine/octa.h engine/lightmap.h
engine/renderparticles.o: engine/bih.h engine/texture.h engine/model.h
engine/renderparticles.o: engine/rendertarget.h engine/depthfx.h
engine/renderparticles.o: engine/explosion.h engine/lensflare.h
engine/renderparticles.o: engine/lightning.h
engine/rendersky.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/rendersky.o: shared/geom.h shared/ents.h shared/command.h
engine/rendersky.o: shared/iengine.h shared/igame.h engine/world.h
engine/rendersky.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/rendersky.o: engine/texture.h engine/model.h
engine/rendertext.o: shared/pch.h engine/engine.h shared/cube.h
engine/rendertext.o: shared/tools.h shared/geom.h shared/ents.h
engine/rendertext.o: shared/command.h shared/iengine.h shared/igame.h
engine/rendertext.o: engine/world.h engine/octa.h engine/lightmap.h
engine/rendertext.o: engine/bih.h engine/texture.h engine/model.h
engine/renderva.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/renderva.o: shared/geom.h shared/ents.h shared/command.h
engine/renderva.o: shared/iengine.h shared/igame.h engine/world.h
engine/renderva.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/renderva.o: engine/texture.h engine/model.h
engine/server.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/server.o: shared/geom.h shared/ents.h shared/command.h
engine/server.o: shared/iengine.h shared/igame.h engine/world.h engine/octa.h
engine/server.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/server.o: engine/model.h
engine/serverbrowser.o: shared/pch.h engine/engine.h shared/cube.h
engine/serverbrowser.o: shared/tools.h shared/geom.h shared/ents.h
engine/serverbrowser.o: shared/command.h shared/iengine.h shared/igame.h
engine/serverbrowser.o: engine/world.h engine/octa.h engine/lightmap.h
engine/serverbrowser.o: engine/bih.h engine/texture.h engine/model.h
engine/shader.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/shader.o: shared/geom.h shared/ents.h shared/command.h
engine/shader.o: shared/iengine.h shared/igame.h engine/world.h engine/octa.h
engine/shader.o: engine/lightmap.h engine/bih.h engine/texture.h
engine/shader.o: engine/model.h
engine/shadowmap.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/shadowmap.o: shared/geom.h shared/ents.h shared/command.h
engine/shadowmap.o: shared/iengine.h shared/igame.h engine/world.h
engine/shadowmap.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/shadowmap.o: engine/texture.h engine/model.h engine/rendertarget.h
engine/sound.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/sound.o: shared/geom.h shared/ents.h shared/command.h shared/iengine.h
engine/sound.o: shared/igame.h engine/world.h engine/octa.h engine/lightmap.h
engine/sound.o: engine/bih.h engine/texture.h engine/model.h
engine/texture.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/texture.o: shared/geom.h shared/ents.h shared/command.h
engine/texture.o: shared/iengine.h shared/igame.h engine/world.h
engine/texture.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/texture.o: engine/texture.h engine/model.h engine/scale.h
engine/water.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/water.o: shared/geom.h shared/ents.h shared/command.h shared/iengine.h
engine/water.o: shared/igame.h engine/world.h engine/octa.h engine/lightmap.h
engine/water.o: engine/bih.h engine/texture.h engine/model.h
engine/world.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/world.o: shared/geom.h shared/ents.h shared/command.h shared/iengine.h
engine/world.o: shared/igame.h engine/world.h engine/octa.h engine/lightmap.h
engine/world.o: engine/bih.h engine/texture.h engine/model.h
engine/worldio.o: shared/pch.h engine/engine.h shared/cube.h shared/tools.h
engine/worldio.o: shared/geom.h shared/ents.h shared/command.h
engine/worldio.o: shared/iengine.h shared/igame.h engine/world.h
engine/worldio.o: engine/octa.h engine/lightmap.h engine/bih.h
engine/worldio.o: engine/texture.h engine/model.h
fpsgame/fps.o: shared/pch.h shared/cube.h shared/tools.h shared/geom.h
fpsgame/fps.o: shared/ents.h shared/command.h shared/iengine.h shared/igame.h
fpsgame/fps.o: fpsgame/game.h shared/hash.h fpsgame/fpsserver.h
fpsgame/fps.o: fpsgame/capture.h fpsgame/ctf.h fpsgame/auth.h
fpsgame/fps.o: fpsgame/extinfo.h shared/crypto.h fpsgame/weapon.h
fpsgame/fps.o: fpsgame/monster.h fpsgame/movable.h fpsgame/scoreboard.h
fpsgame/fps.o: fpsgame/fpsrender.h fpsgame/entities.h fpsgame/client.h

shared/tools-standalone.o: shared/pch.h shared/tools.h
engine/server-standalone.o: shared/pch.h engine/engine.h shared/cube.h
engine/server-standalone.o: shared/tools.h shared/geom.h shared/ents.h
engine/server-standalone.o: shared/command.h shared/iengine.h shared/igame.h
engine/server-standalone.o: engine/world.h engine/octa.h engine/lightmap.h
engine/server-standalone.o: engine/bih.h engine/texture.h engine/model.h
fpsgame/fps-standalone.o: shared/pch.h shared/cube.h shared/tools.h
fpsgame/fps-standalone.o: shared/geom.h shared/ents.h shared/command.h
fpsgame/fps-standalone.o: shared/iengine.h shared/igame.h fpsgame/game.h
fpsgame/fps-standalone.o: shared/hash.h fpsgame/fpsserver.h fpsgame/capture.h
fpsgame/fps-standalone.o: fpsgame/ctf.h fpsgame/auth.h fpsgame/extinfo.h
fpsgame/fps-standalone.o: shared/crypto.h fpsgame/weapon.h fpsgame/monster.h
fpsgame/fps-standalone.o: fpsgame/movable.h fpsgame/scoreboard.h
fpsgame/fps-standalone.o: fpsgame/fpsrender.h fpsgame/entities.h
fpsgame/fps-standalone.o: fpsgame/client.h
