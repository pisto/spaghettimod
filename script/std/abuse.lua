--[[

  Miscellaneous anti abuse tools.

]]--

local playermsg, module = require"std.playermsg", {}

--packet rate limit

local fp, lambda, tb = require"utils.fp", require"utils.lambda", require"utils.tokenbucket"
local noop, I, map, L, U = fp.noop, fp.I, fp.map, lambda.L, fp.U

function module.ratelimit(packets, rate, maxtokens, selector)
  if type(packets) == "number" then packets = {packets} end
  packets = {U(packets)}
  packets.hooks = map.m(function(_, p)
    return p, spaghetti.addhook(p, function(info)
      local skip, desc, client
      if selector then
        skip, desc, client = selector(info)
        if skip then return end
      end
      local extra = (client or info.ci).extra
      extra.ratelimit = extra.ratelimit or {}
      extra.ratelimit[packets] = extra.ratelimit[packets] or tb(rate, maxtokens)
      if extra.ratelimit[packets]() then return end
      info.skip, info.ratelimited = true, (desc or true)

    end, true) end
  , packets)
  return packets
end

module.delratelimit = spaghetti.removehook


--Sound that can be sent by vanilla clients

local clientsounds = map.sv(I, server.S_JUMP, server.S_LAND, server.S_NOAMMO)
function module.clientsound(s)
  return (server.m_capture and s == server.S_ITEMAMMO) or clientsounds[s]
end


--Reconnect spam
local ipextra = require"std.ipextra"
local connecthook
function module.reconnectspam(rate, maxtokens)
  connecthook = nil, connecthook and spaghetti.removehook(connecthook)
  if not rate then return end
  connecthook = spaghetti.addhook("enetevent", function(info)
    if info.event.type ~= engine.ENET_EVENT_TYPE_CONNECT then return end
    local ip = engine.ENET_NET_TO_HOST_32(info.event.peer.address.host)
    local ipextra = ipextra.find(ip)
    local limiter = ipextra.reconnectlimit or { tb = tb(rate, maxtokens) }
    if not limiter.tb() then
      info.skip = true
      engine.enet_peer_disconnect_now(info.event.peer, engine.DISC_OVERFLOW)
      if not limiter.logged then
        engine.writelog("Reconnect spam from " .. tostring(require"utils.ip".ip(ip)))
        limiter.logged = true
      end
    else limiter.logged = false end
    ipextra.reconnectlimit = limiter
  end)
end

--#cheater command
local commands, iterators, uuid = require"std.commands", require"std.iterators", require"std.uuid"
local pending, laterlambda

local function getip(ci)
  return engine.ENET_NET_TO_HOST_32(engine.getclientip(ci.clientnum))
end

local function flush(lambda)
  local args = map.mp(function(ip, requests)
    local cheaters = map.sf(I, iterators.select(function(ci) return getip(ci) == ip end))
    local reporters = map.sp(uuid.find, requests.who)
    return ip, { cheaters = cheaters, reporters = reporters, total = requests.total, ai = requests.ai }
  end, pending)
  pending = {}
  lambda(args)
end

function module.cheatercmd(lambda, lambdarate, userrate, usertokens)
  if pending then error("std.abuse.cheatercmd already installed") end
  pending = {}
  commands.add("cheater", function(info)
    if info.args == "" then playermsg("Please specify who is cheating", info.ci) return end
    local cn, who = tonumber(info.args)
    if cn then for ci in iterators.all() do
      if ci.clientnum == cn then who = ci break end
    end end
    if not who then
      for ci in iterators.clients() do
        if ci.name == info.args then
          if who then playermsg(info.args .. " is ambiguous, please use a client number (/showclientnum 1)", info.ci) return end
          who = ci
        end
      end
    end
    if not who then playermsg("Cannot find specified client", info.ci) return end
    if who.clientnum == info.ci.clientnum then playermsg("Cannot report yourself", info.ci) return end
    if who.privilege > server.PRIV_MASTER then playermsg("Cannot report an auth user", info.ci) return end
    local cmdrate = info.ci.extra.ipextra.cheatercmd or tb(userrate, usertokens)
    info.ci.extra.ipextra.cheatercmd = cmdrate
    if not cmdrate() then playermsg("You are using this command too often.", info.ci) return end
    local ip = getip(who.state.aitype == server.AI_NONE and who or engine.getclientinfo(who.ownernum))
    local requests = pending[ip] or { total = 0, who = {} }
    if not requests.who[who.extra.uuid] then
      requests.total, requests.who[who.extra.uuid], requests.ai = requests.total + 1, true, requests.ai or who.state.aitype ~= server.AI_NONE or nil
      pending[ip] = requests
    end
    playermsg("Ok, someone will look into this soon, please be patient.", info.ci)
    if laterlambda then return end
    laterlambda = spaghetti.later(10000, function()    --gather initial burst
      laterlambda = nil
      flush(lambda)
      laterlambda = spaghetti.later(lambdarate, function()
        local me
        me, laterlambda = laterlambda
        if not next(pending) then spaghetti.cancel(me) return end
        flush(lambda)
        laterlambda = me
      end, true)
    end)
  end, "#cheater <cn|name>: notify operators that there is a cheater. Abuses will be punished!")
