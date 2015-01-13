--[[

  Put the client in a limbo as if it wasn't sending N_CONNECT, avoid that the engine disconnects it after 15 seconds or for unexpected messages

]]--

local module = {}

local iterators, fp, lambda = require"std.iterators", require"utils.fp", require"utils.lambda"
local map, I, L, Lr = fp.map, fp.I, lambda.L, lambda.Lr

local putf, iterators, uuid = require"std.putf", require"std.iterators", require"std.uuid"

local function release(limbo)
  if limbo.releasing then return end
  limbo.releasing = true
  local ci = uuid.find(limbo.uuid)
  if not ci then return end
  local connect = putf({ 100, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_CONNECT, limbo.reqnick, limbo.playermodel, limbo.password, limbo.authdesc, limbo.authname)
  connect:resize(connect.len)
  connect.len = 0
  ci.extra.limbo = false
  server.parsepacket(ci.clientnum, 1, connect)
end

spaghetti.addhook(server.N_CONNECT, function(info)
  local limbo = info.ci.extra.limbo
  if limbo then info.skip = true return
  elseif limbo == false then return end
  if not spaghetti.hooks.enterlimbo then info.ci.extra.limbo = false return end
  info.skip = true
  limbo = { uuid = info.ci.extra.uuid, release = release, reqnick = info.text, playermodel = info.playermodel, password = info.password, authdesc = info.authdesc, authname = info.authname }
  local meta, waiters = {}, {}
  meta.__index, meta.__newindex = meta, function(locks, req, value)
    value = value and value > 0 and value or nil
    meta[req], waiters[req] = value, nil, waiters[req] and spaghetti.cancel(waiters[req])
    if value then waiters[req] = spaghetti.later(value, function() locks[req] = nil end)
    elseif not next(waiters) then limbo:release() end
  end
  limbo.locks = setmetatable({}, meta)
  info.ci.extra.limbo = limbo
  engine.sendpacket(info.ci.clientnum, 1, putf({ 1, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_WELCOME):finalize(), -1)
  spaghetti.hooks.enterlimbo{ ci = info.ci }
  limbo.locks[{}] = nil   --release immediately if no locks are held
  if info.ci.extra.limbo then engine.writelog(("limbo: requested nick %s (%d)"):format(engine.filtertext(info.text):sub(1, server.MAXNAMELEN), info.ci.clientnum)) end
end)

--privileges gained during limbo aren't broadcast, synch when the client appears
spaghetti.addhook("connected", function(info)
  local p = putf({ 30, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_CURRENTMASTER, server.mastermode)
  for ci in iterators.minpriv(server.PRIV_MASTER) do putf(p, ci.clientnum, ci.privilege) end
  engine.sendpacket(-1, 1, p:putint(-1):finalize(), -1)
end)

spaghetti.addhook("jointimeout", L"_.skip = true")
spaghetti.addhook("martian", function(info)
  if info.ci.connected or info.skip then return end
  info.skip = true
  local p = info.p
  p.len = p.maxlen
end)
