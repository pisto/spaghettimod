--[[

  Send auth probes before the client joins the game, handle responses.

]]--

local module = {}

local fp, lambda, limbo, later, uuid = require"utils.fp", require"utils.lambda", require"std.limbo", require"utils.later", require"std.uuid"
local map, Lr, I = fp.map, lambda.Lr, fp.I

local limbotoken, martianhook, reqauths
function module.on(auths, maxlimbo)

  if limbotoken and not auths then
    limbo.on(false)
    map.nv(spaghetti.removehook, limbotoken, martianhook)
    reqauths.referenceCount = reqauths.referenceCount - 1
    if reqauths.referenceCount == 0 then engine.enet_packet_destroy(reqauths) end
    limbotoken, martianhook, reqauths = nil

  elseif not limbotoken and auths then
    limbo.on(true)
    local p = engine.packetbuf(100, engine.ENET_PACKET_FLAG_RELIABLE)
    auths = type(auths) == "string" and {auths} or auths
    map.ni(function(_, desc) p:putint(server.N_REQAUTH):sendstring(desc) end, auths)
    reqauths = p:finalize()
    reqauths.referenceCount = 1

    limbotoken = spaghetti.addhook("enterlimbo", function(info)
      engine.sendpacket(info.ci.clientnum, 1, reqauths, -1)
      local ciuuid = info.ci.extra.uuid
      later.later(maxlimbo or 500, function()
        local ci = uuid.find(ciuuid)
        if not ci or not ci.extra.limbo or info.ci.extra.preauth then return end
        ci.extra.limbo.release()
      end)
    end)

    local authset = map.si(Lr"_2", auths)
    martianhook = spaghetti.addhook("martian", function(info)
      if info.ci.connected or info.ratelimited then return end
      --copy parsepacket logic
      if info.type == server.N_AUTHTRY then
        info.ci.extra.preauth = info.ci.extra.preauth or map.sp(I, authset)
        info.skip, info.type = false
        info.desc = info.p:getstring():sub(1, server.MAXSTRLEN)
        info.name = info.p:getstring():sub(1, server.MAXSTRLEN)
        if spaghetti.hooks[server.N_AUTHTRY] then spaghetti.hooks[server.N_AUTHTRY](info) end
        if not info.skip then server.tryauth(info.ci, info.name, info.desc) end
      elseif info.type == server.N_AUTHANS then
        info.skip, info.type = false
        info.desc = info.p:getstring():sub(1, server.MAXSTRLEN)
        info.id = info.p:getint()
        info.ans = info.p:getstring():sub(1, server.MAXSTRLEN)
        if spaghetti.hooks[server.N_AUTHANS] then spaghetti.hooks[server.N_AUTHANS](info) end
        if not info.skip then server.answerchallenge(info.ci, info.id, info.ans, info.desc) end
        info.ci.extra.preauth[info.desc] = nil
        if not next(info.ci.extra.preauth) then info.ci.extra.preauth = nil info.ci.extra.limbo.release() end
      end
      info.skip, info.desc, info.id, info.ans, info.name = true
    end)

  end  
end

return module
