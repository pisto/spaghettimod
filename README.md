#What is this?

**spaghettimod** is a Cube 2: Sauerbraten server mod with Lua scripting. It is completely different from Hopmod (the other Lua server mod around). It is for modders who already have a good knowledge of the Sauerbraten server codebase, and just want tools to easily extend that with Lua. It is *not* for clan leaders who just want a personalized (read, custom message color or little more) server.

For this reason, the principles are mostly coding principles.

* minimize impact on the C++ codebase, for easier merging and predictable behavior
* expose the C++ engine as-is to Lua, making it easier to write Lua code as if it was C++ inlined into the vanilla implementation
* no bloat, and use external libraries as much as possible (fight the NIH syndrome)
* provide modular standard libraries for most wanted stuff, but configuration is done through a Lua script, not configuration variables
* no cubescript (`VAR` and `COMMAND` are mirrored to Lua)
* ease of debugging

I am available in Gamesurge as pisto. It is called **spaghettimod** because I'm italian.

#Example configuration

There are some files in *script/load.d*, which enable some sane default configuration. These modules are:

1. *100-logging.lua* : improved logging with date, renames, etc
  * showcasing simple event hooking and modification of arguments
2. *200-geoip.lua* : show Geoip on client connect, and provide the `#geoip [cn]`
  * showcasing external module loading and the `std.command` module
3. *2100-demorecord.lua* : record demos
  * showcasing default folder structure: server instances can be tagged with unique names (by default, the listen port), and some modules use that to create unique folder or files (in this case, *\<tag\>.demos*)
4. *2200-serverexec.lua* : create a unix socket for a Lua interactive shell, connect with `socat READLINE,history=.spaghetti_history UNIX-CLIENT:./28785.serverexec`
  * showcasing `std.cmdpipe` and awesomeness.

Optionally, you can enable the fork-to-background, full-reload module (requires `lua-posix`). Just do `(cd script/load.d/; ln -s off/3000-shelldetach.lua)` and restart. The server will now fork to background, write the log to *28785.log*, and it will perform a full restart of the C++ program when `SIGUSR1` is received. Updating to the latest revision is as easy as `git pull && make && killall -s SIGUSR1 sauer_server`.

###The PISTOVPS configuration

If you start the server with the enviroment variable `PISTOVPS` set (`PISTOVPS=1 ./sauer_server`), you will have a clone of the server that I run myself (*github:spaghettimod* in the server browser). *1000-sample-config.lua* sports a more real life configuration, including map rotation and abuse protection.

#Compilation

Compilation has been tested with luajit, lua 5.2, lua 5.1, on Mac OS X, Windows and Linux. The default scripts are written with a Unix environment in mind, so most probably they won't work under Windows.

This code depends on libz and pkg-config.

The Makefile is different from vanilla but acts the same. `make` generally should suffice to create the executable in the top folder.

The Lua version is determined automatically, preferring in order luajit, lua5.2, lua, lua5.1. If you wish to select a version manually, pass `LUAVERSION` to `make`, e.g. `make LUAVERSION=5.3`.

If you want to cross compile, pass the host triplet on the command line with `PLATFORM`, e.g. `make PLATFORM=x86_64-w64-mingw32`.

To change the optimization settings, you sould use the `OPTFLAGS` variable instead of `CXXFLAGS` directly.

###RAM usage during compilation

Compilation of *{engine,fpsgame}/server.cpp* take an enormous amount of RAM (~ 700MB) because of heavy template instantiation in the Lua binding code. This can be a problem on a resource limited VPS. If you hit this problem, try this:

```bash
fallocate -l 2G /swapfile
chmod 600 /swapfile
mkswap /swapfile
swapon /swapfile
```

`clang` is known to use around one fourth less memory. Additionally, you can disable debug symbols generation with `DEBUG=""`: this will save one fourth of memory in `g++`, roughly half in `clang` (it will also make compilation around one third faster). If you have a crash and find yourself without debugging symbols you can try to rebuild spaghettimod without the `DEBUG` modifier and hope that the output executable is the same as the old one at binary level, which is generally the case but not guaranteed. `gdb` will also spit out the warning "exec file is newer than core file.", but you can ignore that. With these hints (`clang` and `DEBUG=""`) you can get down to ~ 300MB memory usage.

###Debugging

