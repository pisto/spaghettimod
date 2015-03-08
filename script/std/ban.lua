--[[

  Ban lists. Takes over master gban support from the C++ side. If a client is banned it stays in a preconnection limbo, where he has a chance
  to take auth/admin and bypass the ban.

  You can configure multiple ban lists to have different access rules, and a connecting client is matched against all of them.

  The access policy specification uses three parameters (default values in parenthesis):
    client  (PRIV_AUTH)   | kick a connected client from the server for at most 4 hours and add the ban to this list
    full    (PRIV_ADMIN)  | full access to the ban list
    bypass  (PRIV_AUTH)   | bypass the ban
  The privileges can be specified as a table, where keys are the authdomains, and the values are either 'true' (any auth name) or a set where the keys are auth names.

  Each ban list has a default message shown to banned clients when they try to join, but each single ban can override that.

  There are four hardcoded lists:
    gban       | bans from the master server
    teamkill   | automatic vanilla teamkill kicks
    openmaster | kicks by anonymous masters on open servers
    kick       | kicks made with /authkick or /kick while being authed
  The teamkill and openmaster lists are reset by /clearbans, or when the server goes empty.

]]--

require"std.limbo"

local module = {}

local fp, L, ip, jsonpersist = require"utils.fp", require"utils.lambda", require"utils.ip", require"utils.jsonpersist"
local map, pick = fp.map, fp.pick
local playermsg, commands, iterators = require"std.playermsg", require"std.commands", require"std.iterators"
local auth = require"std.auth"

local banlists = {}

local unixtime = os.time
local unixprint = L"_ and os.date('!%c', _) .. ' UTC' or 'permanent'"

local function access(ci, access, authpriv)
  local numberpriv = type(access) == "number" and access or access[1]
  if numberpriv and (ci.privilege >= numberpriv or (authpriv and authpriv >= numberpriv)) then return true end
  return type(access) ~= "number" and auth.intersectauths(ci.extra.allclaims, access)
end

