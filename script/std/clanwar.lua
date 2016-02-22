--[[

  Implement standard clanwar utilities: locked mode, no autoteam, specall, autopause on player leave.

]]--

local fp, L, specall, putf, iterators, saveteam, setteam = require"utils.fp", require"utils.lambda", require"std.specall", require"std.putf", require"std.iterators", require"std.saveteam", require"std.setteam"
local map = fp.map

local hooks
local respawn = false

local function setlocked()
  server.mastermode = server.MM_LOCKED
  engine.sendpacket(-1, 1, putf({2, r=1}, server.N_MASTERMODE, server.MM_LOCKED):finalize(), -1)
end

local function vsmode()
  return server.m_collect or server.m_ctf and (server.m_protect or not server.m_hold)
end

local function defaultteam(ci)
  if not server.m_teammode then return "" end
  return vsmode() and "good" or "?"
end

local function validteam(ci)
  if not server.m_teammode then return ci.team == "" end
  if vsmode() then return ci.team == "good" or ci.team == "evil" end
  return ci.team ~= ""
end

local module, toggle = {}
toggle = function(_, on, ingame)
  if not not on == not not hooks then return end
  if not on then
    map.np(L"spaghetti.removehook(_2)", hooks)
    saveteam(false)
    respawn, hooks = nil
    return
  end
  specall(ingame)
  saveteam(true)
  hooks = {}
  hooks.autoteam = spaghetti.addhook("autoteam", function(info)
    if info.ci then
      if info.skip then return end
      info.ci.team = defaultteam(info.ci)
    else
      server.addteaminfo("good") server.addteaminfo("evil")
      for ci in iterators.all() do if not validteam(ci) then setteam(ci, defaultteam(ci), -1, true) end end
      setteam.sync()
    end
    info.skip = true
  end)
  hooks.spectate = spaghetti.addhook("specstate", function(info)
    if module.autopause == false or info.ci.state.state ~= engine.CS_SPECTATOR or server.gamepaused then return end
    server.forcepaused(true)
    server.sendservmsg("Game paused because " .. server.colorname(info.ci, nil) .. " went into spectator mode")
  end)
  hooks.spectate = spaghetti.addhook("clientdisconnect", function(info)
    if module.autopause == false or not info.ci.connected or info.ci.state.state == engine.CS_SPECTATOR or server.gamepaused then return end
    server.forcepaused(true)
    server.sendservmsg("Game paused because " .. server.colorname(info.ci, nil) .. " disconnected")
  end)
  hooks.changemap = spaghetti.addhook("changemap", function()
    server.forcepaused(true)
    respawn = true
  end)
  hooks.pausegame = spaghetti.addhook("pausegame", function(info)
    if info.val or not respawn then return end
    respawn = false
    for ci in iterators.players() do server.sendspawn(ci) end
  end)
  hooks.checkpausegame = spaghetti.addhook("checkpausegame", L"_.skip = true")
  hooks.checkmastermode = spaghetti.addhook("checkmastermode", L"_.skip = true")
  if server.mastermode >= server.MM_LOCKED then return end
  setlocked()
end

return setmetatable(module, { __call = toggle})
