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

local module = {}

local fp, lambda, ip, posix, jsonpersist = require"utils.fp", require"utils.lambda", require"utils.ip", require"posix", require"utils.jsonpersist"
local map, pick, breakk, I, Lr = fp.map, fp.pick, fp.breakk, fp.I, lambda.Lr
local playermsg, servertag, commands, allclaims, iterators = require"std.playermsg", require"utils.servertag", require"std.commands", require"std.allclaims", require"std.iterators"

local banlists = {}

local unixtime = posix.time
local unixprint = Lr"_ == 1/0 and 'permanent' or (os.date('!%c', _) .. ' UTC')"

local function access(ci, access)
  if type(access) == "number" then return ci.privilege >= access end
  return allclaims.intersect(ci.extra.allclaims or {}, access)
end

local function remove(list, ban, name)
  list.set:remove(ban)
  local tagskey = tostring(ban)
  local tag = list.tags[tagskey]
  list.tags[tagskey] = nil
  if name then engine.writelog("ban: delete " .. tostring(ban) .. " [" .. name .. "]") end
  return tag and tag.expire and spaghetti.cancel(tag.expire.later)
end

--interface

function module.unban(name, ban, force)
  local list = banlists[name]
  if not list then error("Cannot find ban list " .. name) end
  local matcher = list.set:matcherof(ban)
  if matcher then if matcher ~= ban then return false, { matcher = matcher } end
  else
    local matches = list.set:matchesof(ban)
    if matches and not force then return false, matches end
    map.np(function(_ip) remove(list, _ip) end, matches)
  end
  remove(list, ban)
  return true
end

function module.ban(name, ban, msg, expire, force)
  local list = banlists[name]
  if not list then error("Cannot find ban list " .. name) end
  local ok, shadows = list.set:put(ban)
  if not ok and (not force or (shadows.matcher and shadows.matcher ~= ban)) then return false, shadows end
  if not ok and not shadows.matcher then
    map.np(function(_ip) remove(list, _ip) end, shadows)
    list.set:put(ban)
  end
  expire = (expire and expire ~= 1/0) and { when = unixtime() + expire, later = spaghetti.later(expire * 1000, function()
    remove(list, ban)
    engine.writelog("ban: expire " .. tostring(ban) .. " [" .. name .. "]")
  end) } or nil
  if msg or expire then list.tags[tostring(ban)] = { msg = msg, expire = expire } end
  return true
end

local hardcoded = {}
function module.newlist(name, msg, access)
  if banlists[name] then module.dellist(name) end
  access = access or {}
  banlists[name] = {
    set = ip.ipset(),
    msg = msg or "Your ip is banned. Use your (g)auth to join.",
    tags = {},
    client = access.client or server.PRIV_AUTH,
    full = access.full or server.PRIV_ADMIN,
    bypass = access.bypass or server.PRIV_AUTH,
    hardcoded = access[hardcoded]
  }
end

function module.enum(name)
  local list = banlists[name]
  if not list then error("Cannot find ban list " .. name) end
  return map.zf(function(ban)
    local tag = list.tags[tostring(ban)]
    return ban, tag and tag.expire and tag.expire.when or 1/0, tag and tag.msg or list.msg
  end, list.set:enum())
end

function module.clear(name)
  local list = banlists[name]
  if not list then error("Cannot find ban list " .. name) end
  map.np(Lr"_2.expire and spaghetti.cancel(_2.expire.later)", list.tags)
  list.set, list.tags = ip.ipset(), {}
end

function module.dellist(name)
  local list = banlists[name]
  if not list then return end
  if list.hardcoded then error("ban list  " .. name .. " cannot be deleted") end
  map.np(Lr"_2.expire and spaghetti.cancel(_2.expire.later)", list.tags)
  banlists[name] = nil
end

function module.checkban(ci)
  local _ip = ip.ip(engine.ENET_NET_TO_HOST_32(engine.getclientip(ci.clientnum)))
  return map.mp(function(_, list)
    local match = list.set:matcherof(_ip)
    if not match or access(ci, list.bypass) then return end
    return list, match
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
  local mask = 0
  local o1, o2, o3, o4 = map.uf(function(oct)
    if oct == "*" then mask = -1 breakk() end
    mask = mask + 8 return oct
  end, gban:gmatch("(%d+)%.?"))
  gban = o1 and ip.ip(table.concat({o1, o2 or "0", o3 or "0", o4 or "0"}, '.'), mask)
  if not gban then engine.writelog("Unrecognized gban: " .. info.input) return end
  module.ban("gban", gban)
end)

spaghetti.addhook("addban", function(info)
  info.skip = true
  local _ip = ip.ip(engine.ENET_NET_TO_HOST_32(info.ip))
  if info.type == "kick" then
    local who, reason = info.authname and info.authname or info.ci.name, info.reason and info.reason ~= "" and " because: " .. info.reason or ""
    module.ban(info.authname and "kick" or "openmaster", _ip, "you have been kicked by " .. who .. reason, info.time/1000)
    if access(info.ci, banlists.kick.client) then playermsg("Consider using #ban to specify ban expiration and message.", info.ci) end
  elseif info.type == "teamkill" then module.ban("teamkill", _ip, nil, info.time/1000) end
end)

local function clearbans()
  module.clear("teamkill")
  module.clear("openmaster")
end
spaghetti.addhook("disconnected", function() return server.clients:length() == 1 and clearbans() end)

spaghetti.addhook(server.N_CLEARBANS, function(info)
  if info.skip or info.ci.privilege == server.PRIV_NONE then return end
  info.skip = true
  clearbans()
  server.sendservmsg("cleared all bans")
  playermsg("Cleared kicks by open masters. Use #bandel for a more complete control.", info.ci)
end)

