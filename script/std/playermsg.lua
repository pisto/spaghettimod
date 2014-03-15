--[[

  Send message to specific players (client numbers, clientinfo structs, iterators)

]]--

local fp = require"utils.fp"

local function msg(msg, ...)
  local packet = engine.packetbuf(#msg + 1, engine.ENET_PACKET_FLAG_RELIABLE):putint(server.N_SERVMSG):sendstring(msg):finalize()
  local map = (type(...) == "function" and fp.map.nf or fp.map.nv)
  map(function(p)
    if type(p) ~= "number" then p = p.clientnum end
    engine.sendpacket(p, 1, packet, -1)
  end, ...)
end

return msg
