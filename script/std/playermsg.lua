--[[

  Send message to specific players (client numbers, clientinfo structs, iterators)

]]--

local fp, putf = require"utils.fp", require"std.putf"

local function msg(msg, ...)
  local packet = putf({ #msg + 2, r = 1}, server.N_SERVMSG, msg):finalize()
  local map = (type(...) == "function" and fp.map.nf or fp.map.nv)
  map(function(p)
    if type(p) ~= "number" then p = p.clientnum end
    engine.sendpacket(p, 1, packet, -1)
  end, ...)
end

return msg
