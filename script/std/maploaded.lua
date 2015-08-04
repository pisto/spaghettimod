--[[

  Set a callback on when the client appears to have loaded the map

]]--

local module = {}

local fence, iterators = require"std.fence", require"std.iterators"

spaghetti.addhook("changemap", function() for ci in iterators.clients() do ci.extra.maploadedfence = fence(ci) end end)
spaghetti.addhook("connected", function(info) info.ci.extra.maploadedfence = fence(info.ci) end)

spaghetti.addhook("fence", function(info)
  if info.ci.extra.maploadedfence ~= info.fence then return end
  info.ci.extra.maploadedfence = nil
  local hooks = spaghetti.hooks.maploaded
  if hooks then hooks{ci = info.ci} end
end)

return { allloaded = function(spectators)
  for ci in iterators.players() do if ci.extra.maploadedfence then return false end end
  if not spectators then return true end
  for ci in iterators.spectators() do if ci.extra.maploadedfence then return false end end
  if not spectators then return true end
end }
