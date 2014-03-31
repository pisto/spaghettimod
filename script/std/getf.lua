--[[

  Format gets, returning variadic output.

]]--

local fp, lambda = require"utils.fp", require"utils.lambda"
local map, Lr = fp.map, lambda.Lr

local getters = {
  c = Lr"_:get()",
  i = Lr"_:getint()",
  f = Lr"_:getfloat()",
  u = Lr"_:getuint()",
  s = Lr"_:getstring()",
  b = Lr"_:getbuf(_2:match('%d+', _3))"
}

return function(p, format)
  return map.uf(function(fmt, n) return getters[fmt](p, format, n) end, format:gmatch("([^%d])()"))
end
