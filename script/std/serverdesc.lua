--[[

  Update serverdesc dynamically.

]]--

local L, iterators, parsepacket = require"utils.lambda", require"std.iterators", require"std.parsepacket"

spaghetti.addhook("martian", function(info)
  if info.skip or not info.ci.connected or info.type ~= server.N_CONNECT or parsepacket(info) then return end
  info.skip = true
end)
spaghetti.addhook("connected", L"server.sendservinfo(_.ci)")

return function(serverdesc)
  cs.serverdesc = serverdesc
  for ci in iterators.clients() do server.sendservinfo(ci) end
end
