--[[

  Send auth probes before the client joins the game, handle responses.

]]--

local module = {}

local fp, lambda, limbo, uuid, putf, getf, packetgc = require"utils.fp", require"utils.lambda", require"std.limbo", require"std.uuid", require"std.putf", require"std.getf", require"std.packetgc"
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
    reqauths = packetgc(p:finalize())

    limbotoken = spaghetti.addhook("enterlimbo", function(info)
      engine.sendpacket(info.ci.clientnum, 1, reqauths.p, -1)
      info.ci.extra.limbo.locks.preauth = 500
    end)

    local authset = map.si(Lr"_2", auths)
    martianhook = spaghetti.addhook("martian", function(info)
      if info.skip or info.ci.connected or info.ratelimited then return end
      --copy parsepacket logic
      if info.type == server.N_AUTHTRY then
        if not info.ci.extra.preauth then info.ci.extra.preauth, info.ci.extra.limbo.locks.preauth = map.sp(I, authset), 1000 end
        local desc, name = map.uv(Lr"_:sub(1, server.MAXSTRLEN)", getf(info.p, "ss"))
        local hooks, authinfo = spaghetti.hooks[server.N_AUTHTRY], setmetatable({ skip = false, desc = desc, name = name }, { __index = info, __newindex = info })
        if hooks then hooks(authinfo) end
        if not authinfo.skip then server.tryauth(info.ci, authinfo.name, authinfo.desc) end
        info.skip = true
      elseif info.type == server.N_AUTHANS then
        local p = info.p
        local authinfo = setmetatable({ skip = false, desc = p:getstring():sub(1, server.MAXSTRLEN), id = p:getint() % 2^32, ans = p:getstring():sub(1, server.MAXSTRLEN) }, { __index = info, __newindex = info })
        local hooks = spaghetti.hooks[server.N_AUTHANS]
        if hooks then hooks(info) end
        if not authinfo.skip then server.answerchallenge(info.ci, authinfo.id, authinfo.ans, authinfo.desc) end
        info.ci.extra.preauth[authinfo.desc] = nil
        if not next(info.ci.extra.preauth) then
          info.ci.extra.preauth = nil
          local limbo = info.ci.extra.limbo
          if limbo then limbo.locks.preauth = nil end
        end
        info.skip = true
      end
    end, true)

  end  
end

return module
