--[[

  This is the configuration I use on my server.

]]--

if not os.getenv("PISTOVPS") then return end
engine.writelog("Applying the sample configuration.")

local servertag = require"utils.servertag"
servertag.tag = "pisto"

local uuid = require"std.uuid"

local fp, L = require"utils.fp", require"utils.lambda"
local map, I = fp.map, fp.I
local abuse, playermsg = require"std.abuse", require"std.playermsg"

cs.maxclients = 42
cs.serverport = 1024

--make sure you delete the next two lines, or I'll have admin on your server.
cs.serverauth = "pisto"
local auth = require"std.auth"
cs.adduser("pisto", "pisto", "+515027a91c3de5eecb8d0e0267f46d6bbb0b4bd87c4faae0", "a")
cs.adduser("Fear", "pisto", "+6c9ab9b9815cd392f83edb0f8c6c1dd35e4e262ff2190a7f", "m")
cs.adduser("Frosty", "pisto", "+bebaea64312c9c9365b0d54f8d013b546811b0da44284d33", "m")
cs.adduser("llama", "pisto", "+2d04377064720d72467ec71c165d185fc776bb5b437e71e4", "m")
cs.adduser("Dino_Martino", "pisto", "+8e786719673d030939d873ca0258913c69379581666d6cb7", "m")
cs.adduser("Buck", "pisto", "+5028b0663cf878a8b14d57f97ec06295b7a87676f881b6bc", "m")
cs.adduser("Pink", "pisto", "+48188ad779be16a77820ecba07f198b8d5898b9b9932b0a9", "m")
cs.adduser("Zaharia", "pisto", "-402f911db546976370f9f971477dd3d0563a6d6125b685bf", "m")
cs.adduser("S4US3SCHR1TT", "pisto", "-976346001fbbbba812d28d8214bede17e71bd35c8aec68e2", "m")
cs.adduser("quico", "pisto", "-d4c7af1291e4b22d1c93639861e074af43de814495e9d69c", "m")
cs.adduser("LordBug", "pisto", "-2ea3c4511769fabceee754e7a22b71b7e02b31a25227f2bf", "m")
cs.adduser("ne0n", "pisto", "+c799c10fc19b95d230bb91430f4dfd92234896c1bcfa29f6", "m")
cs.adduser("hades", "pisto", "+2651a29f0060d48c37534a2509698c798c21fafd454fd9b8", "m")
cs.adduser("firefly", "pisto", "-88a2394cbb748897bb28ba2b37c949b29019ad12320c55bb", "m")
cs.adduser("Charlotte", "pisto", "-4b7f5b00186b068aba0f85942a154ef7389fba048de88832", "m")
cs.adduser("Jay", "pisto", "-2468d805d2fce755fb92114caa537e8787573300e0c049bd", "m")
cs.adduser("Galaxy", "pisto", "+1e9244443a171573839c19bb96fb59d2532e9f17a2dd10e0", "m")
cs.adduser("px", "pisto", "+bbec32b37c334bf798c9e79cdf8426ef33f3bdf83778ab52", "m")
cs.adduser("Mr.Benz", "pisto", "+21485c750d9a39d9373576312418ba63928333756538e506", "m")
cs.adduser("a-monster", "pisto", "+15cb72e43e9ca29981636edf9e771b53c878a36a07244708", "m")
cs.adduser("deathstar", "pisto", "+d7900617ee9d447a74692ff114384f8d2f2b8e8582fc7af0", "m")
cs.adduser("swatllama", "pisto", "+e544f11d6424497013bacf99f01a3555d311954efbd111fe", "m")
cs.adduser("noobie", "pisto", "-a590c205846c50b07ecfba22d3bc3e7fe6ad6bef554c73da", "m")
cs.adduser("GustavoLapasta", "pisto", "-d9bf1ea15b96042ebada9c2dd14b928ea06642eaae318410", "m")
cs.adduser("Cedii**", "ASkidban-bypass", "-4e75e0e92e6512415a8114e1db856af36d00e801615a3e98", "n")
cs.adduser("xcb567", "ASkidban-bypass", "+41b02bfb90f87d403a864e722d2131a5c7941f2b35491d0f", "n")
cs.adduser("M0UL", "ASkidban-bypass", "+640728e15ab552342b68a293f2c6b3e15b5adf1be53fd4f2", "n")
cs.adduser("/dev/zero", "ASkidban-bypass", "-fbd614eb7d65a9422f40f329d093dc7ecf30c7d1e0130618", "n")
cs.adduser("Cedi", "ASkidban-bypass", "-1f631f3d7940f4ca651a0941f694ac9db8dfaf082bcaed8d", "n")
cs.adduser("Star", "ASkidban-bypass", "-c5f79ab9249cc896eb735464e91aa1ec8c4c08ddeed156ee", "n")
cs.adduser("lopi2", "ASkidban-bypass", "-e4ce067444a51914ce289b656154f172b367a64bb04f854a", "n")
cs.adduser("Shag", "ASkidban-bypass", "+0b7e1cdadb84c545b4bb181c3f3744dd62fcc2d60eba3ecd", "n")
table.insert(auth.preauths, "pisto")

