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

cs.serverdesc = "\f7new maps only."

cs.lockmaprotation = 2
cs.maprotationreset()

local maps = "aerowalk bklyn croma hektik legacy mc-lab pandora sandstorm2 stahlbox star origin2 memento2b binary scedm5"
local coopeditmaps = map.fs(L"_", maps:gmatch("[^ ]+"))
cs.maprotation("ffa", maps)

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

require"std.rcs"

--simple banner

local commands = require"std.commands"

local git = io.popen("echo `git rev-parse --short HEAD` `git show -s --format=%ci`")
local gitversion = git:read()
git = nil, git:close()
commands.add("info", function(info)
  playermsg("spaghettimod is a reboot of hopmod for programmers. Will be used for SDoS.\nKindly brought to you by pisto." .. (gitversion and "\nCommit " .. gitversion or ""), info.ci)
end)

local uuid, fence, sendmap = require"std.uuid", require"std.fence", require"std.sendmap", require"std.maploaded"

spaghetti.addhook("maploaded", function(info)
  local ci = info.ci
  local extra = ci.extra
  if not extra.rcs and not extra.spamrcs then
    extra.spamrcs = spaghetti.later(1000, function()
      extra.spamrcs = nil
      if extra.rcs then return end
      local function spam() playermsg("\n\n\f6Got no map and cannot play\f7? Install \f1Remote CubeScript\f7 to \f0auto-download\f7! Type \f0#rcs\f7 for more info", ci) end
      extra.spamrcs = spaghetti.later(20000, spam, true)
      spam()
    end)
  end
  extra.mapcrcfence = fence(ci)
end)

spaghetti.addhook("fence", function(info)
  local ci = info.ci
  local extra = ci.extra
  if server.smapname == "" or server.m_edit or extra.mapcrcfence ~= info.fence or server.mcrc ~= 0 and ci.mapcrc ~= server.mcrc or not sendmap.hasmap() then return end
  if coopeditmaps[server.smapname] and not server.m_teammode then
    sendmap.forcecurrent(ci, true, true, true)
    if not extra.rcs then playermsg("Textures and mapmodels for this map may be screwed. Install \f1Remote CubeScript\f7 for better support, type \f0#rcs\f7 for more info", ci) end
    return
  end
  if savemaps[server.smapname] ~= nil and extra.rcs then sendmap.forcecurrent(info.ci, false, true, savemaps[server.smapname])
  else playermsg("Cannot send map. Install \f1Remote CubeScript\f7 to enable auto-download, type \f0#rcs\f7 for more info", ci) end
end)

spaghetti.addhook("rcshello", function(info)
  if not info.ci.extra.spamrcs then return end
  spaghetti.cancel(info.ci.extra.spamrcs)
  info.ci.extra.spamrcs = nil
  playermsg("\f1Remote CubeScript\f7 detected! Maps will be auto-downloaded.", info.ci)
end)

spaghetti.addhook("clientdisconnect", L"_.ci.extra.spamrcs and spaghetti.cancel(_.ci.extra.spamrcs)")
