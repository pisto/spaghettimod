--[[

  Implement standard clanwar utilities: locked mode, no autoteam, specall, autopause on player leave.

]]--

local fp, L, specall, putf, iterators = require"utils.fp", require"utils.lambda", require"std.specall", require"std.putf", require"std.iterators"
local map = fp.map

local hooks
local respawn = false

local toggle
toggle = function(on, ingame)
  if not not on == not not hooks then return end
  if not on then
    map.np(L"spaghetti.removehook(_2)", hooks)
    respawn, hooks = nil
    return
  end
  hooks = {}
  hooks.autoteam = spaghetti.addhook("autoteam", L"_.skip = true")
  hooks.noclients = spaghetti.addhook("noclients", function() toggle(false) end)
  hooks.spectate = spaghetti.addhook("specstate", function(info)
    if info.ci.state.state ~= engine.CS_SPECTATOR or server.gamepaused then return end
    server.forcepaused(true)
    server.sendservmsg("Game paused because " .. server.colorname(info.ci, nil) .. " went into spectator mode")
  end)
  hooks.spectate = spaghetti.addhook("clientdisconnect", function(info)
    if info.ci.state.state == engine.CS_SPECTATOR or server.gamepaused then return end
    server.forcepaused(true)
    server.sendservmsg("Game paused because " .. server.colorname(info.ci, nil) .. " disconnected")
  end)
  hooks.changemap = spaghetti.addhook("changemap", function()
    server.forcepaused(true)
    respawn = true
  end)
  hooks.mastermode = spaghetti.addhook("mastermode", function(info)
    return server.mastermode < server.MM_LOCKED and toggle(false)
  end)
  hooks.npausegame = spaghetti.addhook(server.N_PAUSEGAME, function(info)
    if info.ci.state.state == engine.CS_SPECTATOR or info.skip then return end
    info.skip = true
    server.pausegame(info.val > 0, info.ci)
  end)
  hooks.pausegame = spaghetti.addhook("pausegame", function(info)
    if info.val or not respawn then return end
    respawn = false
    for ci in iterators.players() do server.sendspawn(ci) end
  end)
  specall(ingame)
  if server.mastermode >= server.MM_LOCKED then return end
  server.mastermode = server.MM_LOCKED
  engine.sendpacket(-1, 1, putf({2, r=1}, server.N_MASTERMODE, server.MM_LOCKED):finalize(), -1)
end

return toggle
