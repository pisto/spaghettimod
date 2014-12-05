--[[

  Wrap a packetbuf in a N_CLIENT packet.

]]--

local putf = require"std.putf"

local function wrap(p, client)
  local buf = p.buf
  p.len = 0
  return putf(p, server.N_CLIENT, type(client) == "number" and client or client.clientnum, { uint = #buf }, { buf = buf })
end

return wrap
