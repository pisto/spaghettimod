--[[

  This is the configuration I use on my server.

]]--

if not os.getenv("MAPSVPS") then return end
engine.writelog("Applying the new maps configuration.")

local servertag = require"utils.servertag"
servertag.tag = "maps"

local uuid = require"std.uuid"

local fp, L = require"utils.fp", require"utils.lambda"
local map, range, fold, last, I = fp.map, fp.range, fp.fold, fp.last, fp.I
local abuse, playermsg = require"std.abuse", require"std.playermsg"

cs.maxclients = 42
cs.serverport = 1111

--make sure you delete the next two lines, or I'll have admin on your server.
cs.serverauth = "pisto"
local auth = require"std.auth"
cs.adduser("pisto", "pisto", "+8ce1687301aea5c4500df0042849191f875c70555c3cc4c9", "a")
cs.adduser("Fear", "pisto", "+6c9ab9b9815cd392f83edb0f8c6c1dd35e4e262ff2190a7f", "m")
cs.adduser("Frosty", "pisto", "+bebaea64312c9c9365b0d54f8d013b546811b0da44284d33", "m")
cs.adduser("llama", "pisto", "+2d04377064720d72467ec71c165d185fc776bb5b437e71e4", "m")
cs.adduser("Cedii**", "ASkidban-bypass", "-4e75e0e92e6512415a8114e1db856af36d00e801615a3e98", "n")
cs.adduser("xcb567", "ASkidban-bypass", "+41b02bfb90f87d403a864e722d2131a5c7941f2b35491d0f", "n")
cs.adduser("M0UL", "ASkidban-bypass", "+640728e15ab552342b68a293f2c6b3e15b5adf1be53fd4f2", "n")
cs.adduser("/dev/zero", "ASkidban-bypass", "-fbd614eb7d65a9422f40f329d093dc7ecf30c7d1e0130618", "n")
table.insert(auth.preauths, "pisto")

spaghetti.addhook(server.N_SETMASTER, L"_.skip = _.skip or (_.mn ~= _.ci.clientnum and _.ci.privilege < server.PRIV_ADMIN)")

local nameprotect = require"std.nameprotect"
local protectdb = nameprotect.on(true)
protectdb["^pisto$"] = { pisto = { pisto = true } }

cs.serverdesc = "\f7reissen? \f3FUCK YOU\f7!"

cs.lockmaprotation = 2
cs.maprotationreset()

local rcs = require"std.rcs"

local monomaps = "aerowalk bklyn croma hektik legacy mc-lab pandora sandstorm2 stahlbox star origin2 memento2b binary scedm5"
local ctfmaps = "bklyn croma mc-lab pandora"
local capturemaps = "aerowalk croma mc-lab pandora sandstorm2 stahlbox memento2b scedm5"
monomaps, capturemaps, ctfmaps = map.uv(function(maps)
  local t = map.f(I, maps:gmatch("[^ ]+"))
  for i = 2, #t do
    local j = math.random(i)
    local s = t[j]
    t[j] = t[i]
    t[i] = s
  end
  return t
end, monomaps, capturemaps, ctfmaps)
cs.maprotation("ffa insta effic tac", table.concat(monomaps, " "), "regencapture capture hold effichold instahold", table.concat(capturemaps, " "), "ctf efficctf instactf protect efficprotect instaprotect", table.concat(ctfmaps, " "))

local needscfg = L"_2, { needcfg = not not io.open('packages/base/' .. _2 .. '.cfg') }"
local maps = map.im(needscfg, table.sort(monomaps))
map.tim(maps, needscfg, table.sort(ctfmaps))
map.tim(maps, needscfg, table.sort(capturemaps))

local maplist_gui = ([[
newgui reissenfu_maplist [
    guilist [
      guilist [ guistrut 0 0; genmapitems "%s" ]
    ]
    guitab capture
    guilist [
      guilist [ guistrut 0 0; genmapitems "%s" ]
    ]
    guitab ctf
    guilist [
      guilist [ guistrut 0 0; genmapitems "%s" ]
    ]
] "^f7reissen^f3FU^f7! map list"
showgui reissenfu_maplist
]]):format(table.concat(monomaps, " "), table.concat(capturemaps, " "), table.concat(ctfmaps, " "))