local function persistchanges(list, fname)
  local bans = map.lf(function(ip, expire, msg)
    return { net = tostring(ip), expire = expire, msg = msg ~= list.msg and msg or nil }
  end, module.enum(list.name))
  jsonpersist.save(#bans > 0 and { msg = list.msg, bans = bans } or {}, fname)
end

local function restore(list, fname)
  local t = jsonpersist.load(fname)
  if not t or not t.bans then return end
  local now, pruned = unixtime(), false
  for _, ban in ipairs(t.bans) do
    local expire = ban.expire and ban.expire - now
    if expire and expire < 0 then pruned = true
    else module.ban(list.name, ip.ip(ban.net), ban.msg, expire, nil, nil, true) end
  end
  return pruned and persistchanges(list, fname)
end

local function removeban(list, ban)
  local ok, tags = list.set:remove(ban)
  if ok and tags.expirehook then
    spaghetti.cancel(tags.expirehook)
    tags.expirehook = nil
  end
  return ok, tags
end

local function addban(list, ban, msg, expire)
  local ok, tags = list.set:put(ban, { msg = msg, expire = expire and unixtime() + expire or nil })
  tags.expirehook = ok and tags.expire and spaghetti.later(expire * 1000, function()
    tags.expirehook = nil
    removeban(list, ban)
    engine.writelog("ban: expire " .. tostring(ban) .. " [" .. list.name .. "]")
    return list.persist and persistchanges(list, list.persist)
  end) or nil
  return ok, tags
end

local function kickmask(mask, bypass, actor, reason, expire, actormsg)
  local kicked = pick.sf(function(ci)
    return mask:matches(ip.ip(engine.ENET_NET_TO_HOST_32(engine.getclientip(ci.clientnum)))) and (not bypass or not access(ci, bypass))
  end, iterators.clients())
  if not next(kicked) then return end
  actormsg = actormsg and actormsg or actor and server.colorname(actor, nil) or "The server"
  local who = table.concat(map.lp(L"server.colorname(_, nil)", kicked), ", ")
  expire = not expire and "permanently" or "until " .. unixprint(expire + unixtime())
  reason = (reason and reason ~= "") and " because: " .. reason or ""
  server.sendservmsg(("%s bans %s (%s) %s%s"):format(actormsg, tostring(mask), who, expire, reason))
  for ci in pairs(kicked) do
    if actor then
      local hooks = spaghetti.hooks.kick
      if hooks then hooks{ actor = actor, c = ci } end
    end
    engine.enet_peer_disconnect_later(engine.getclientpeer(ci.clientnum), engine.DISC_KICK)
  end
end

--interface

function module.unban(name, ban, nolog)
  local list = assert(banlists[name], "Cannot find ban list " .. name)
  local ok, tags = removeban(list, ban)
  if not ok then return false, map.spt(tags, L"_", tags) end
  if list.persist then persistchanges(list, list.persist) end
  if not nolog then engine.writelog("ban: delete " .. tostring(ban) .. " [" .. name .. "]") end
  return ok, tags
end

function module.ban(name, ban, msg, expire, actor, actormsg, nolog)
  local list = assert(banlists[name], "Cannot find ban list " .. name)
  local ok, tags = addban(list, ban, msg, expire)
  if not ok then return false, map.spt(tags, L"_", tags) end
  if list.persist then persistchanges(list, list.persist) end
  if not nolog then engine.writelog("ban: add " .. tostring(ban) .. " [" .. name .. "]") end
  kickmask(ip.ip(ban), list.bypass, actor, msg, expire, actormsg)
  return true, tags
end

local hardcoded = {}
function module.newlist(name, msg, access, persist, lite)
  if banlists[name] then module.dellist(name) end
  access = access or {}
  local list = {
    set = lite and ip.lipset() or ip.ipset(),
    lite = not not lite,
    name = name,
    msg = msg or "Your ip is banned. Use your (g)auth to join.",
    client = access.client or server.PRIV_AUTH,
    full = access.full or server.PRIV_ADMIN,
    bypass = access.bypass or server.PRIV_AUTH,
    hardcoded = access[hardcoded],
  }
  banlists[name] = list
  if not persist then return end
  restore(list, persist)
  list.persist = persist
end

function module.enum(name)
  local list = assert(banlists[name], "Cannot find ban list " .. name)
  return map.zf(function(ban, tags)
    return ban, tags.expire, tags.msg or list.msg
  end, list.set:enum())
end

function module.clear(name, nolog)
  local list = assert(banlists[name], "Cannot find ban list " .. name)
  map.nf(L"_2.expirehook and spaghetti.cancel(_2.expirehook)", list.set:enum())
  list.set = list.lite and ip.lipset() or ip.ipset()
  if not nolog then engine.writelog("ban: cleared [" .. name .. "]") end
end

function module.fill(name, banset)
  local list = assert(banlists[name], "Cannot find ban list " .. name)
  for ban, data in pairs(banset) do
    data = data == true and {} or data
    if not addban(list, ban, data.msg, data.expire) then engine.writelog("ban: failed clear-add " .. tostring(ip.ip(ban)) .. " [" .. name .. "]") end
  end
  for ci in iterators.clients() do
    local match, tags = list.set:matches(ip.ip(engine.ENET_NET_TO_HOST_32(engine.getclientip(ci.clientnum))))
    if next(match) then kickmask(match, list.bypass, nil, tags.msg or list.msg, tags.expire) end
  end
  engine.writelog("ban: filled [" .. name .. "]")
end

function module.dellist(name)
  local list = assert(banlists[name], "Cannot find ban list " .. name)
  assert(not list.hardcoded, "ban list " .. name .. " cannot be deleted")
  map.nf(L"_2.expirehook and spaghetti.cancel(_2.expirehook)", list.set:enum())
  banlists[name] = nil
end

function module.checkban(ci)
  local ip = ip.ip(engine.ENET_NET_TO_HOST_32(engine.getclientip(ci.clientnum)))
  return map.mp(function(name, list)
    local match = not access(ci, list.bypass) and list.set:matches(ip)
    if not match or not next(match) then return end
    local match, tags = next(match)
    return name, { ban = match, expire = tags.expire, msg = tags.msg or list.msg }
  end, banlists)
end

--taking over ban control from vanilla

spaghetti.addhook("masterin", function(info)
  if info.input:match"^ *cleargbans *$" then
    module.clear"gban"
    info.skip = true
    return
  end
  local gban = info.input:match"^ *addgban +(.-) *$"
  if not gban then return end
  info.skip = true
  local gbanip = ip.ip(gban)
  if not gbanip then
    local fixed, extra = (gban .. ".0.0.0"):match("(%d+%.%d+%.%d+%.%d+)(.*)")
    gbanip = fixed and ip.ip(fixed, 8 * ( 1 + #extra / 2))
  end
  if not gbanip then engine.writelog("Unrecognized gban: " .. info.input) return end
  if not module.ban("gban", gbanip) then engine.writelog("Cannot add gban " .. tostring(gban)) end
end)

spaghetti.addhook("addban", function(info)
  info.skip = true
  local ip = ip.ip(engine.ENET_NET_TO_HOST_32(info.ip))
  if info.type == "kick" then
    local actormsg = info.authname and
    (info.authdesc and #info.authdesc > 0 and ("%s as '\fs\f5%s\fr' [\fs\f0%s\fr]"):format(server.colorname(info.ci, nil), info.authname, info.authdesc)
      or ("%s as '\fs\f5%s\fr'"):format(server.colorname(info.ci, nil), info.authname))
    or server.colorname(info.ci, nil)
    local reason = info.reason and info.reason ~= "" and info.reason or nil
    module.ban(info.authname and "kick" or "openmaster", ip, reason, info.time/1000, info.ci, actormsg)
    if access(info.ci, banlists.kick.client) then playermsg("Consider using #ban to specify ban expiration and message.", info.ci) end
  elseif info.type == "teamkill" then module.ban("teamkill", ip, nil, info.time/1000) end
end)

local function clearbans()
  module.clear("teamkill")
  module.clear("openmaster")
end
spaghetti.addhook("noclients", clearbans)

spaghetti.addhook(server.N_CLEARBANS, function(info)
  if info.skip or info.ci.privilege == server.PRIV_NONE then return end
  info.skip = true
  clearbans()
  server.sendservmsg("cleared all bans")
  playermsg("Cleared kicks by open masters. Use #bandel for a more complete control.", info.ci)
end)

function module.kickpersist(fname)
  local kick = banlists.kick
  assert(not kick.persist, "Trying to call std.ban.kickpersist multiple times")
  map.nf(L"assert(false, 'kick ban list is not empty before calling kickpersist')", module.enum"kick")
  restore(kick, fname)
  kick.persist = fname
end

spaghetti.addhook("trykick", function(info)
  if info.skip then return end
  info.skip = true
  local vinfo = info.vinfo
  if vinfo.clientnum == info.ci.clientnum then return playermsg("You tried to kick yourself.", info.ci) end
  if not vinfo or not vinfo.connected then return playermsg("No such client.", info.ci) end
  local list = banlists[info.authdesc and "kick" or "openmaster"]
  local client, bypass = list.client, list.bypass
  if not info.trial and not access(info.ci, client, info.authpriv) then return playermsg("You lack privileges to kick players.", info.ci) end
  if access(vinfo, bypass) then return playermsg("Cannot kick because the client can bypass the ban.", info.ci) end
  info.cankick = true
  if info.trial then return end
  local _ip, time = engine.getclientip(vinfo.clientnum), 4*60*60
  local hooks = spaghetti.hooks.addban
  if hooks then hooks{ type = "kick", ip = _ip, time = time * 1000, ci = info.ci, reason = info.reason, authname = info.authname, authdesc = info.authdesc } end
end)

--client commands

commands.add("banenum", function(info)
  if info.skip then return end
  local name = info.args:match"^[^ ]*"
  if name == "" then playermsg("Which list? Available lists: " .. table.concat(map.pl(L"_", banlists), " "), info.ci) return end
  local list = banlists[name]
  if not list then return playermsg("Ban list not found", info.ci) end
  local header = false
  for ip, expire, msg in module.enum(name) do
    if not header then
      playermsg("\f0Expire date\f7 :: \f6ip/range\f7 :: Note", info.ci)
      header = true
    end
    playermsg(("\f0%s\f7 :: \f6%s"):format(unixprint(expire), tostring(ip)) .. (msg ~= list.msg and ("\f7 :: " .. msg) or ""), info.ci)
  end
  return header or playermsg("Empty.", info.ci)
end, "#banenum <list>", "Enumerate bans in banlist.")

local timespec = { d = { m = 60*60*24, n = "days" }, h = { m = 60*60, n = "hours" }, m = { m = 60, n = "minutes" } }
timespec.D, timespec.H, timespec.M = timespec.d, timespec.h, timespec.m
local function kickban(info)
  local force, who, name, time, mult, msg = info.args:match("^(!?)([%d%./]+) *([^ %d]*) *(%d*)([DdHhMm]?) *(.-) *$")
  local cn, _ip = tonumber(who), ip.ip(who or "")
  force, name, msg = force == "!", name == "" and "kick" or name, msg ~= "" and msg or nil
  if (not cn and not _ip) or (not _ip and force) or not time or (time ~= "" and not timespec[mult]) then return playermsg("Bad format", info.ci) end
  if time == "" then time, msg = nil, mult ~= "" and msg and mult .. " " .. msg or msg
  elseif time == '0' then return playermsg("Cannot ban for no time.", info.ci)
  else time = timespec[mult].m * time end
  local list = banlists[name]
  if not list then return playermsg("Ban list not found", info.ci) end
  _ip = _ip or ip.ip(engine.ENET_NET_TO_HOST_32(engine.getclientip(cn)))
  if cn and _ip.ip == 0 then return playermsg("Client not found.", info.ci) end
  if cn == info.ci.clientnum then return playermsg("Cannot self ban.", info.ci) end
  local can
  if cn then can = access(info.ci, list.client) else can = access(info.ci, list.full) end
  if not can then return playermsg("Permission denied.", info.ci) end
  if cn and access(engine.getclientinfo(cn), list.bypass) then return playermsg("The player has sufficient credentials to bypass the ban, not adding.", info.ci) end
  local toolong = (not time or time > 4*60*60) and not access(info.ci, list.full)
  local matches = list.set:matches(_ip)
  local firstmatch = next(matches)
  if firstmatch then
    local failmsg
    if not next(matches, firstmatch) and firstmatch.mask < _ip.mask then failmsg = "it is contained by " .. tostring(firstmatch)
    else failmsg = not force and (firstmatch == _ip and "it is already present" or "it contains ranges " .. table.concat(map.lp(tostring, matches), " ")) end
    if failmsg then playermsg("Cannot add ban " .. tostring(_ip) .. " because " .. failmsg, info.ci) return end
    for range in pairs(matches) do removeban(list, range) end
  end
  module.ban(name, _ip, msg, toolong and 4*60*60 or time, info.ci)
  playermsg(toolong and "Ban added (4 hours only as you lack full privileges)." or "Ban added.", info.ci)
end
local help = "#ban <cn|[!]range> [list=kick] <time> [reason]\nTime format: #d|#h|#m\nIf !forced, coalesces present ranges, or updates the message/expiration"
commands.add("kick", kickban, help)
commands.add("ban", kickban, help)

commands.add("bandel", function(info)
  local force, who, name = info.args:match("^(!?)([%d%./]+) *([^ ]*) *$")
  local _ip = ip.ip(who or "")
  name, force = name == "" and "kick" or name, force == "!"
  local list = banlists[name]
  if not _ip then return playermsg("Bad format.", info.ci) end
  if not list then return playermsg("Ban list not found", info.ci) end
  if not access(info.ci, list.full) then return playermsg("Permission denied.", info.ci) end
  local matches = list.set:matches(_ip)
  local firstmatch = next(matches)
  if firstmatch ~= _ip then
    local failmsg
    if not firstmatch then failmsg = "it is not in the ban list."
    elseif not next(matches, firstmatch) and firstmatch.mask < _ip.mask then failmsg = "it is contained by " .. tostring(firstmatch)
    else failmsg = not force and "it contains ranges " .. table.concat(map.lp(tostring, matches), " ") end
    if failmsg then playermsg("Cannot delete ban " .. tostring(_ip) .. " because " .. failmsg, info.ci) return end
  end
  for range in pairs(matches) do module.unban(name, range) end
  playermsg("Ban deleted.", info.ci)
end, "#bandel <[!]range> [list=kick]\nIf !forced, removes all included ranges.")

--escaping the ban

spaghetti.addhook("enterlimbo", function(info)
  local bans = module.checkban(info.ci)
  if not next(bans) then return end
  info.ci.extra.bans = bans
  info.ci.extra.limbo.locks.ban = 1/0
  local msg = "You cannot join because you are in a ban list:"
  for listname, match in pairs(bans) do
    msg = ("%s\n%s (%s), expiration: %s"):format(msg, listname, match.msg, unixprint(match.expire))
    engine.writelog(("ban: hold %s for %s [%s] (%s)"):format(ip.ip(engine.ENET_NET_TO_HOST_32(engine.getclientip(info.ci.clientnum))), match.ban, listname, match.msg))
  end
  playermsg(msg .. "\nUse your (g)auth to join.", info.ci)
end)

spaghetti.addhook("master", function(info)
  if not info.ci.extra.limbo or not info.ci.extra.limbo.locks.ban then return end
  info.ci.extra.bans = module.checkban(info.ci)
  if not next(info.ci.extra.bans) then
    info.ci.extra.limbo.locks.ban, info.ci.extra.bans = nil
    playermsg("Your credentials are sufficient to bypass your ip ban.", info.ci)
  end
end)

--initial state

module.newlist("gban", "eihrul's decision", { [hardcoded] = true, client = server.PRIV_ADMIN })
module.newlist("kick", "your ip is banned", { [hardcoded] = true})
module.newlist("teamkill", "you teamkill too much", { [hardcoded] = true, client = server.PRIV_ADMIN })
module.newlist("openmaster", "your ip is banned", { [hardcoded] = true, client = server.PRIV_MASTER })

return module
