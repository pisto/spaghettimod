--[[

  This is the configuration I use on my server.

]]--

if not os.getenv("PISTOVPS") then return end
engine.writelog("Applying the sample configuration.")

local servertag = require"utils.servertag"
servertag.tag = "pisto"

local uuid = require"std.uuid"

local fp, lambda = require"utils.fp", require"utils.lambda"
local map, range, fold, last, I, L, Lr = fp.map, fp.range, fp.fold, fp.last, fp.I, lambda.L, lambda.Lr
local abuse, playermsg = require"std.abuse", require"std.playermsg"

cs.maxclients = 42
cs.serverport = 1024

--make sure you delete the next two lines, or I'll have admin on your server.
cs.serverauth = "pisto"
cs.adduser("pisto", "pisto", "+8ce1687301aea5c4500df0042849191f875c70555c3cc4c9", "a")
cs.adduser("Fear", "pisto", "+6c9ab9b9815cd392f83edb0f8c6c1dd35e4e262ff2190a7f", "m")
cs.adduser("Frosty", "pisto", "+bebaea64312c9c9365b0d54f8d013b546811b0da44284d33", "m")
cs.adduser("llama", "pisto", "+2d04377064720d72467ec71c165d185fc776bb5b437e71e4", "m")
cs.adduser("Dino_Martino", "pisto", "+8e786719673d030939d873ca0258913c69379581666d6cb7", "m")
cs.adduser("Buck", "pisto", "+5028b0663cf878a8b14d57f97ec06295b7a87676f881b6bc", "m")
cs.adduser("Pink", "pisto", "+48188ad779be16a77820ecba07f198b8d5898b9b9932b0a9", "m")
require("std.preauth").on("pisto")

local nameprotect = require"std.nameprotect"
local protectdb = nameprotect.on(true)
protectdb["^pisto$"] = { pisto = { pisto = true } }
protectdb["[Dd]ino_?[Mm]artino"] = { pisto = { Dino_Martino = true } }

cs.serverdesc = "\f7pisto.horse 1024"

cs.lockmaprotation = 2
cs.maprotationreset()
--copied from data/menus.cfg
local ffamaps, capturemaps, ctfmaps = table.concat({
  "aard3c academy akaritori alithia alloy aqueducts arbana bvdm_01 castle_trap collusion complex corruption curvedm curvy_castle darkdeath deathtek depot",
  "dirtndust DM_BS1 dock douze duel7 duel8 dune elegy fanatic_quake force fragplaza frostbyte frozen fury guacamole gubo hades",
"hashi hog2 industry injustice island justice kalking1 katrez_d kffa killfactory kmap5 konkuri-to ksauer1 legazzo lostinspace masdm mbt10",
  "mbt2 mbt9 memento metl2 metl3 metl4 moonlite neondevastation neonpanic nmp8 nucleus oasis oddworld ogrosupply orbe orion osiris",
  "ot outpost paradigm park pgdm phosgene pitch_black powerplant refuge renegade rm5 roughinery ruby ruine sauerstruck sdm1 shadowed",
  "shindou shinmei1 shiva simplicity skrdm1 stemple suburb tartech teahupoo tejen thetowers thor torment tumwalk turbine wake5 wdcd"
}, " "), table.concat({
  "abbey akroseum alithia arabic asgard asteroids c_egypt c_valley campo capture_night caribbean collusion core_refuge core_transfer corruption cwcastle damnation",
  "dirtndust donya duomo dust2 eternal_valley evilness face-capture fb_capture fc3 fc4 fc5 forge frostbyte hades hallo haste hidden",
  "infamy killcore3 kopenhagen lostinspace mbt12 mercury monastery nevil_c nitro nmp4 nmp8 nmp9 nucleus ogrosupply paradigm ph-capture reissen",
  "relic river_c serenity snapper_rocks spcr subterra suburb tempest tortuga turbulence twinforts urban_c valhalla venice xenon"
}, " "), table.concat({
  "abbey akroseum arbana asgard authentic autumn bad_moon berlin_wall bt_falls campo capture_night catch22 core_refuge core_transfer damnation desecration dust2",
  "eternal_valley europium evilness face-capture flagstone forge forgotten garden hallo haste hidden infamy kopenhagen l_ctf mach2 mbt1 mbt12",
  "mbt4 mercury mill nitro nucleus recovery redemption reissen sacrifice shipwreck siberia snapper_rocks spcr subterra suburb tejen tempest",
  "tortuga turbulence twinforts urban_c valhalla wdcd xenon"
}, " ")

ffamaps, capturemaps, ctfmaps = map.uv(function(maps)
  local t = map.f(I, maps:gmatch("[^ ]+"))
  for i = 2, #t do
    local j = math.random(i)
    local s = t[j]
    t[j] = t[i]
    t[i] = s
  end
  return table.concat(t, " ")
end, ffamaps, capturemaps, ctfmaps)

