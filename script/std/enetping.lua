--[[

  Force ping statistics to be taken from the enet peers.

]]--

local fp, L, iterators = require"utils.fp", require"utils.lambda", require"std.iterators"

spaghetti.addhook(server.N_CLIENTPING, L"_.skip = true")

spaghetti.later(250, function() for ci in iterators.all() do
  local ping = engine.getclientpeer(ci.ownernum).roundTripTime
  ci.ping = ping
  if ci.state.aitype == server.AI_NONE then ci.messages:putint(server.N_CLIENTPING):putint(ping) end
end end, true)
