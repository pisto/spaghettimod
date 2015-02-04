--[[

  Save team across reconnections.

]]--

local fp, L, putf = require"utils.fp", require"utils.lambda", require"std.putf"
local map = fp.map

local module = {}
local hooks = {}

function module.on(state)
  map.np(L"spaghetti.removehook(_2)", hooks) hooks = {}
  if not state then return end

  hooks.savegamestate = spaghetti.addhook("savegamestate", L"_.sc.extra.team = _.ci.team")
  hooks.restoregamestate = spaghetti.addhook("restoregamestate", function(info)
    info.ci.team, info.ci.extra.saveteam = info.sc.extra.team, true
    engine.sendpacket(info.ci.ownernum, 1, putf({ 10, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_SETTEAM, info.ci.clientnum, info.ci.team, -1):finalize(), -1)
  end)

end

return module
