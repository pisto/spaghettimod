--[[

  Network fence: send a magic packet and receive a notification when the client has processed it and all the preceding ones.

]]--

local n_client, putf, getf, parsepacket = require"std.n_client", require"std.putf", require"std.getf", require"std.parsepacket"

local magicmodel = 0x70000000

local function checknconnect(info)
  local fence = info.ci.extra.fence
  if info.playermodel < magicmodel or (fence and fence.expected or -1) ~= info.playermodel - magicmodel then return end
  info.skip = true
  fence.fence, fence.expected = fence.expected, fence.expected < fence.sentfence and fence.expected + 1 or nil
  local hooks = spaghetti.hooks.fence
  return hooks and hooks{ ci = info.ci, fence = fence.fence }
end

spaghetti.addhook("martian", function(info)
  if info.type ~= server.N_CONNECT or parsepacket(info) then return end
  checknconnect(info)
end)
spaghetti.addhook(server.N_CONNECT, checknconnect)

return function(ci)
  local fence = ci.extra.fence or {}
  fence.sentfence = fence.sentfence and fence.sentfence + 1 or 0
  engine.sendpacket(ci.clientnum, 1, n_client(putf({20, r=1}, server.N_SWITCHMODEL, magicmodel + fence.sentfence), ci):finalize(), -1)
  server.sendservinfo(ci)
  engine.sendpacket(ci.clientnum, 1, n_client(putf({20, r=1}, server.N_SWITCHMODEL, ci.playermodel), ci):finalize(), -1)
  fence.expected = fence.expected or fence.sentfence
  ci.extra.fence = fence
  return fence.sentfence
end
