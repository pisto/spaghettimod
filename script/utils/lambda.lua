local compiled = {}

return function(code)
  local defined = debug.getinfo(2, "lS")
  defined = ("<lambda %s:%d>"):format(defined.short_src, defined.currentline)
  local cached = compiled[code .. defined]
  if cached then return cached end
  local L, err = load("local _, _1, _2, _3, _4, _5, _6, _7, _8, _9 = ..., ... return " .. code, defined)
  if not L then L, err = load("local _, _1, _2, _3, _4, _5, _6, _7, _8, _9 = ..., ... " .. code, defined) end
  compiled[code .. defined] = assert(L, "Cannot compile lambda: " .. tostring(err))
  return L
end
