--[[

  Send auth probes before the client joins the game, handle responses.

]]--

local module = {}

local fp, lambda, limbo, later, uuid, putf, getf = require"utils.fp", require"utils.lambda", require"std.limbo", require"utils.later", require"std.uuid", require"std.putf", require"std.getf"
local map, fold, last, Lr, I = fp.map, fp.fold, fp.last, lambda.Lr, fp.I

local limbotoken, martianhook, reqauths
function module.on(auths, maxauthreqwait, maxauthprocess)

  if limbotoken and not auths then
    limbo.on(false)
    map.nv(spaghetti.removehook, limbotoken, martianhook)
    limbotoken, martianhook, reqauths = nil

  elseif not limbotoken and auths then
    limbo.on(true)
    auths = type(auths) == "string" and {auths} or auths
    local p = last(fold.zi(function(p, _, desc)
      return putf(p, server.N_REQAUTH, desc)
    end, { 100, engine.ENET_PACKET_FLAG_RELIABLE }, auths))
    reqauths = setmetatable({ packet = p:finalize() }, { __gc = function(reqauths)
      local p = reqauths.packet
      p.referenceCount = p.referenceCount - 1
      if p.referenceCount == 0 then engine.enet_packet_destroy(p) end
    end })
    reqauths.packet.referenceCount = 1

    limbotoken = spaghetti.addhook("enterlimbo", function(info)
      engine.sendpacket(info.ci.clientnum, 1, reqauths.packet, -1)
      local ciuuid = info.ci.extra.uuid
      later.later(maxauthreqwait or 500, function()
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
        if not info.ci.extra.preauth then
          info.ci.extra.preauth = map.sp(I, authset)
          local ciuuid = info.ci.extra.uuid
          later.later(maxauthprocess or 1000, function()
            local ci = uuid.find(ciuuid)
            if not ci or not ci.extra.limbo then return end
            ci.extra.limbo.release()
          end)
        end
        info.skip, info.type = false
        info.desc, info.name = map.uv(Lr"_:sub(1, server.MAXSTRLEN)", getf(info.p, "ss"))
        if spaghetti.hooks[server.N_AUTHTRY] then spaghetti.hooks[server.N_AUTHTRY](info) end
        if not info.skip then server.tryauth(info.ci, info.name, info.desc) end
      elseif info.type == server.N_AUTHANS then
        info.skip, info.type = false
        info.desc = info.p:getstring():sub(1, server.MAXSTRLEN)
        info.id = info.p:getint() % 2^32
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
