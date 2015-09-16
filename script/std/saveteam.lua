--[[

  Save team across reconnections.

]]--

local fp, L, putf, setteam = require"utils.fp", require"utils.lambda", require"std.putf", require"std.setteam"
local map = fp.map

local hooks = {}

return function(state)
  map.np(L"spaghetti.removehook(_2)", hooks) hooks = {}
  if not state then return end
  hooks.savegamestate = spaghetti.addhook("savegamestate", L"_.sc.extra.team = server.m_teammode and _.ci.team or nil")
  hooks.autoteam = spaghetti.addhook("autoteam", function(info)
    if not info.ci or info.skip then return end
    local sc = server.findscore(info.ci, false)
    if not sc or not sc.extra.team then return end
    info.skip = true
    info.ci.team = sc.extra.team
    return info.ci.team ~= "" and server.addteaminfo(info.ci.team)
  end)
end
