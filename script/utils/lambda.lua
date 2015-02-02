local compiled = {}

return function(code)
  local cached = compiled[code]
  if cached then return cached end
  local L, err = load("local _, _1, _2, _3, _4, _5, _6, _7, _8, _9 = ..., ... return " .. code, "<lambda>")
  if not L then L, err = load("local _, _1, _2, _3, _4, _5, _6, _7, _8, _9 = ..., ... " .. code, "<lambda>") end
  compiled[code] = assert(L, "Cannot compile lambda: " .. tostring(err))
  return L
end
