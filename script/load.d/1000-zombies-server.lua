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
cs.adduser("Fear", "pisto", "+6c9ab9b9815cd392f83edb0f8c6c1dd35e4e262ff2190a7f", "m")
cs.adduser("Frosty", "pisto", "+bebaea64312c9c9365b0d54f8d013b546811b0da44284d33", "m")
cs.adduser("llama", "pisto", "+2d04377064720d72467ec71c165d185fc776bb5b437e71e4", "m")
cs.adduser("Dino_Martino", "pisto", "+8e786719673d030939d873ca0258913c69379581666d6cb7", "m")
require("std.preauth").on("pisto")

local nameprotect = require"std.nameprotect"
local protectdb = nameprotect.on(true)
protectdb["^pisto$"] = { pisto = { pisto = true } }

cs.serverdesc = "\f7 ZOMBIE OUTBREAK!"

cs.lockmaprotation = 2
cs.maprotationreset()

local zombiemaps = map.f(I, ("aard3c abbey academy akaritori akroseum alithia alloy aqueducts arabic asgard asteroids authentic autumn bad_moon berlin_wall bt_falls bvdm_01 campo capture_night caribbean castle_trap catch22 c_egypt collusion complex core_refuge core_transfer corruption curvedm curvy_castle c_valley cwcastle damnation darkdeath deathtek depot desecration dirtndust DM_BS1 dock donya douze duel7 duel8 dune duomo dust2 elegy eternal_valley europium evilness face-capture fanatic_quake fb_capture fc3 fc4 fc5 flagstone force forge forgotten fragplaza frostbyte frozen fury garden guacamole gubo hades hallo hashi haste hidden hog2 industry infamy injustice island justice kalking1 katrez_d kffa killcore3 killfactory kmap5 konkuri-to kopenhagen ksauer1 l_ctf legazzo lostinspace mach2 masdm mbt10 mbt12 mbt1 mbt2 mbt4 mbt9 memento mercury metl2 metl3 metl4 mill monastery moonlite neondevastation neonpanic nevil_c nitro nmp4 nmp8 nmp9 nucleus oasis ogrosupply orbe orion osiris ot outpost paradigm park pgdm ph-capture phosgene pitch_black powerplant recovery redemption refuge reissen relic renegade river_c rm5 roughinery ruby ruine sacrifice sauerstruck sdm1 serenity shadowed shindou shinmei1 shipwreck shiva siberia simplicity skrdm1 snapper_rocks spcr stemple subterra suburb tartech teahupoo tejen tempest thetowers thor torment tortuga tumwalk turbine turbulence twinforts urban_c valhalla venice wake5 wdcd xenon"):gmatch("[^ ]+"))
for i = 2, #zombiemaps do
  local j = math.random(i)
  local s = zombiemaps[j]
  zombiemaps[j] = zombiemaps[i]
  zombiemaps[i] = s
end

cs.maprotation("efficteam", table.concat(zombiemaps, " "))
cs.publicserver = 1
spaghetti.addhook(server.N_MAPVOTE, function(info)
  if info.skip or info.ci.privilege > 0 or info.text ~= server.smapname then return end
  info.skip = true
  playermsg("Cannot revote the current map.", info.ci)
end)

local ents = require"std.ents"
spaghetti.addhook("entsloaded", function()
  if server.smapname == "core_refuge" then
    ents.newent(server.MAPMODEL, {x = 495, y = 910, z = 509}, 60, 62)
    ents.newent(server.MAPMODEL, {x = 400, y = 910, z = 511}, 60, 62)
  elseif server.smapname == "fb_capture" then
    ents.newent(server.MAPMODEL, {x = 986, y = 572.5, z = 182}, 266, 1)
  end
end)

--gamemods

local function ammo(ci)
  local st = ci.state
  for i = 0, server.NUMGUNS - 1 do st.ammo[i] = 0 end
  st.ammo[server.GUN_FIST], st.armourtype, st.armour = 1, server.A_BLUE, 0
  if ci.team == "good" then st.ammo[server.GUN_CG], st.gunselect, st.health, st.maxhealth = 9999, server.GUN_CG, 200, 200
  else st.ammo[server.GUN_RL], st.ammo[server.GUN_GL], st.gunselect, st.health, st.maxhealth = 9999, 9999, st.aitype == server.AI_BOT and server.GUN_FIST or server.GUN_RL, 90, 0 end