You can debug the server and client code, C++ and Lua, without network timeouts, thanks to the patch [enetnotimeouts.diff](https://raw.github.com/pisto/spaghettimod/master/enetnotimeouts.diff). Follow this procedure:

1. download and install the [SDoS test client](https://github.com/pisto/sdos-test), or apply the patch to your client
2. start the server, optionally under a C++ debugger
  * the standard bootstrap file tries to run the [Eclipse LDT remote debugger](https://wiki.eclipse.org/Koneki/LDT/Developer_Area/User_Guides/User_Guide_1.1#Attach_Debug), which must be placed in the top directory
3. start the client, optionally under a debugger, and connect to the server
4. issue `/enetnotimeouts 1` on the client, and `engine.serverhost.noTimeouts = 1` on the server

With these steps, you can stop the execution with any of the three debuggers involved, and resume at will without timeouts.

#Information for Lua modders

The Lua API tries to be as much as similar to the C++ code. Generally you can write basically the same stuff in Lua and C++ (replacing `->` with `.` maybe). This also means that there is no handholding: very little is being checked for sanity (like in C++), your lua script *can* crash the server, and don't even think to run Lua script sent by the client, exploits are possible.

Things that are accessible in the C++ global namespace `::` are bound to Lua in the table `engine`, and things that are in `server::` are bound in `server`. This means that the crypto functions are in `engine`. There is an extra predefined table, `spaghetti`, which holds user defined events (see later) and a field, `spaghetti.quit`: once this is set to true, it cannot be unset, and the server will shutdown as soon as possible. This way to shutdown the server is preferred to just terminate the program from lua, or even from calling the sauerbraten shutdown functions from Lua.

Cubescript has been totally stripped. variables and commands are exported to Lua in the table `cs` as variables that you can read/write and functions that you can call.

**spaghettimod** tries to minimize the modifications to the vanilla code. This is reflected also in the way that C++ and Lua interact. Whereas Lua can access almost all the internals of the sauerbraten server, the interaction C++ -> Lua happens through
* binding
* calling *script/bootstrap.lua* at boot
* issuing events

The default bootstrap file just export two event related helpers (see next section), and calls the files in *script/load.d/*, which have to follow the naming scheme *##-somename.lua*, where *##* determines the relative order in calling the files (from lower to higher).

###Events

Events are calls that the C++ code makes to Lua. When a specific even occurs, the engine runs this code:

```lua
local argument_table = {
    -- event specific named arguments
}
local listener = spaghetti[event_type]
if listener then listener(argument_table) end
```

The arguments are usually linked directly to C++ function variables, and the modifications you do in Lua are reflected in C++. Some arguments might be read only.

If the event is cancellable (with semantics specific to the event), the argument table contains a field `skip`, which if set to true, once the listener returns, causes the event to be cancelled. Cancellable events are issued before "side effects" take place, and non cancellable events after.

The number and kind of events is in flux, the arguments passed correspond, most of the time, to the C++ function variables, and the exact meaning of cancellation depends on the kind of event. Hence it's rather pointless to write down a list here, since it would need to constantly refer to code lines. You can work out a list of event with some `grep` commands.
* cancellable events: `grep -RF simplehook engine/ fpsgame/ shared/`
* non cancellable events: `grep -RE "simple(const)?event" engine/ fpsgame/ shared/ spaghetti/spaghetti.cpp`
The results are in the form `spaghettimod::issueevent(event_type, args...)`, where issueevent is one of simplehook, simpleevent, simpleconstevent. The `event_type` is either a `N_*` enum which correspond to `server.N_*` in Lua, or `spaghetti::hotstring::event_type`, which means `"event_type"` in Lua.

So far this is the only hardcoded behavior, but the *script/bootstrap.lua* that comes with upstream adds two functions: `spaghetti.addhook(event_type, your_callback, do_prepend)` and `spaghetti.removehook(token)`. They implement a simple event listeners multiplexer: you add a listener with `local hook_token = spaghetti.addhook(event_type, your_callback)`, and you remove it with `spaghetti.removehook(hook_token)`. Hooks are called in the order that they are installed, and you can force a hook to be put first in the list with `do_prepend = true`.

###Caveats on bindings (important!)

`packetbuf:finalize()` *disowns* the `ENetPacket` from the `packetbuf`. If you don't pass the packet directly to enet or you don't destroy it, you have a memory leak.

In C++ the cryptographic functions return generally pointers to `void*` and have to be freed. Lua returns and takes strings with literal or binary hashes (`grep -F addFunction shared/crypto.cpp`).

The original sauer implementation of hash swaps the nibbles (e.g. byte 0x4F is written as 0xF4). This is kept for compatibility, but if you want to get a correct tigersum use `engine.hashstring(yourdata, true)`.

`ucharbuf`, `vector<uchar>`, `packetbuf` now have method versions for `sendstring` `putint` `putuint` `putfloat` (they return the object itself so you can make a dot chain), `getstring` `getint` `getuint` `getfloat`.

Some C++ structures that represent binary buffers map to Lua strings by accessing the `char*` (or `void*`) pointer: `ENetPacket` (read only), `ENetBuffer` (read-write), `ucharbuf` (read only) (`grep -F lua_buff_type engine/server.cpp fpsgame/server.cpp shared/crypto.cpp`).

Some functions that require a binary buffer are proxied by functions that take strings, or functions that require an output buffer just return a new string (along with the original return, if applicable): `enet_packet_create`, `decodeutf8`, `encodeutf8`, `filtertext`, `hashstring`, `genprivkey`, `processmasterinput`... (`grep -E '\.add.*\+\[\]' engine/server.cpp fpsgame/server.cpp shared/crypto.cpp`).

`luabridge`, the library I use to bind C++ stuff to Lua, allows only one constructor to be bound (find out which with `grep -FB 1 addConstructor engine/server.cpp fpsgame/server.cpp shared/crypto.cpp`).

The `static const` parameters in *fpsgame/{ctf,capture,collect}.h* are now modifiable, as well as some `const` arrays in *fpsgame/game.h* (`itemstat`, `guninfo`).

Not all fields of `ENetHost` and `ENetPeer` are exported. As a rule of thumb, those that are clearly meant for internal usage by enet (for example the lists of packet fragments) will be unavailable.

##The scripting environment

The default boostrap code adds *script/* to the `LUA_PATH`, to ease `require`.

*utils* contain some functional programming utilities, `ip` and `ipset` object that are already documented in [kidban](https://github.com/pisto/kidban/tree/master/maintaining-docs#modules), and other generic helpers.

*std* contains the standard modules. I am too lazy to write a documentation for these before someone actually shows interest in using my code. Feel free to contact me if you want to write your own modules.