local ents, putf, n_client = require"std.ents", require"std.putf", require"std.n_client"
local function quirk_replacemodels(replacements)
  replacements = replacements or {}
  return function(ci)
    if not ents.active() then return end
    local p
    for i, _, ment in ents.enum(server.MAPMODEL) do
      if replacements[ment.attr2] then p = putf(p or {15, r=1}, server.N_EDITENT, i, ment.o.x * server.DMF, ment.o.y * server.DMF, ment.o.z * server.DMF, server.MAPMODEL, ment.attr1, replacements[ment.attr2], 0, 0, 0)
      else p = putf(p or {15, r=1}, server.N_EDITENT, i, 0, 0, 0, server.NOTUSED, 0, 0, 0, 0, 0) end
    end
    return p and engine.sendpacket(ci.clientnum, 1, n_client(p, ci):finalize(), -1)
  end
end
local function quirk_extrarcs(cs)
  return function(ci) return ci.extra.rcs and rcs.send(ci, cs) end
end
local function quirk_multi(quirks)
  return function(ci) for _, f in ipairs(quirks) do f(ci) end end
end

maps.bklyn.nocfgquirk = quirk_replacemodels()
maps["mc-lab"].nocfgquirk = quirk_extrarcs[[
alias base_1 "The Control Stage ^f5(Riflerounds)"
alias base_2 "The Storage Platform ^f6(Rockets)"
alias base_3 "The Cloning Room 1 ^f3(Shells)"
alias base_4 "The Cloning Room 2 ^f0(Bullets)"
alias base_5 "The Bridge ^f0(Bullets)"
alias base_6 "The Pool ^f9(Grenades)"
]]
maps.memento2b.nocfgquirk = quirk_multi{ quirk_replacemodels({[0] = 93}), quirk_extrarcs[[
alias base_1 "the ^f2Rifle Rounds^f~"
alias base_2 "the ^f2Shotgun Shells^f~"
alias base_3 "the ^f2Grenades^f~"
]] }
maps.pandora.nocfgquirk = quirk_extrarcs[[
alias base_1 "The Bridge ^f2(Rockets)^f~"
alias base_2 "The Power Cells ^f5(Rifle)^f~"
alias base_3 "Test Room 1 ^f0(Chaingun)^f~"
alias base_4 "The Cell Reserve ^f9(Grenades)^f~"
alias base_5 "Break Room Entrance ^f3(Shotgun)^f~"
alias base_6 "The Observation Stage ^f5(Rifle)^f~"
]]
maps.sandstorm2.nocfgquirk = quirk_replacemodels({ false, false, false, false, 115, 116, 114, 118, 151})
maps.scedm5.nocfgquirk = quirk_multi{ quirk_replacemodels({ [0] = 93, 34, 31, 29, false, 138, 38, 7, 14, 18, 5, 37, 41, 99, 93, 94 }), quirk_extrarcs[[
alias base_1 "the crate storage"
alias base_2 "the centre hall"
alias base_3 "the pool"
alias base_4 "the edge"
alias base_5 "the sewer"
]] }
maps.stahlbox.nocfgquirk = quirk_replacemodels({[0] = 114, 169})

server.mastermask = server.MM_PUBSERV + server.MM_AUTOAPPROVE

require"std.pm"

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
    local str = "#cheater" .. (requests.ai and " \x02through bots\x02" or "") .. " on pisto.horse 1111"
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

local commands = require"std.commands"

local git = io.popen("echo `git rev-parse --short HEAD` `git show -s --format=%ci`")
local gitversion = git:read()
git = nil, git:close()
commands.add("info", function(info)
  playermsg("spaghettimod is a reboot of hopmod for programmers. Will be used for SDoS.\nKindly brought to you by pisto." .. (gitversion and "\nCommit " .. gitversion or ""), info.ci)
end)

local fence, sendmap = require"std.fence", require"std.sendmap", require"std.maploaded"

banner = "\nWant to play reissen? \f3FUCK YOU\f7! Use \f0#maplist\f7 to display the list of \f0new original maps\f7 that this server runs.\nThis server uses \f1Remote CubeScript\f7 to \f0auto-send \f3new \f0maps\f7 to you. Type \f0#rcs\f7 for more info."
spaghetti.addhook("maploaded", function(info) info.ci.extra.mapcrcfence = fence(info.ci) end)
spaghetti.later(60000, L"server.sendservmsg(banner)", true)

