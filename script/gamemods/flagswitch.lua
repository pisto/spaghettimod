--[[

  One of my very first hacks. Switch flags in ctf/protect modes.
  Unfortunately clients won't see the right flag colours.

]]--

local fp = require"utils.fp"
local map, range = fp.map, fp.range

local module = {}

local servmodehook
function module.on(state)
  if servmodehook then spaghetti.removehook(servmodehook) servmodehook = nil end
  if not state then return end
  servmodehook = spaghetti.addhook("servmodesetup", function()
    if not server.m_ctf or server.m_hold then return end
    map.nf(function(i)
      local ment = server.ments[i]
      if ment.type ~= server.FLAG then return end
      ment.attr2 = ment.attr2 == 1 and 2 or ment.attr2 == 2 and 1 or ment.attr2
    end, range.z(0, server.ments:length() - 1))
  end)
end

return module
