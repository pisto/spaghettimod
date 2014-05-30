--[[

  Ban lists. Takes over master gban support from the C++ side.

]]--

local module = {}

local fp, lambda, ip = require"utils.fp", require"utils.lambda", require"utils.ip"
local map, pick, breakk, I, Lr = fp.map, fp.pick, fp.breakk, fp.I, lambda.Lr

local playermsg, servertag, commands, allclaims, later, iterators = require"std.playermsg", require"std.servertag", require"std.commands", require"std.allclaims", require"utils.later", require"std.iterators"

--default parameters: msg = "Your ip is banned. Use your (g)auth to join.", client = PRIV_AUTH, full = PRIV_ADMIN, bypass = PRIV_AUTH
module.bans = {}


local function unixtime()
  local p = io.popen"date +%s"
  local date = p:read"*n"
  p:close()
  return date
end

local function unixprint(seconds)
  local p = io.popen("date --date=@'" .. seconds .. "' +'%b %d %T'")
  local date = p:read"*a":sub(1, -2)
  p:close()
  return date
end


function module.remove(list, ban, force, nocheck)
  if not nocheck then
    local matcher = list.set:matcherof(ban)
    if matcher then if matcher ~= ban then return false, { matcher = matcher } end
    else
      local matches = list.set:matchesof(ban)
      if matches and not force then return false, matches end
      map.np(function(_ip) module.remove(list, _ip, false, true) end, matches)
    end
  end
  list.set:remove(ban)
  list.tags[tostring(ban)] = nil
  return true
end

function module.add(list, ban, msg, expire, force)
  local ok, shadows = list.set:put(ban)
  if not ok and (not force or (shadows.matcher and shadows.matcher ~= ban)) then return false, shadows end
  if not ok and not shadows.matcher then
    map.np(function(_ip) module.remove(list, _ip, true, true) end, shadows)
    list.set:put(ban)
  end
  list.tags[tostring(ban)] = (msg or expire) and { msg = msg, expire = expire and unixtime() + expire or nil } or nil
  return true
end


local function pruner(j)
  local time = unixtime()
  map.np(function(_, list) map.np(function(_ip, tag)
      return tag.expire and tag.expire <= time and module.remove(list, ip.ip(_ip))
  end, list.tags or {}) end, j and { j } or module.bans)
end
later.later(10000, pruner, true)


local function access(ci, access)
  if type(access) == "number" then return ci.privilege >= access end
  return allclaims.intersect(ci.extra.allclaims or {}, access)
end


function module.init(name, msg, access)
  local list = { set = ip.ipset(), msg = msg, name = name, tags = {} }
  module.bans[name] = access and map.tmv(list, function(n) return n, access[n] end, "client", "full", "bypass") or list
end


function module.checkban(ci)
  local _ip = ip.ip(engine.ENET_NET_TO_HOST_32(engine.getclientip(ci.clientnum)))
  return map.mp(function(_, list)
    local match = list.set:matcherof(_ip)
    if not match or access(ci, list.bypass or server.PRIV_AUTH) then return end
    return list, match
  end, module.bans)
end



spaghetti.addhook("masterin", function(info)
  local gbans = module.bans.gban
  if info.input:match"^ *cleargbans *$" then
    gbans.set = ip.ipset()
    info.skip = true
    return
  end
  local gban = info.input:match"^ *addgban +(.-) *$"
  if not gban then return end
  info.skip = true
  local mask = 0
  local o1, o2, o3, o4 = map.uf(function(oct)
    if oct == "*" then mask = -1 breakk() end
    mask = mask + 8 return oct
  end, gban:gmatch("(%d+)%.?"))
  gban = o1 and ip.ip(table.concat({o1, o2 or "0", o3 or "0", o4 or "0"}, '.'), mask)
  if not gban then engine.writelog("Unrecognized gban: " .. info.input) return end
  module.add(gbans, gban)
end)
module.init("gban", "eihrul's decision", { client = server.PRIV_ADMIN })


spaghetti.addhook("addban", function(info)
  info.skip = true
  local _ip = ip.ip(engine.ENET_NET_TO_HOST_32(info.ip))
  if info.type == "kick" then
    local who, reason = info.authname and info.authname or info.ci.name, info.reason and info.reason ~= "" and " because: " .. info.reason or ""
    module.add(module.bans[info.authname and "kick" or "openmaster"], _ip, "you have been kicked by " .. who .. reason, info.time/1000)
    if access(info.ci, module.bans.kick.client or server.PRIV_AUTH) then playermsg("Consider using #ban to specify ban expiration and message.", info.ci) end
  elseif info.type == "teamkill" then module.add(module.bans.teamkill, _ip, nil, info.time/1000) end
end)
module.init("kick", "your ip is banned")
local function clearbans()
  module.init("teamkill", "you teamkill too much")
  module.init("openmaster", "you have been kicked")
end
clearbans()
spaghetti.addhook("disconnected", function() return server.clients.length() == 1 and clearbans() end)



commands.add("banenum", function(info)
  local list = info.args:match"^[^ ]*"
  list = module.bans[list == "" and "kick" or list]
  if not list then return playermsg("Ban list not found", info.ci) end
  local header = false
  map.nf(function(_ip)
    if not header then
      playermsg(("%-17s %-17s Reason"):format("IP/range", "Expire date"), info.ci)
      header = true
    end
    local tag = list.tags[tostring(_ip)] or {}
    playermsg(("%-17s %-17s %s"):format(tostring(_ip), tag.expire and unixprint(tag.expire) or "permanent", tag.msg or list.msg), info.ci)
  end, list.set:enum())
  return header or playermsg("Empty.", info.ci)
end, "#banenum [list=kick]")

