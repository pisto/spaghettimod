--[[

  Set a callback on when the client appears to have loaded the map

]]--

local module = {}

local fence, iterators = require"std.fence", require"std.iterators"

spaghetti.addhook("changemap", function() for ci in iterators.clients() do ci.extra.maploadedfence = fence(ci) end end)
spaghetti.addhook("connected", function(info) info.ci.extra.maploadedfence = fence(info.ci) end)

spaghetti.addhook("fence", function(info)
  if info.ci.extra.maploadedfence ~= info.fence then return end
  local hooks = spaghetti.hooks.maploaded
  return hooks and spaghetti.hooks.maploaded{ci = info.ci}
end)
