--[[

  Put the client in a limbo as if it wasn't sending N_CONNECT, avoid that the engine disconnects it after 15 seconds or for unexpected messages

]]--

local module = {}

local iterators, fp, lambda = require"std.iterators", require"utils.fp", require"utils.lambda"
local map, I, L, Lr = fp.map, fp.I, lambda.L, lambda.Lr

local later, putf, iterators, uuid = require"utils.later", require"std.putf", require"std.iterators", require"std.uuid"

local function release(limbo)
  if limbo.releasing then return end
  limbo.releasing = true
  local ci = uuid.find(limbo.uuid)
  if not ci then return end
  local connect = putf({ 100, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_CONNECT, limbo.reqnick, limbo.playermodel, limbo.password, limbo.authdesc, limbo.authname)
  connect:resize(connect.len)
  connect.len = 0
  server.parsepacket(ci.clientnum, 1, connect)
end

local connecttoken, timeouttoken, martiantoken, connectedtoken
function module.on(on)

  if not on and connecttoken then
    map.nv(spaghetti.removehook, connecttoken, timeouttoken, martiantoken, connectedtoken)
    connecttoken, timeouttoken, martiantoken, connectedtoken = nil
    map.nf(Lr"_.extra.limbo and _.extra.limbo:release()", iterators.all())

  elseif on and not connecttoken then

    connecttoken = spaghetti.addhook(server.N_CONNECT, function(info)
      local limbo = info.ci.extra.limbo
      if limbo then
        if limbo.releasing then info.ci.extra.limbo = nil
        else info.skip = true end
        return
      end
      if not spaghetti.hooks.enterlimbo then return end
      info.skip = true
      limbo = { uuid = info.ci.extra.uuid, release = release, reqnick = info.text, playermodel = info.playermodel, password = info.password, authdesc = info.authdesc, authname = info.authname }
      local meta, waiters = {}, {}
      meta.__index, meta.__newindex = meta, function(locks, req, value)
        value = value and value > 0 and value or nil
        meta[req], waiters[req] = value, nil, waiters[req] and later.cancel(waiters[req])
        if value then waiters[req] = later.later(value, function() locks[req] = nil end)
        elseif not next(waiters) then limbo:release() end
      end
      limbo.locks = setmetatable({}, meta)
      info.ci.extra.limbo = limbo
      spaghetti.hooks.enterlimbo{ ci = info.ci }
      limbo.locks[{}] = nil   --release immediately if no locks are held
    end)

    --privileges gained during limbo aren't broadcast, synch when the client appears
    connectedtoken = spaghetti.addhook("connected", function(info)
      local p = putf({ 30, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_CURRENTMASTER, server.mastermode)
      map.nf(function(ci) putf(p, ci.clientnum, ci.privilege) end, iterators.minpriv(server.PRIV_MASTER))
      engine.sendpacket(-1, 1, p:putint(-1):finalize(), -1)
    end)

    timeouttoken, martiantoken = spaghetti.addhook("jointimeout", L"_.skip = true"), spaghetti.addhook("martian", L"if not _.ci.connected then _.skip = true end")

  end
end

return module
