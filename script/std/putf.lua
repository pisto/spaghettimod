--[[

  Simplify format of packetbuf. 

]]--

local fp = require"utils.fp"
local map, U = fp.map, fp.U

return function(p, ...)
  if type(p) == "table" then p = engine.packetbuf(U(p)) end
  map.nv(function(a)
    if type(a) == "string" then p:sendstring(a)
    elseif type(a) == "number" then p:putint(a)
    elseif type(a) == "table" then
      if a[1] then p:put(a[1])
      elseif a.int then p:putint(a.int)
      elseif a.string then p:sendstring(a.string)
      elseif a.uint then p:putuint(a.uint)
      elseif a.float then p:putfloat(a.float)
      elseif a.buf then p:putbuf(a.buf)
      else error("Unknown data type in putf.") end
    else error("Unknown data type in putf.") end
  end, ...)
  return p
end
