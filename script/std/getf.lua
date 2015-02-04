--[[

  Format gets, returning variadic output.

]]--

local fp, L = require"utils.fp", require"utils.lambda"
local map = fp.map

local getters = {
  c = L"_:get()",
  i = L"_:getint()",
  f = L"_:getfloat()",
  u = L"_:getuint()",
  s = L"_:getstring()",
  b = L"_:getbuf(_2:match('%d+', _3))"
}

return function(p, format)
  return map.uf(function(fmt, n) return getters[fmt](p, format, n) end, format:gmatch("([^%d])()"))
end
