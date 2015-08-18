--[[

  Force respawn, avoid hiding at top of scoreboard.

]]--

local fp, L, iterators, spawn = require"utils.fp", require"utils.lambda", require"std.iterators", require"std.spawn"
local map = fp.map

local hooks, autospawntime

local function cleanclient(info)
  local ci = info.ci or info
  if not ci.extra.autospawn then return end
  spaghetti.cancel(ci.extra.autospawn)
  ci.extra.autospawn = nil
end
local function cleanup()
  map.np(L"spaghetti.removehook(_2)", hooks)
  hooks, autospawntime = nil
  for ci in iterators.all() do cleanclient(ci) end
end

local function doautospawn(info)
  local ci = info.ci or info
  if ci.state.state ~= engine.CS_DEAD or not autospawntime then return end
  ci.extra.autospawn = spaghetti.latergame(math.max(autospawntime, 1), function() --XXX crash if spawning immediately, why?
    ci.extra.autospawn = nil
    spawn(ci)
  end)
end


require"std.notalive"

return function(time)
  if hooks then cleanup() end
  if not time then return end
  assert(time >= 0, "invalid autospawn value")
  hooks = {}
  hooks.spawned = spaghetti.addhook("spawned", cleanclient)
  hooks.specstate = spaghetti.addhook("specstate", function(info)
    if info.ci.state.state == engine.CS_SPECTATOR then cleanclient(info)
    else doautospawn(info.ci) end
  end)
  hooks.disconnect = spaghetti.addhook("clientdisconnect", cleanclient)
  hooks.notalive = spaghetti.addhook("notalive", doautospawn)
  autospawntime = time
end
