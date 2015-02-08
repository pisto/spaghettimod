--[[

  Force ping statistics to be taken from the enet peers.

]]--

local fp, L, putf, iterators, n_client = require"utils.fp", require"utils.lambda", require"std.putf", require"std.iterators", require"std.n_client"

spaghetti.addhook(server.N_CLIENTPING, L"_.skip = true")
spaghetti.addhook(server.N_PING, L"_.skip = true")

spaghetti.later(250, function() for ci in iterators.all() do
  local ping = engine.getclientpeer(ci.ownernum).roundTripTime
  ci.ping = ping
  if ci.state.aitype == server.AI_NONE then
    engine.sendpacket(-1, 1, n_client(putf({}, server.N_CLIENTPING, ping), ci):finalize(), -1)
  end
end end, true)