end

local ip = require"utils.ip"
local function ciip(ci)
  return engine.ENET_NET_TO_HOST_32(engine.getclientpeer(ci.clientnum).address.host)
end

local timespec = { d = { m = 60*60*24, n = "days" }, h = { m = 60*60, n = "hours" }, m = { m = 60, n = "minutes" } }
timespec.D, timespec.H, timespec.M = timespec.d, timespec.h, timespec.m
commands.add("mute", function(info)
  if info.ci.privilege < server.PRIV_AUTH then playermsg("Insufficient privileges", info.ci) return end
  local cn, time, mult = info.args:match("^(%d+) *(%d*)([DdHhMm]?) *$")
  if not cn or (mult ~= "" and time == "") or time == '0' then playermsg("Invalid format.", info.ci) return end
  local who = server.getinfo(cn or -1)
  if not who then playermsg("Cannot find specified client", info.ci) return end
  time = time == "" and 30 * 60 or timespec[mult].m * time
  local toomuch = false
  if info.ci.privilege < server.PRIV_ADMIN and time > 30 * 60 then toomuch, time = true, 30 * 60 end
  who = server.getinfo(who.ownernum)
  local muted = who.extra.ipextra.muted
  if muted then spaghetti.cancel(muted) end
  local _ip = ciip(who)
  who.extra.ipextra.muted = spaghetti.later(time * 1000, function()
    local extra = ipextra.find(_ip)
    if extra then
      extra.muted = nil
      engine.writelog("mute: expire " .. tostring(ip.ip(_ip)))
    end
  end)
  playermsg("Muted cn " .. who.clientnum .. (toomuch and " (for 30 minutes as you lack privileges)" or "."), info.ci)
  engine.writelog("mute: muted " .. tostring(ip.ip(_ip)))
end, "#mute <cn> [time=30m]: mute a client for a specified time, or default 30 minutes")
commands.add("unmute", function(info)
  if info.ci.privilege < server.PRIV_AUTH then playermsg("Insufficient privileges", info.ci) return end
  local cn, extra, _ip = tonumber(info.args)
  if not cn then
    _ip = ip.ip(info.args)
    if _ip then extra = ipextra.find(_ip.ip) end
  else
    local who = server.getinfo(cn)
    if who then _ip, extra = ip.ip(ciip(who)), who.extra.ipextra end
  end
  if not extra then playermsg("Cannot find specified client.", info.ci) return end
  local muted = extra.muted
  if muted then
    spaghetti.cancel(muted)
    extra.muted = nil
    engine.writelog("mute: unmuted " .. tostring(_ip))
  end
  playermsg("Unmuted " .. (cn and cn or tostring(_ip)) .. ".", info.ci)
end, "#unmute <cn|ip>: unmute player or ip")
local nobodycares = {
  "Nobody cares."
}
local function blocktext(info)
  if info.skip or not info.ci.extra.ipextra.muted or info.ci.privilege >= server.PRIV_AUTH then return end
  info.skip = true
  playermsg(nobodycares[math.random(#nobodycares)], info.ci)
end
spaghetti.addhook(server.N_TEXT, blocktext)
spaghetti.addhook(server.N_SAYTEAM, blocktext)
spaghetti.addhook(server.N_SWITCHNAME, function(info)
  if info.skip or not info.ci.extra.ipextra.muted or info.ci.privilege >= server.PRIV_AUTH then return end
  info.skip = true
  playermsg("Your name change is not visible because you are muted.", info.ci)
end)
spaghetti.addhook("commands.pm", blocktext, true)

return module
