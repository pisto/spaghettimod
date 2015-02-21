--[[

  One of my very first hacks. Switch flags in ctf/protect modes.

]]--

local fp, L, ents = require"utils.fp", require"utils.lambda", require"std.ents"
local map, range = fp.map, fp.range

local module, hooks = {}, {}

function module.on(state)
  map.np(L"spaghetti.removehook(_2)", hooks)
  hooks = {}
  if not state then return end
  hooks.ents = spaghetti.addhook("entsloaded", function()
    if not server.m_ctf or server.m_hold then return end
    for i, sent, ment in ents.enum(server.PLAYERSTART) do if ment.attr2 == 1 or ment.attr2 == 2 then
      ents.editent(i, server.PLAYERSTART, ment.o, ment.attr1, 3 - ment.attr2)
    end end
  end)
  hooks.connect = spaghetti.addhook("connected", function(info)
    return ents.active() and server.m_ctf and not server.m_hold and info.ci.state.state ~= engine.CS_SPECTATOR and server.sendspawn(info.ci)
  end)
  
end

return module