local function rcsspam(ci, msg)
  if ci.extra.rcsspam then spaghetti.cancel(ci.extra.rcsspam) end
  if not msg then ci.extra.rcsspam = nil return end
  local sender
  sender = function()
    playermsg(ci.extra.firstspam and msg or "\n\n\f0New maps only\f7 server! This server uses \f1Remote CubeScript\f7 to \f0auto-send \f0maps\f7 maps\f7 to you.\nNever heard of \f1Remote CubeScript\f7? Type \f0#rcs\f7 for more info.", ci)
    ci.extra.firstspam = true
    ci.extra.rcsspam = spaghetti.later(15000, sender)
  end
  ci.extra.rcsspam = spaghetti.later(3000, sender)
end
spaghetti.addhook("clientdisconnect", L"_.ci.extra.rcsspam and spaghetti.cancel(_.ci.extra.rcsspam)")

commands.add("rcs", function(info) playermsg(
"\f1Remote CubeScript\f7 (\f1rcs\f7) allows the server to run cubescript code on your client (like the \f2crapmod.net\f7 master server), and so download new maps. " ..
"\f1rcs\f7 requires a one-time installation with these commands: \f0/mastername pisto.horse; updatefrommaster\f7\n" ..
"For detailed information visit \f0pisto.horse/rcs\f7 . You can uninstall \f1rcs\f7 any time by typing \f0/rcs_uninstall"
, info.ci) end)

commands.add("maplist", function(info)
  local msg = "\f0duel modes\f7: " .. table.concat(monomaps, ", ")
  if #ctfmaps > 0 then msg = msg .. "\n\f0ctf\f7/\f0collect\f7: " .. table.concat(ctfmaps, ", ") end
  if #capturemaps > 0 then msg = msg .. "\n\f0capture\f7: " .. table.concat(capturemaps, ", ") end
  playermsg(msg, info.ci)
  if not info.ci.extra.rcs then return end
  rcs.send(info.ci, maplist_gui)
end, "#maplist : show the list of maps that can be played and voted on this server.")

local function trysendmap(ci, force)
  if not maps[server.smapname] or server.m_edit or not sendmap.hasmap() then return end
  if not force and ci.mapcrc % 2^32 == server.mcrc then server.sendservmsg(server.colorname(ci, nil) .. " \f0has this map already\f7.") return end
  local extra = ci.extra
  if not server.m_teammode then
    engine.writelog("sending map to " .. server.colorname(ci, nil) .. " with coopedit" .. (extra.rcs and " and rcs" or ""))
    sendmap.forcecurrent(ci, true, true, true)
    rcsspam(ci, maps[server.smapname].needcfg and not extra.rcs and "\f6Crappy textures\f7? Install \f1Remote CubeScript\f7! Type \f0#rcs\f7 for more info.")
  elseif extra.rcs then
    engine.writelog("sending map to " .. server.colorname(ci, nil) .. " with savemap")
    sendmap.forcecurrent(ci, false, true, maps[server.smapname].cfgcopy)
  else rcsspam(ci, "\f6Got no map and cannot play\f7? Install \f1Remote CubeScript\f7! Type \f0#rcs\f7 for more info.") end
end

spaghetti.addhook("fence", function(info)
  local ci = info.ci
  local extra = ci.extra
  if extra.mapcrcfence ~= info.fence then return end
  rcsspam(ci)
  trysendmap(ci)
end)

spaghetti.addhook("rcshello", function(info)
  local ci = info.ci
  if not ci.extra.rcsspam then return end
  rcsspam(ci)
  playermsg("\f1Remote CubeScript\f7 detected! Maps will be sent automatically.", ci)
  trysendmap(ci, true)
end)

spaghetti.addhook(server.N_SPECTATOR, function(info)
  if info.skip or not info.spinfo or info.spinfo.clientnum ~= info.ci.clientnum and info.ci.privilege == server.PRIV_NONE then return end
  info.spinfo.extra.wantspec = info.val == 1
end)

spaghetti.addhook("sendmap", function(info)
  local ci = info.ci
  server.sendservmsg(server.colorname(ci, nil) .. " \f0downloaded the map\f7.")
  if not ci.extra.firstspam then
    local ciuuid = ci.extra.uuid
    spaghetti.later(1000, function()
      local ci = uuid.find(ciuuid)
      return ci and playermsg(banner, ci)
    end)
    ci.extra.firstspam = true
  end
  if (not info.rcs or info.method == "savemap") and maps[server.smapname].nocfgquirk then maps[server.smapname].nocfgquirk(ci) end
  if ci.extra.wantspec or ci.state.state ~= engine.CS_SPECTATOR or ci.privilege == server.PRIV_NONE and server.mastermode >= server.MM_LOCKED then return end
  server.unspectate(ci)
  server.sendspawn(ci)
end)
