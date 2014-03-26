--[[

  Put the client in a limbo as if it wasn't sending N_CONNECT, avoid that the engine disconnects it after 15 seconds or for unexpected messages

]]--

local module = {}

local iterators, fp, lambda = require"std.iterators", require"utils.fp", require"utils.lambda"
local map, L, Lr = fp.map, lambda.L, lambda.Lr

local connecttoken, timeouttoken, martiantoken
function module.on(on)
  if not on and connecttoken then
    spaghetti.removehook(server.N_CONNECT, connecttoken)
    spaghetti.removehook("jointimeout", timeouttoken)
    spaghetti.removehook("martian", martiantoken)
    connecttoken, timeouttoken, martiantoken = nil
    map.nf(Lr"_.extra.limbo and _.extra.limbo.release()", iterators.all())
    return
  end

  connecttoken = spaghetti.addhook(server.N_CONNECT, function(info)
    if info.ci.extra.limbo then return end
    local limbo = { ci = info.ci, reqnick = info.text, playermodel = info.playermodel, password = info.password, authdesc = info.authdesc, authname = info.authname }
    function limbo.release()
      local connect = engine.packetbuf(100, engine.ENET_PACKET_FLAG_RELIABLE)
      connect:putint(server.N_CONNECT):sendstring(limbo.reqnick):putint(limbo.playermodel):sendstring(limbo.password):sendstring(limbo.authdesc):sendstring(limbo.authname)
      connect:resize(connect.len)
      connect.len = 0
      server.parsepacket(limbo.ci.clientnum, 1, connect)
    end
    info.ci.extra.limbo = limbo
    info.skip = true
    if spaghetti.hooks.enterlimbo then spaghetti.hooks.enterlimbo{ ci = info.ci } end
  end)

  timeouttoken, martiantoken = spaghetti.addhook("jointimeout", L"_.skip = true"), spaghetti.addhook("martian", L"if not _.ci.connected then _.skip = true end")

end

return module
