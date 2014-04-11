--[[

  Wrap a packetbuf in a N_CLIENT packet.

]]--

local putf = require"std.putf"

local function wrap(p, client)
  return putf({ p.growth, p.packet.flags }, server.N_CLIENT, type(client) == "number" and client or client.clientnum, p.len, { buf = p.buf:sub(1, p.len) })
end

return wrap