--client commands

commands.add("banenum", function(info)
  local name = info.args:match"^[^ ]*"
  name = name == "" and "kick" or name
  local list = banlists[name]
  if not list then return playermsg("Ban list not found", info.ci) end
  local header = false
  map.nf(function(_ip, expire, msg)
    if not header then
      playermsg("\f0Expire date\f7 :: \f6ip/range\f7 :: Note", info.ci)
      header = true
    end
    playermsg(("\f0%s\f7 :: \f6%s"):format(unixprint(expire), tostring(_ip)) .. (msg ~= list.msg and ("\f7 :: \f2" .. msg) or ""), info.ci)
  end, module.enum(name))
  return header or playermsg("Empty.", info.ci)
end, "#banenum [list=kick]")

commands.add("banlists", function(info) playermsg(table.concat(map.pl(I, banlists), " "), info.ci) end)

local timespec = { d = { m = 60*60*24, n = "days" }, h = { m = 60*60, n = "hours" }, m = { m = 60, n = "minutes" } }
timespec.D, timespec.H, timespec.M = timespec.d, timespec.h, timespec.m
local function kickban(info)
  local force, who, name, time, mult, msg = info.args:match("^(!?)([%d%./]+) *([^ ]*) +(%d+)([DdHhMm]) *(.-) *$")
  local cn, _ip = tonumber(who), ip.ip(who or "")
  force, name, msg = force == "!", name == "" and "kick" or name, msg ~= "" and msg or nil
  if (not cn and not _ip) or (not _ip and force) or not time then return playermsg("Bad format.", info.ci) end
  time = timespec[mult].m * time
  local list = banlists[name]
  if not list then return playermsg("Ban list not found", info.ci) end
  _ip = _ip or ip.ip(engine.ENET_NET_TO_HOST_32(engine.getclientip(cn)))
  if cn and _ip.ip == 0 then return playermsg("Client not found.", info.ci) end
  if cn == info.ci.clientnum then return playermsg("Cannot self ban.") end
  local can
  if cn then can = access(info.ci, list.client) else can = access(info.ci, list.full) end
  if not can then return playermsg("Permission denied.", info.ci) end
  if cn and access(engine.getclientinfo(cn), list.bypass) then return playermsg("The player has sufficient credentials to bypass the ban, not adding.") end
  local toolong = time > 4*60*60 and not access(info.ci, list.full)
  local ok, overlap = module.ban(name, _ip, msg, toolong and 4*60*60 or time, force)
  if not ok then
    if overlap.matcher == _ip then overlap = "is already present"
    elseif overlap.matcher then overlap = "is contained by " .. tostring(overlap.matcher)
    else overlap = "contains other ranges" end
    return playermsg("Cannot add ban because range " .. overlap, info.ci)
  end
  map.nf(function(ci)
    local cip = ip.ip(engine.ENET_NET_TO_HOST_32(engine.getclientip(ci.clientnum)))
    if _ip:matches(cip) and not access(ci, list.bypass) then
      playermsg(msg or list.msg, ci)
      engine.enet_host_flush(engine.serverhost)
      engine.disconnect_client(ci.clientnum, engine.DISC_KICK)
    end
  end, iterators.clients())
  playermsg(toolong and "Ban added (4 hours only as you lack privileges)." or "Ban added.", info.ci)
end
local help = "#ban cn|[!]range [list=kick] time [reason]\nTime format: #d|#h|#m\nIf !forced, coalesces present ranges, or updates the message/expiration"
commands.add("kick", kickban, help)
commands.add("ban", kickban, help)

commands.add("bandel", function(info)
  local force, who, name = info.args:match("^(!?)([%d%./]+) *([^ ]*) *$")
  local _ip = ip.ip(who or "")
  name = name == "" and "kick" or name
  local list = banlists[name]
  if not _ip then return playermsg("Bad format.", info.ci) end
  if not list then return playermsg("Ban list not found", info.ci) end
  if not access(info.ci, list.full) then return playermsg("Permission denied.", info.ci) end
  local ok, overlap = module.unban(name, _ip, force)
  if not ok then return playermsg("Cannot delete ban because range " .. (overlap.matcher and ("is contained by " .. tostring(overlap.matcher)) or "contains other ranges"), info.ci) end
  playermsg("Ban deleted.", info.ci)
end, "#bandel [!]range [list=kick]\nIf !forced, removes all included ranges.")

--escaping the ban

spaghetti.addhook("enterlimbo", function(info)
  local bans = module.checkban(info.ci)
  info.ci.extra.bans = bans
  if not next(bans) then return end
  info.ci.extra.limbo.locks.ban = 1/0
  local msg = "You cannot join because you are in a ban list:"
  map.np(function(list, match) msg = ("%s\n%s (%s)"):format(msg, list.name, (list.tags[tostring(match)] or {}).msg or list.msg) end, bans)
  playermsg(msg .. "\nUse your (g)auth to join.", info.ci)
end)

spaghetti.addhook("master", function(info)
  if not info.ci.extra.limbo or not next(info.ci.extra.bans) then return end
  info.ci.extra.bans = module.checkban(info.ci)
  if not next(info.ci.extra.bans) then
    info.ci.extra.limbo.locks.ban = nil
    playermsg("Your credentials are sufficient to bypass your ip ban.", info.ci)
  end
end)

--initial state

module.newlist("gban", "eihrul's decision", { [hardcoded] = true, client = server.PRIV_ADMIN })
module.newlist("kick", "your ip is banned", { [hardcoded] = true})
module.newlist("teamkill", "you teamkill too much", { [hardcoded] = true, client = server.PRIV_ADMIN })
module.newlist("openmaster", "you have been kicked", { [hardcoded] = true, client = server.PRIV_MASTER })

return module

