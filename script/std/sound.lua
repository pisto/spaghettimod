--[[

  Inject a player sound.

]]--

local n_client, putf = require"std.n_client", require"std.putf"

return function(ci, sound, unreliable)
  engine.sendpacket(-1, 1, n_client(putf({4, unreliable and 0 or engine.ENET_PACKET_FLAG_RELIABLE}, server.N_SOUND, sound), ci):finalize(), -1)
end
