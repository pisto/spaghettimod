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
local playermsg, commands, allclaims, iterators = require"std.playermsg", require"std.commands", require"std.allclaims", require"std.iterators"

local banlists = {}

local unixtime = posix.time
local unixprint = Lr"_ == 1/0 and 'permanent' or (os.date('!%c', _) .. ' UTC')"

local function access(ci, access)
  if type(access) == "number" then return ci.privilege >= access end
  return allclaims.intersect(ci.extra.allclaims or {}, access)
end

local function remove(list, ban, log)
  local removed = list.set:remove(ban)
  local tagskey = tostring(ban)
  local tag = list.tags[tagskey]
  list.tags[tagskey] = nil
  if log and removed then engine.writelog("ban: delete " .. tostring(ban) .. " [" .. list.name .. "]") end
  return tag and tag.expire and spaghetti.cancel(tag.expire.later)
end

local function persistchanges(list, fname)
  if not fname then return end
  local bans = map.lf(function(ip, expire, msg)
    return { net = tostring(ip), expire = expire ~= 1/0 and expire or nil, msg = msg ~= list.msg and msg or nil }
  end, module.enum(list.name))
  jsonpersist.save(#bans > 0 and { msg = list.msg, bans = bans } or {}, fname)
end

local function restore(list, fname)
  local t = jsonpersist.load(fname)
  if not t or not t.bans then return end
  local msg, now, pruned = t.msg, unixtime(), false
  for _, ban in ipairs(t.bans) do
    local expire = (ban.expire or 1/0) - now
    if expire < 0 then pruned = true
    else module.ban(list.name, ip.ip(ban.net), ban.msg or msg, expire, true, true) end
  end
  return pruned and persistchanges(list, fname)
end

--interface

function module.unban(name, ban, force)
  local list = banlists[name]
  if not list then error("Cannot find ban list " .. name) end
  local matcher = list.set:matcherof(ban)
  if matcher then
    if matcher ~= ban then return false, { matcher = matcher } end
    remove(list, ban, true)
  else
    local matches = list.set:matchesof(ban)
    if not next(matches) then return false end
    if not force then return false, matches end
    for ip in pairs(matches) do remove(list, ip, true) end
  end
  persistchanges(list, list.persist)
  return true
end

function module.ban(name, ban, msg, expire, force, nolog)
  local list = banlists[name]
  if not list then error("Cannot find ban list " .. name) end
  local ok, shadows = list.set:put(ban)
  if not ok and (not force or (shadows.matcher and shadows.matcher ~= ban)) then return false, shadows end
  if not ok and not shadows.matcher then
    for ip in pairs(shadows) do remove(list, ip, true) end
    ok = list.set:put(ban)
  end
  expire = (expire and expire ~= 1/0) and { when = unixtime() + expire, later = spaghetti.later(expire * 1000, function()
    remove(list, ban)
    engine.writelog("ban: expire " .. tostring(ban) .. " [" .. name .. "]")
    persistchanges(list, list.persist)
  end) } or nil
  msg = msg ~= list.msg and msg or nil
  if msg or expire then list.tags[tostring(ban)] = { msg = msg, expire = expire } end
  if ok and not nolog then engine.writelog("ban: add " .. tostring(ban) .. " [" .. name .. "]") end
  persistchanges(list, list.persist)
  return true
end

local hardcoded = {}
function module.newlist(name, msg, access, persist)
  if banlists[name] then module.dellist(name) end
  access = access or {}
  local list = {
    set = ip.ipset(),
    name = name,
    msg = msg or "Your ip is banned. Use your (g)auth to join.",
    tags = {},
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
  engine.writelog("ban: cleared [" .. name .. "]")
end

function module.dellist(name)
  local list = banlists[name]
  if not list then return end
  if list.hardcoded then error("ban list  " .. name .. " cannot be deleted") end
  map.np(Lr"_2.expire and spaghetti.cancel(_2.expire.later)", list.tags)
  banlists[name] = nil
end

function module.checkban(ci)
  local ip = ip.ip(engine.ENET_NET_TO_HOST_32(engine.getclientip(ci.clientnum)))
  return map.mp(function(_, list)
    local match = list.set:matcherof(ip)
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
  local ip = ip.ip(engine.ENET_NET_TO_HOST_32(info.ip))
  if info.type == "kick" then
    local who, reason = info.authname and info.authname or info.ci.name, info.reason and info.reason ~= "" and " because: " .. info.reason or ""
    module.ban(info.authname and "kick" or "openmaster", ip, "you have been kicked by " .. who .. reason, info.time/1000)
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
  map.nf(Lr"assert(false, 'kick ban list is not empty before calling kickpersist')", module.enum"kick")
  restore(kick, fname)
  kick.persist = fname
end

local function kickmask(mask, bypass, actor, timemsg, reasonmsg, actormsg)
  actormsg = actormsg and actormsg or actor and server.colorname(actor, nil) or "The server"
  reasonmsg = reasonmsg and " because: " .. reasonmsg or ""
  timemsg = timemsg and " " .. timemsg or ""
  for ci in iterators.clients() do
    local cip, peer = ip.ip(engine.ENET_NET_TO_HOST_32(engine.getclientip(ci.clientnum))), engine.getclientpeer(ci.clientnum)
    if mask:matches(cip) and (not bypass or not access(ci, bypass)) then
      if peer then
        if actor then
          local hooks = spaghetti.hooks.kick
          if hooks then hooks{ actor = actor, c = ci } end
        end
        server.sendservmsg(("%s kicks %s (%s)%s%s"):format(actormsg, server.colorname(ci, nil), cip, timemsg, reasonmsg))
        engine.enet_peer_disconnect_later(peer, engine.DISC_KICK)
      else engine.disconnect_client(ci.clientnum, engine.DISC_KICK) end
    end
  end
end

spaghetti.addhook("trykick", function(info)
  if info.skip then return end
  info.skip = true
  local vinfo = info.vinfo
  if not vinfo or not vinfo.connected then return playermsg("No such client.", info.ci) end
  local list = banlists[info.authdesc and "kick" or "openmaster"]
  local client, bypass = list.client, list.bypass
  if not info.trial and not access(info.ci, client) then return playermsg("You lack privileges to kick players.", info.ci) end
  if access(vinfo, bypass) then return playermsg("Cannot kick because the client can bypass the ban.", info.ci) end
  info.cankick = true
  if info.trial then return end
  local _ip, time = engine.getclientip(vinfo.clientnum), 4*60*60
  local hooks = spaghetti.hooks.addban
  if hooks then hooks{ type = "kick", ip = _ip, time = time * 1000, ci = info.ci, reason = info.reason, authname = info.authname, authdesc = info.authdesc } end
  local actormsg = info.authname and
    (info.authdesc and #info.authdesc > 0 and ("%s as '\fs\f5%s\fr' [\fs\f0%s\fr]"):format(server.colorname(info.ci, nil), info.authname, info.authdesc)
      or ("%s as '\fs\f5%s\fr'"):format(server.colorname(info.ci, nil), info.authname))
    or server.colorname(info.ci, nil)
  kickmask(ip.ip(engine.ENET_NET_TO_HOST_32(_ip)), bypass, info.ci, "for 4 hours", info.reason and #info.reason > 0 and info.reason, actormsg)
end)

--client commands

commands.add("banenum", function(info)
  local name = info.args:match"^[^ ]*"
  name = name == "" and "kick" or name
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
end, "#banenum [list=kick]")

commands.add("banlists", function(info) playermsg(table.concat(map.pl(I, banlists), " "), info.ci) end)

local timespec = { d = { m = 60*60*24, n = "days" }, h = { m = 60*60, n = "hours" }, m = { m = 60, n = "minutes" } }
timespec.D, timespec.H, timespec.M = timespec.d, timespec.h, timespec.m
local function kickban(info)
  local force, who, name, time, mult, msg = info.args:match("^(!?)([%d%./]+) *([^ %d]*) *(%d*)([DdHhMm]?) *(.-) *$")
  local cn, _ip = tonumber(who), ip.ip(who or "")
  force, name, msg = force == "!", name == "" and "kick" or name, msg ~= "" and msg or nil
  if (not cn and not _ip) or (not _ip and force) or not time or (time ~= "" and not timespec[mult]) then return playermsg("Bad format", info.ci) end
  if time == "" then time, msg = 1/0, mult ~= "" and msg and mult .. " " .. msg or msg
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
  local toolong = time > 4*60*60 and not access(info.ci, list.full)
  local ok, overlap = module.ban(name, _ip, msg, toolong and 4*60*60 or time, force)
  if not ok then
    if overlap.matcher == _ip then overlap = "is already present"
    elseif overlap.matcher then overlap = "is contained by " .. tostring(overlap.matcher)
    else overlap = "contains other ranges" end
    return playermsg("Cannot add ban because range " .. overlap, info.ci)
  end
  kickmask(_ip, list.bypass, info.ci, toolong and "for 4 hours" or (time == 1/0 and "forever" or "for " .. (time / timespec[mult].m) .. ' ' .. timespec[mult].n), msg)
  playermsg(toolong and "Ban added (4 hours only as you lack full privileges)." or "Ban added.", info.ci)
end
local help = "#ban <cn|[!]range> [list=kick] <time> [reason]\nTime format: #d|#h|#m\nIf !forced, coalesces present ranges, or updates the message/expiration"
commands.add("kick", kickban, help)
commands.add("ban", kickban, help)

commands.add("bandel", function(info)
  local force, who, name = info.args:match("^(!?)([%d%./]+) *([^ ]*) *$")
  local ip = ip.ip(who or "")
  name = name == "" and "kick" or name
  local list = banlists[name]
  if not ip then return playermsg("Bad format.", info.ci) end
  if not list then return playermsg("Ban list not found", info.ci) end
  if not access(info.ci, list.full) then return playermsg("Permission denied.", info.ci) end
  local ok, overlap = module.unban(name, ip, force == '!')
  if not ok then
    if overlap then playermsg("Cannot delete ban because range " .. (overlap.matcher and "is contained by " .. tostring(overlap.matcher) or "contains ranges:\n\t" .. table.concat(map.lp(tostring, overlap), '\n\t')), info.ci)
    else playermsg("Ban does not exist.", info.ci) end
    return
  end
  playermsg("Ban deleted.", info.ci)
end, "#bandel <[!]range> [list=kick]\nIf !forced, removes all included ranges.")

--escaping the ban

spaghetti.addhook("enterlimbo", function(info)
  local bans = module.checkban(info.ci)
  info.ci.extra.bans = bans
  if not next(bans) then return end
  info.ci.extra.limbo.locks.ban = 1/0
  local msg = "You cannot join because you are in a ban list:"
  for list, match in pairs(bans) do
    local tags = list.tags[tostring(match)] or {}
    msg = ("%s\n%s (%s), expiration: %s"):format(msg, list.name, tags.msg or list.msg, unixprint(tags.expire and tags.expire.when or 1/0))
    engine.writelog(("ban: hold %s for %s [%s] (%s)"):format(ip.ip(engine.ENET_NET_TO_HOST_32(engine.getclientpeer(info.ci.clientnum).address.host)), match, list.name, tags.msg or list.msg))
  end
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