commands.add("banlists", function(info) playermsg(table.concat(map.pl(I, module.bans), " "), info.ci) end)

local timespec = { d = { m = 60*60*24, n = "days" }, h = { m = 60*60, n = "hours" }, m = { m = 60, n = "minutes" } }
timespec.D, timespec.H, timespec.M = timespec.d, timespec.h, timespec.m
local function kickban(info)
  local force, who, list, time, mult, msg = info.args:match("^(!?)([%d%./]+) *([^ ]*) +(%d+)([DdHhMm]) *(.-) *$")
  local cn, _ip = tonumber(who), ip.ip(who or "")
  force, list, msg = force == "!", module.bans[list == "" and "kick" or list], msg ~= "" and msg or nil
  if (not cn and not _ip) or (not _ip and force) or not time then return playermsg("Bad format.", info.ci) end
  time = timespec[mult].m * time
  if not list then return playermsg("Ban list not found", info.ci) end
  _ip = _ip or ip.ip(engine.ENET_NET_TO_HOST_32(engine.getclientip(cn)))
  if cn and _ip.ip == 0 then return playermsg("Client not found.", info.ci) end
  if cn == info.ci.clientnum then return playermsg("Cannot self ban.") end
  local can
  if cn then can = access(info.ci, list.client or server.PRIV_AUTH) else can = access(info.ci, list.full or server.PRIV_ADMIN) end
  if not can then return playermsg("Permission denied.", info.ci) end
  if cn and access(engine.getclientinfo(cn), list.bypass or server.PRIV_AUTH) then return playermsg("The player has sufficient credentials to bypass the ban, not adding.") end
  local ok, overlap = module.add(list, _ip, msg, time, force)
  if not ok then
    if overlap.matcher == _ip then overlap = "is already present"
    elseif overlap.matcher then overlap = "is contained by " .. tostring(overlap.matcher)
    else overlap = "contains other ranges" end
    return playermsg("Cannot add ban because range " .. overlap, info.ci)
  end
  map.nf(function(ci)
    local cip = ip.ip(engine.ENET_NET_TO_HOST_32(engine.getclientip(ci.clientnum)))
    if _ip:matches(cip) and not access(ci, list.bypass or server.PRIV_AUTH) then
      playermsg(msg or list.msg, ci)
      engine.enet_host_flush(engine.serverhost)
      engine.disconnect_client(ci.clientnum, engine.DISC_KICK)
    end
  end, iterators.clients())
  playermsg("Ban added.", info.ci)
end
local help = "#ban cn|[!]range [list=kick] time [reason]\nTime format: #d|#h|#m\nIf !forced, coalesces present ranges, or updates the message/expiration"
commands.add("kick", kickban, help)
commands.add("ban", kickban, help)

commands.add("bandel", function(info)
  local force, who, list = info.args:match("^(!?)([%d%./]+) *([^ ]*) *$")
  local _ip = ip.ip(who or "")
  list = module.bans[list == "" and "kick" or list]
  if not _ip then return playermsg("Bad format.", info.ci) end
  if not list then return playermsg("Ban list not found", info.ci) end
  if not access(info.ci, list.full or server.PRIV_ADMIN) then return playermsg("Permission denied.", info.ci) end
  local ok, overlap = module.remove(list, _ip, force)
  if not ok then return playermsg("Cannot delete ban because range " .. (overlap.matcher and ("is contained by " .. tostring(overlap.matcher)) or "contains other ranges"), info.ci) end
end, "#bandel [!]range [list=kick]\nIf !forced, removes all included ranges.")

spaghetti.addhook(server.N_CLEARBANS, function(info)
  if info.skip or info.ci.privilege == server.PRIV_NONE then return end
  info.skip = true
  clearbans()
  engine.sendservmsg("cleared all bans")
  playermsg("Cleared kicks by open masters. Use #bandel for a more complete control.", info.ci)
end)



spaghetti.addhook("enterlimbo", function(info)
  info.ci.extra.bans = module.checkban(info.ci)
  if not next(info.ci.extra.bans) then return end
  info.ci.extra.limbo.locks.ban = 1/0
  local msg = "You cannot join because you are in a ban list:"
  map.np(function(list, match) msg = ("%s\n%s (%s)"):format(msg, list.name, (list.tags[tostring(match)] or {}).msg or list.msg) end, info.ci.extra.bans)
  playermsg(msg .. "\nUse your (g)auth within 10 seconds to join.", info.ci)
end)

spaghetti.addhook("master", function(info)
  if not info.ci.extra.limbo or not next(info.ci.extra.bans) then return end
  info.ci.extra.bans = module.checkban(info.ci)
  if not next(info.ci.extra.bans) then
    info.ci.extra.limbo.locks.ban = nil
    playermsg("Your credentials are sufficient to bypass your ip ban.", info.ci)
  end
end)

spaghetti.addhook("connected", function(info)
  if next(module.checkban(info.ci)) then engine.disconnect_client(info.ci.clientnum, engine.DISC_KICK) end
end)

