--[[

  Rugby mode: pass the flag you hold to a teammate by shooting at him with rifle

]]--

local module = {}

local dodamagehook
function module.on(state)
  if dodamagehook then spaghetti.removehook(dodamagehook) dodamagehook = nil end
  if not state then return end
  dodamagehook = spaghetti.addhook("dodamage", function(info)
    if info.skip or not server.m_ctf or info.target.team ~= info.actor.team or info.gun ~= server.GUN_RIFLE then return end
    local flags, actorflags = server.ctfmode.flags, {}
    for i = 0, flags:length() - 1 do if flags[i].owner == info.actor.clientnum then actorflags[i] = true end end
    if not next(actorflags) then return end
    info.skip = true
    for flag in pairs(actorflags) do
      server.ctfmode:returnflag(flag, 0)
      server.ctfmode:takeflag(info.target, flag, flags[flag].version)
    end
    local hooks = spaghetti.hooks.rugbypass
    if hooks then hooks{ actor = info.actor, target = info.target } end
  end)
end

return module
