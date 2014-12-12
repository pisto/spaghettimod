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
    return ip, { cheaters = cheaters, reporters = reporters, total = requests.total }
  end, pending)
  pending = {}
  lambda(args)
end

function module.cheatercmd(lambda, lambdarate, userrate, usertokens)
  if pending then error("std.abuse.cheatercmd already installed") end
  pending = {}
  commands.add("cheater", function(info)
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
      requests.total, requests.who[who.extra.uuid] = requests.total + 1, true
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
  end, "#cheater [cn|name]: notify operators that there is a cheater. Abuses will be punished!")
end


return module
