--[[

  Issue an event "notalive" when a client dies or spectates.

]]--

local function issue(ci)
  local hooks = spaghetti.hooks.notalive
  return hooks and hooks{ ci = ci }
end

spaghetti.addhook("suicide", function(info) issue(info.ci) end)
spaghetti.addhook("specstate", function(info) return info.ci.state.state == engine.CS_SPECTATOR and issue(info.ci) end)
spaghetti.addhook("damaged", function(info) return info.target.state.state == engine.CS_DEAD and issue(info.target) end)