end
local banner = "\f3ZOMBIE OUTBREAK IN 10 SECONDS\f7! Take cover!\n\f7Kill them with \f6CHAINSAW \f7for \f050 HEALTH POINTS\f7! Zombie intestines are yummy and healthy."
require"gamemods.zombieoutbreak".on({ammo = ammo, speed = 30, spawninterval = 10000/100*30, banner = banner, healthdrops = 2, burnhealth = true}, true)

require"std.antispawnkill".on(server.guns[server.GUN_FIST].range * 3)

require"std.pm"

--Merry Christmas!

local ents = require"std.ents"
local moveareaents = map.sv(I, server.PLAYERSTART, server.I_BULLETS, server.I_ROCKETS, server.I_ROUNDS, server.I_GRENADES, server.I_CARTIDGES, server.I_HEALTH, server.I_BOOST, server.I_GREENARMOUR, server.I_YELLOWARMOUR, server.I_QUAD, server.TELEPORT, server.TELEDEST, server.JUMPPAD, server.BASE, server.FLAG)
spaghetti.addhook("entsloaded", function()
  local xmin, xmax, ymin, ymax, zmax = 1/0, -1/0, 1/0, -1/0, -1/0
  for _, _, ment in ents.enum() do
    if moveareaents[ment.type] then
      local x, y, z = ment.o.x, ment.o.y, ment.o.z
      xmin, xmax, ymin, ymax, zmax = math.min(xmin, x), math.max(xmax, x), math.min(ymin, y), math.max(ymax, y), math.max(zmax, z)
    end
  end
  if xmin == 1/0 then return end
  local xavg, yavg, radius, z = (xmax + xmin) / 2, (ymax + ymin) / 2, math.max(xmax - xmin, ymax - ymin)/2 + 40, zmax + 60
  for i = 1, 5 do ents.newent(server.PARTICLES, {x = xavg, y = yavg, z = z}, 13, 280, radius, 0xFFF, 5000) end
end)

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

--#cheater command
local home = os.getenv("HOME") or "."
local function ircnotify(args)
  --I use ii for the bots
  local cheaterchan, pisto = io.open(home .. "/irc/cheaterchan/in", "w"), io.open(home .. "/irc/ii/pipes/pisto/in", "w")
  for ip, requests in pairs(args) do
    local str = "#cheater" .. (requests.ai and " \x02through bots\x02" or "") .. " on pisto.horse 6666"
    if requests.total > 1 then str = str .. " (" .. requests.total .. " reports)" end
    str = str .. ": "
    local names
    for cheater in pairs(requests.cheaters) do str, names = str .. (names and ", \x02" or "\x02") .. cheater.name .. " (" .. cheater.clientnum .. ")\x02", true end
    if not names then str = str .. "<disconnected>" end
    if cheaterchan then cheaterchan:write(str .. ", auth holders please help!\n") end
    if pisto then pisto:write(str .. " -- " .. tostring(require"utils.ip".ip(ip)) .. "\n") end
  end
  if cheaterchan then cheaterchan:close() end
  if pisto then pisto:close() end
end

abuse.cheatercmd(ircnotify, 20000, 1/30000, 3)
spaghetti.addhook(server.N_TEXT, function(info)
  if info.skip then return end
  local low = info.text:lower()
  if not low:match"cheat" and not low:match"hack" and not low:match"auth" and not low:match"kick" then return end
  local tellcheatcmd = info.ci.extra.tellcheatcmd or tb(1/30000, 1)
  info.ci.extra.tellcheatcmd = tellcheatcmd
  if not tellcheatcmd() then return end
  playermsg("\f2Problems with a cheater? Please use \f3#cheater [cn|name]\f2, and operators will look into the situation!\nYou can report zombies too, the controlling client will be reported.", info.ci)
end)

--simple banner

local git = io.popen("echo `git rev-parse --short HEAD` `git show -s --format=%ci`")
local gitversion = git:read()
git = nil, git:close()
commands.add("info", function(info)
  playermsg("spaghettimod is a reboot of hopmod for programmers. Will be used for SDoS.\nKindly brought to you by pisto." .. (gitversion and "\nCommit " .. gitversion or ""), info.ci)
end)

spaghetti.later(60000, function() server.sendservmsg("\nLiking this modded server? Check out also \f2/connect pisto.horse 1024\f7 for \f6QUAD ARMOURS\f7, \f1FLAG SWITCH\f7 mode, \f3RUGBY\f7 mode!\nSuggestions? \f0www.pisto.horse/spaghetti") end, true)
