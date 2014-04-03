--[[

  Put the client in a limbo as if it wasn't sending N_CONNECT, avoid that the engine disconnects it after 15 seconds or for unexpected messages

]]--

local module = {}

local iterators, fp, lambda = require"std.iterators", require"utils.fp", require"utils.lambda"
local map, L, Lr = fp.map, lambda.L, lambda.Lr

local putf = require"std.putf"

local connecttoken, timeouttoken, martiantoken
function module.on(on)

  if not on and connecttoken then
    map.nv(spaghetti.removehook, connecttoken, timeouttoken, martiantoken)
    connecttoken, timeouttoken, martiantoken = nil
    map.nf(Lr"_.extra.limbo and _.extra.limbo.release()", iterators.all())

  elseif on and not connecttoken then
    connecttoken = spaghetti.addhook(server.N_CONNECT, function(info)
      local limbo = info.ci.extra.limbo
      if limbo then
        if limbo.releasing then info.ci.extra.limbo = nil
        else info.skip = true end
        return
      end
      limbo = { ci = info.ci, reqnick = info.text, playermodel = info.playermodel, password = info.password, authdesc = info.authdesc, authname = info.authname }
      function limbo.release()
        local connect = putf({ 100, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_CONNECT, limbo.reqnick, limbo.playermodel, limbo.password, limbo.authdesc, limbo.authname)
        connect:resize(connect.len)
        connect.len = 0
        limbo.ci.extra.limbo.releasing = true
        server.parsepacket(limbo.ci.clientnum, 1, connect)
      end
      info.ci.extra.limbo = limbo
      info.skip = true
      if spaghetti.hooks.enterlimbo then spaghetti.hooks.enterlimbo{ ci = info.ci } end
    end)

    timeouttoken, martiantoken = spaghetti.addhook("jointimeout", L"_.skip = true"), spaghetti.addhook("martian", L"if not _.ci.connected then _.skip = true end")

  end
end

return module
