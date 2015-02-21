--[[

  Ridiculous hack to force the player to spawn at a specified location: delete all player spawns, spawn, restore them.

]]--

local fp, L, ents, putf, n_client = require"utils.fp", require"utils.lambda", require"std.ents", require"std.putf", require"std.n_client"
local map, pick, range = fp.map, fp.pick, fp.range

return function(ci, o, yaw)
  local forcedstart, ments, tag = ents.newent(), server.ments, ((server.m_ctf and not server.m_hold) or server.m_protect or server.m_collect) and server.ctfteamflag(ci.team) or 0
  local spawnpoints = map.sf(L"_", pick.zf(function(i)
    return ments[i].type == server.PLAYERSTART and ments[i].attr2 == tag
  end, range.z(0, server.ments:length() - 1)))
  local p = { 100, r = 1}
  for i in pairs(spawnpoints) do   --suppress all spawnpoints
    p = putf(p, server.N_EDITENT, i, 0, 0, 0, server.NOTUSED, 0, 0, 0, 0, 0)
  end
  --inject position and yaw
  p = putf(p, server.N_EDITENT, forcedstart, o.x * server.DMF, o.y * server.DMF, o.z * server.DMF, server.PLAYERSTART, yaw, tag, 0, 0, 0)
  --send spawn
  p = putf(p, server.N_SPAWNSTATE, ci.clientnum)
  server.sendstate(ci.state, p)
  --delete fake spawnpoint
  p = putf(p, server.N_EDITENT, forcedstart, 0, 0, 0, server.NOTUSED, 0, 0, 0, 0, 0)
  --restore all spawnpoints
  for i in pairs(spawnpoints) do   --restore them
    local ment = server.ments[i]
    local o = ment.o
    p = putf(p, server.N_EDITENT, i, o.x * server.DMF, o.y * server.DMF, o.z * server.DMF, server.PLAYERSTART, ment.attr1, tag, 0, 0, 0)
  end
  engine.sendpacket(ci.ownernum, 1, n_client(p, ci):finalize(), -1)
  --send notice to other clients too
  p = putf({ 30, r = 1}, server.N_SPAWN)
  server.sendstate(ci.state, p)
  engine.sendpacket(-1, 1, n_client(p, ci):finalize(), ci.ownernum)
end
