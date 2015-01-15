--[[

  Send auth probes before the client joins the game, handle responses.

]]--

require"std.limbo"

local auths = {}

local fp, lambda, putf, getf = require"utils.fp", require"utils.lambda", require"std.putf", require"std.getf"
local map, L, Lr = fp.map, lambda.L, lambda.Lr

spaghetti.addhook("enterlimbo", function(info)
  local reqauths
  for _, desc in ipairs(auths) do reqauths = putf(reqauths or { 100, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_REQAUTH, desc) end
  if not reqauths then return end
  engine.sendpacket(info.ci.clientnum, 1, reqauths:finalize(), -1)
  info.ci.extra.limbo.locks.preauth = 500
end)

spaghetti.addhook("martian", function(info)
  if info.skip or info.ci.connected or info.ratelimited then return end
  --copy parsepacket logic
  if info.type == server.N_AUTHTRY then
    if not info.ci.extra.preauth then info.ci.extra.preauth, info.ci.extra.limbo.locks.preauth = map.si(Lr"_2", auths), 1000 end
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

spaghetti.addhook("connected", L"_.ci.extra.preauth = nil")

return auths
