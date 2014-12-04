--[[

  A server which only runs zombie outbreak mode.

]]--

if not os.getenv("ZOMBIEVPS") then return end
engine.writelog("Applying the zombie configuration.")

local servertag = require"utils.servertag"
servertag.tag = "zombie"

local uuid = require"std.uuid"

local fp, lambda = require"utils.fp", require"utils.lambda"
local map, range, fold, last, I, L, Lr = fp.map, fp.range, fp.fold, fp.last, fp.I, lambda.L, lambda.Lr
local abuse, playermsg, commands = require"std.abuse", require"std.playermsg", require"std.commands"

cs.maxclients = 42
cs.serverport = 6666

--make sure you delete the next two lines, or I'll have admin on your server.
cs.serverauth = "pisto"
cs.adduser("pisto", "pisto", "+8ce1687301aea5c4500df0042849191f875c70555c3cc4c9", "a")
require("std.preauth").on("pisto")

local nameprotect = require"std.nameprotect"
local protectdb = nameprotect.on(true)
protectdb["^pisto$"] = { pisto = { pisto = true } }

cs.serverdesc = "\f7 ZOMBIE OUTBREAK!"

cs.lockmaprotation = 2
cs.maprotationreset()

local zombiemaps = map.f(I, ("aard3c abbey academy akaritori akroseum alithia alloy aqueducts arabic asgard asteroids authentic autumn bad_moon berlin_wall bt_falls bvdm_01 campo capture_night caribbean castle_trap catch22 c_egypt collusion complex core_refuge core_transfer corruption curvedm curvy_castle c_valley cwcastle damnation darkdeath deathtek depot desecration dirtndust DM_BS1 dock donya douze duel7 duel8 dune duomo dust2 elegy eternal_valley europium evilness face-capture fanatic_quake fb_capture fc3 fc4 fc5 flagstone force forge forgotten fragplaza frostbyte frozen fury garden guacamole gubo hades hallo hashi haste hidden hog2 industry infamy injustice island justice kalking1 katrez_d kffa killcore3 killfactory kmap5 konkuri-to kopenhagen ksauer1 l_ctf legazzo lostinspace mach2 masdm mbt10 mbt12 mbt1 mbt2 mbt4 mbt9 memento mercury metl2 metl3 metl4 mill monastery moonlite neondevastation neonpanic nevil_c nitro nmp4 nmp8 nmp9 nucleus oasis oddworld ogrosupply orbe orion osiris ot outpost paradigm park pgdm ph-capture phosgene pitch_black powerplant recovery redemption refuge reissen relic renegade river_c rm5 roughinery ruby ruine sacrifice sauerstruck sdm1 serenity shadowed shindou shinmei1 shipwreck shiva siberia simplicity skrdm1 snapper_rocks spcr stemple subterra suburb tartech teahupoo tejen tempest thetowers thor torment tortuga tumwalk turbine turbulence twinforts urban_c valhalla venice wake5 wdcd xenon"):gmatch("[^ ]+"))
map.nf(function(i)
  local j = math.random(i)
  local s = zombiemaps[j]
  zombiemaps[j] = zombiemaps[i]
  zombiemaps[i] = s
end, range.z(2, #zombiemaps))

cs.maprotation("efficteam", table.concat(zombiemaps, " "))
cs.publicserver = 1
spaghetti.addhook(server.N_MAPVOTE, function(info)
  if info.skip or info.ci.privilege > 0 or info.text ~= server.smapname then return end
  info.skip = true
  playermsg("Cannot revote the current map.", info.ci)
end)

require("std.flushinterval").set(5)

--gamemods

local function ammo(ci)
  local st = ci.state
  for i = 0, server.NUMGUNS - 1 do st.ammo[i] = 0 end
  st.ammo[server.GUN_FIST], st.armourtype, st.armour = 1, server.A_BLUE, 0
  if ci.team == "good" then st.ammo[server.GUN_CG], st.gunselect, st.health = 9999, server.GUN_CG, 200
  else st.ammo[server.GUN_RL], st.ammo[server.GUN_GL], st.gunselect, st.health = 9999, 9999, server.GUN_RL, 90 end
end
require"gamemods.zombieoutbreak".on(ammo, 30, 10000/100*30, true)

--moderation

require"std.ban".kickpersist(servertag.fntag .. "bans")

--limit reconnects when banned, or to avoid spawn wait time
abuse.reconnectspam(1/60, 5)

--limit some message types
spaghetti.addhook(server.N_KICK, function(info)
  if info.skip or info.ci.privilege > server.PRIV_MASTER then return end
  info.skip = true
  playermsg("No. Use gauth.", info.ci)
end)
spaghetti.addhook(server.N_SOUND, function(info)
  if info.skip or abuse.clientsound(info.sound) then return end
  info.skip = true
  playermsg("I know I used to do that but... whatever.", info.ci)
end)
abuse.ratelimit({ server.N_TEXT, server.N_SAYTEAM }, 0.5, 10, Lr"nil, 'I don\\'t like spam.'")
abuse.ratelimit(server.N_SWITCHNAME, 1/30, 4, Lr"nil, 'You\\'re a pain.'")
abuse.ratelimit(server.N_MAPVOTE, 1/10, 3, Lr"nil, 'That map sucks anyway.'")
abuse.ratelimit(server.N_SPECTATOR, 1/30, 5, Lr"_.ci.clientnum ~= _.spectator, 'Can\\'t even describe you.'") --self spec
abuse.ratelimit(server.N_MASTERMODE, 1/30, 5, Lr"_.ci.privilege == server.PRIV_NONE, 'Can\\'t even describe you.'")
abuse.ratelimit({ server.N_AUTHTRY, server.N_AUTHKICK }, 1/60, 4, Lr"nil, 'Are you really trying to bruteforce a 192 bits number? Kudos to you!'")
abuse.ratelimit(server.N_CLIENTPING, 4.5) --no message as it could be cause of network jitter
abuse.ratelimit(server.N_SERVCMD, 0.5, 10, Lr"nil, 'Yes I\\'m filtering this too.'")

--prevent masters from annoying players
local tb = require"utils.tokenbucket"
local function bullying(who, victim)
  local t = who.extra.bullying or {}
  local rate = t[victim.extra.uuid] or tb(1/30, 6)
  t[victim.extra.uuid] = rate
  who.extra.bullying = t
  return not rate()
end
spaghetti.addhook(server.N_SETTEAM, function(info)
  if info.skip or info.who == info.sender or not info.wi or info.ci.privilege == server.PRIV_NONE then return end
  local team = engine.filtertext(info.text):sub(1, engine.MAXTEAMLEN)
  if #team == 0 or team == info.wi.team then return end
  if bullying(info.ci, info.wi) then
    info.skip = true
    playermsg("...", info.ci)
  end
end)
spaghetti.addhook(server.N_SPECTATOR, function(info)
  if info.skip or info.spectator == info.sender or not info.spinfo or info.ci.privilege == server.PRIV_NONE or info.val == (info.spinfo.state.state == engine.CS_SPECTATOR and 1 or 0) then return end
  if bullying(info.ci, info.spinfo) then
    info.skip = true
    playermsg("...", info.ci)
  end
end)

--ratelimit just gobbles the packet. Use the selector to add a tag to the exceeding message, and append another hook to send the message
local function warnspam(packet)
  if not packet.ratelimited or type(packet.ratelimited) ~= "string" then return end
  playermsg(packet.ratelimited, packet.ci)
end
map.nv(function(type) spaghetti.addhook(type, warnspam) end,
  server.N_TEXT, server.N_SAYTEAM, server.N_SWITCHNAME, server.N_MAPVOTE, server.N_SPECTATOR, server.N_MASTERMODE, server.N_AUTHTRY, server.N_AUTHKICK, server.N_CLIENTPING
)

--simple banner

local git = io.popen("echo `git rev-parse --short HEAD` `git show -s --format=%ci`")
local gitversion = git:read()
git = nil, git:close()
commands.add("info", function(info)
  playermsg("spaghettimod is a reboot of hopmod for programmers. Will be used for SDoS.\nKindly brought to you by pisto." .. (gitversion and "\nCommit " .. gitversion or ""), info.ci)
end)

spaghetti.later(60000, function() server.sendservmsg("\nLiking this modded server? Check out also \f2/connect pisto.horse 1024\f7 for \f6QUAD ARMOURS\f7, \f1FLAG SWITCH\f7 mode, \f3RUGBY\f7 mode!") end, true)
