--[[

  Spectate all players, or excluding a subset.

]]--

local fp, L, iterators = require"utils.fp", require"utils.lambda", require"std.iterators"
local map = fp.map

return function(excluding)
  local excludingcn = map.sp(L"type(_) == 'number' and _ or _.clientnum", excluding or {})
  for p in iterators.clients() do (excludingcn[p.clientnum] and server.unspectate or server.forcespectator)(p) end
end
