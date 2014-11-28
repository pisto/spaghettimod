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
    local flags, actorflag = server.ctfmode.flags, {}
    for i = 0, flags:length() - 1 do if flags[i].owner == info.actor.clientnum then actorflag[i] = true end end
    actorflag = next(actorflag)
    if not actorflag then return end
    info.skip = true
    server.ctfmode:returnflag(actorflag, 0)
    server.ctfmode:takeflag(info.target, actorflag, flags[actorflag].version)
  end)
end

return module
