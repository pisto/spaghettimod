--[[

  Force respawn, avoid hiding at top of scoreboard.

]]--

local fp, L, iterators = require"utils.fp", require"utils.lambda", require"std.iterators"
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
  if autospawntime == 0 then server.sendspawn(ci)
  else ci.extra.autospawn = spaghetti.latergame(autospawntime, function()
    ci.extra.autospawn = nil
    server.sendspawn(ci)
  end) end
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
