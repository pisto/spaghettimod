--[[

  Wrap a packetbuf in a N_CLIENT packet.

]]--

local function wrap(p, client)
  --need to save the growth, finalize the packet and reset it, because ENetPacket.data has size maxlen, not len
  local growth = p.growth
  local wrapped = engine.packetbuf(growth, p.packet.flags)
  wrapped:putint(server.N_CLIENT):putint(type(client) == "number" and client or client.clientnum):putint(p.len):putbuf(p:finalize().data)
  p.growth = growth
  return wrapped
end

return wrap
