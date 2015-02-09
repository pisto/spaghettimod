--[[

  Update serverdesc dynamically.

]]--

local L, iterators, getf = require"utils.lambda", require"std.iterators", require"std.getf"

spaghetti.addhook("martian", function(info)
  if info.skip or not info.ci.connected or info.type ~= server.N_CONNECT then return end
  getf(info.p, "sisss")
  info.skip = not info.p:overread()
end)
spaghetti.addhook("connected", L"server.sendservinfo(_.ci)")

return function(serverdesc)
  cs.serverdesc = serverdesc
  for ci in iterators.clients() do server.sendservinfo(ci) end
end