cs.maprotation("ffa effic tac teamplay efficteam tacteam", ffamaps, "regencapture capture hold effichold instahold", capturemaps, "ctf efficctf instactf protect efficprotect instaprotect", ctfmaps)
cs.publicserver = 2

--gamemods
require"gamemods.quadarmour".on(1/2, 6, 0, 20000, 30000, server.A_GREEN, 100)
spaghetti.addhook("changemap", L'require"gamemods.rugby".on(server.m_ctf and server.m_insta)')

local commands = require"std.commands"
local nextflagswitch = false
commands.add("flagswitch", function(info)
  local arg = info.args == "" and 1 or tonumber(info.args)
  if not arg then playermsg("Invalid flagswitch value", info.ci) end
  local old = nextflagswitch
  nextflagswitch = arg == 1
  if old == nextflagswitch then return end
  if nextflagswitch and (not server.m_ctf or server.m_hold) then playermsg("Mind that you still need to force the next mode to be ctf/protect.", info.ci) end
  server.sendservmsg(server.colorname(info.ci, nil) .. (nextflagswitch and " activated" or " deactivated") .. " \f1flag switch mode\f7 for the next map (see #help flagswitch).")
end, "Usage: #flagswitch [0|1]: activate flag switch (blue flag spawns in place of red and viceversa) for the next map if mode is ctf or protect (default 1, only masters)")

local flagswitch, currentflagswitch = require"gamemods.flagswitch", false
spaghetti.addhook("entsloaded", function()
  currentflagswitch = false
  nextflagswitch = nextflagswitch and server.m_ctf and not server.m_hold
  if not nextflagswitch then flagswitch.on(false) return end
  nextflagswitch = false
  flagswitch.on(true)
  currentflagswitch = true
end)

local ents = require"std.ents"
spaghetti.addhook("maploaded", function(info)
  if not info.ci.extra.bannershown then return end
end)

require"std.pm"

--moderation

require"std.ban".kickpersist(servertag.fntag .. "bans")
cs.teamkillkick("*", 7, 30)

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
    local str = "#cheater" .. (requests.ai and " \x02through bots\x02" or "") .. " on pisto.horse 1024"
    if requests.total > 1 then str = str .. " (" .. requests.total .. " reports)" end
    str = str .. ": "
    local names
    for cheater in pairs(requests.cheaters) do str, names = str .. (names and ", \x02" or "\x02") .. engine.encodeutf8(cheater.name) .. " (" .. cheater.clientnum .. ")\x02", true end
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
  playermsg("\f2Problems with a cheater? Please use \f3#cheater [cn|name]\f2, and operators will look into the situation!", info.ci)
end)

--simple banner

local git = io.popen("echo `git rev-parse --short HEAD` `git show -s --format=%ci`")
local gitversion = git:read()
git = nil, git:close()
commands.add("info", function(info)
  playermsg("spaghettimod is a reboot of hopmod for programmers. Will be used for SDoS.\nKindly brought to you by pisto." .. (gitversion and "\nCommit " .. gitversion or ""), info.ci)
end)

local ents = require"std.ents"
local function gamemoddesc()
  local msg
  if ents.active() and currentflagswitch then msg = "\n\f1Flag switch mode activated\f7! " .. (server.m_protect and "Your flag spawns in the enemy base!" or "Bring the enemy flag back to the enemy base!") end
  if server.m_ctf and server.m_insta then msg = (msg or "") .. "\n\f3Rugby mode activated\f7! Shoot a teammate to pass the flag you are carrying" end
  return msg
end

banner = "\n\n\f7Welcome to pisto's horses playground. \f5This server runs continuously testing and experimental code.\n\f7Check out the \f6QUAD ARMOURS\f7! Replaces normal quad with 1/2 probability, gives undepletable armour.\nThis server has \f1FLAG SWITCH MODE\f7 (see #help flagswitch).\nictf/iprotect/ihold have \f3RUGBY MODE\f7: shoot a teammate to pass the flag you are carrying!"
spaghetti.addhook("maploaded", function(info)
  if info.ci.extra.bannershown then
    local moddesc = gamemoddesc()
    return moddesc and playermsg(moddesc, info.ci)
  end
  info.ci.extra.bannershown = true
  local ciuuid = info.ci.extra.uuid
  spaghetti.later(1000, function()
    local ci = uuid.find(ciuuid)
    if not ci then return end
    local moddesc = gamemoddesc()
    playermsg(banner .. (moddesc and moddesc or ""), ci)
  end)
end)

spaghetti.later(60000, function() server.sendservmsg("\n\nLiking this modded server? Check out also \f2/connect pisto.horse 6666\f7 for \f3ZOMBIE OUTBREAK\f7 mode!\nSuggestions? \f0www.pisto.horse/spaghetti") end, true)
