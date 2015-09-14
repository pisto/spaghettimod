--[[

  Simplify format of packetbuf.

]]--

local fp, L = require"utils.fp", require"utils.lambda"
local map, U = fp.map, fp.U

local putters = setmetatable({
  [1] = L"_1:put(_2[1])",
  int = L"_1:putint(_2.int)",
  string = L"_1:sendstring(_2.string:sub(1, engine.MAXTRANS))",
  uint = L"_1:putuint(_2.uint)",
  float = L"_1:putfloat(_2.float)",
  buf = L"_1:putbuf(_2.buf)"
}, { __index = L"error('Unknown data type ' .. tostring(_))" })

return function(p, ...)
  if type(p) == "table" then
    p[1], p[2] = p[1] or 100, p[2] or 0
    if p.r and p[2] / engine.ENET_PACKET_FLAG_RELIABLE % 2 < 1 then p[2] = p[2] + engine.ENET_PACKET_FLAG_RELIABLE end
    p = engine.packetbuf(U(p))
  end
  map.nv(function(a)
    if type(a) == "string" then p:sendstring(a)
    elseif type(a) == "number" then p:putint(a)
    elseif type(a) == "table" then putters[next(a)](p, a)
    else error("Unknown data type in putf.") end
  end, ...)
  return p
end