spaghetti.addhook(server.N_SETMASTER, L"_.skip = _.skip or (_.mn ~= _.ci.clientnum and _.ci.privilege < server.PRIV_ADMIN)")

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
server.mastermask = server.MM_PUBSERV + server.MM_AUTOAPPROVE

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
require"std.getip"

spaghetti.addhook("entsloaded", function()
  if server.smapname ~= "thetowers" then return end
  for i, _, ment in ents.enum(server.JUMPPAD) do if ment.attr4 == 40 then
    ents.editent(i, server.JUMPPAD, ment.o, ment.attr1, ment.attr2, ment.attr3)
    break
  end end
end)

--moderation

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
abuse.ratelimit({ server.N_TEXT, server.N_SAYTEAM }, 0.5, 10, L"nil, 'I don\\'t like spam.'")
abuse.ratelimit(server.N_SWITCHNAME, 1/30, 4, L"nil, 'You\\'re a pain.'")
abuse.ratelimit(server.N_MAPVOTE, 1/10, 3, L"nil, 'That map sucks anyway.'")
abuse.ratelimit(server.N_SPECTATOR, 1/30, 5, L"_.ci.clientnum ~= _.spectator, 'Can\\'t even describe you.'") --self spec
abuse.ratelimit(server.N_MASTERMODE, 1/30, 5, L"_.ci.privilege == server.PRIV_NONE, 'Can\\'t even describe you.'")
abuse.ratelimit({ server.N_AUTHTRY, server.N_AUTHKICK }, 1/60, 4, L"nil, 'Are you really trying to bruteforce a 192 bits number? Kudos to you!'")
abuse.ratelimit(server.N_CLIENTPING, 4.5) --no message as it could be cause of network jitter
abuse.ratelimit(server.N_SERVCMD, 0.5, 10, L"nil, 'Yes I\\'m filtering this too.'")
abuse.ratelimit(server.N_JUMPPAD, 1, 10, L"nil, 'I know I used to do that but... whatever.'")
abuse.ratelimit(server.N_TELEPORT, 1, 10, L"nil, 'I know I used to do that but... whatever.'")

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
  if cheaterchan then cheaterchan:write('\n'):close() end
  if pisto then pisto:write('\n'):close() end
end

abuse.cheatercmd(ircnotify, 20000, 1/30000, 3)
local sound = require"std.sound"
spaghetti.addhook(server.N_TEXT, function(info)
  if info.skip then return end
  local low = info.text:lower()
  if not low:match"cheat" and not low:match"hack" and not low:match"auth" and not low:match"kick" then return end
  local tellcheatcmd = info.ci.extra.tellcheatcmd or tb(1/30000, 1)
  info.ci.extra.tellcheatcmd = tellcheatcmd
  if not tellcheatcmd() then return end
  playermsg("\f2Problems with a cheater? Please use \f3#cheater [cn|name]\f2, and operators will look into the situation!", info.ci)
  sound(info.ci, server.S_HIT, true) sound(info.ci, server.S_HIT, true)
end)

require"std.enetping"

local parsepacket = require"std.parsepacket"
spaghetti.addhook("martian", function(info)
  if info.skip or info.type ~= server.N_TEXT or info.ci.connected or parsepacket(info) then return end
  local text = engine.filtertext(info.text, true, true)
  engine.writelog(("limbotext: (%d) %s"):format(info.ci.clientnum, text))
  info.skip = true
end, true)

--simple banner

local git = io.popen("echo `git rev-parse --short HEAD` `git show -s --format=%ci`")
local gitversion = git:read()
git = nil, git:close()
commands.add("info", function(info)
  playermsg("spaghettimod is a reboot of hopmod for programmers. Will be used for SDoS.\nKindly brought to you by pisto." .. (gitversion and "\nCommit " .. gitversion or ""), info.ci)
end)

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

--Merry Christmas!

spaghetti.later(20000, function()
  return (server.m_ctf and server.m_insta) and server.sendservmsg("\nRemember, it's rugby mode: you \f6shoot a teammate\f7 to \f6pass the flag\f7!")
end, true)
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
