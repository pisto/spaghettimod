--[[

  Mitigate spawnkilling by modifying the yaw of spawns, pointing to the closest enemy.

]]--

local fp, L, iterators, ents, vec3, putf, n_client, hitpush = require"utils.fp", require"utils.lambda", require"std.iterators", require"std.ents", require"utils.vec3", require"std.putf", require"std.n_client", require"std.hitpush"
local map = fp.map

local hooks, module, spawns = {}, {}, {}

function module.on(range, pushz)
  map.np(L"spaghetti.removehook(_2)", hooks)
  hooks, spawns = {}, {}
  if not range then return end
  hooks.try = spaghetti.addhook("spawnstate", function(info)
    if not ents.active() then return end
    local spawning = info.ci
    local ments, clients, p = server.ments, server.clients
    local enemyos = {}
    for i = 0, clients:length() - 1 do
      local ci = clients[i]
      if ci.state.state == engine.CS_ALIVE and ci.clientnum ~= spawning.clientnum and (not server.m_teammode or ci.team ~= spawning.team) then
        enemyos[ci.state.o] = true
      end
    end
    for o, data in pairs(spawns) do
      local nearestdist, nearest = 1/0
      for enemyo in pairs(enemyos) do
        local dist = o:dist(enemyo)
        if dist < nearestdist then nearestdist, nearest = dist, enemyo end
      end
      --sauerbraten coordinate system is backward twice!
      local yaw = nearestdist <= range and nearest and math.atan2(o.x - nearest.x, nearest.y - o.y) * 180 / math.pi or data.attr1
      p = putf(p or { 30, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_EDITENT, data.i, o.x * server.DMF, o.y * server.DMF, o.z * server.DMF, server.PLAYERSTART, yaw, data.attr2, 0, 0, 0)
    end
    if p then engine.sendpacket(spawning.ownernum, 1, n_client(p, spawning.ownernum):finalize(), -1) end
  end)
  module.spawned = pushz and spaghetti.addhook("spawned", function(info) hitpush(info.ci, { x = 0, y = 0, z = -500 }) end) or nil
  module.entsloaded = spaghetti.addhook("entsloaded", module.updatespawns)
  module.updatespawns()
end

function module.updatespawns()
  if not ents.active() then spawns = {} return end
  local tag1, tag2 = (server.m_ctf or server.m_collect) and 1 or 0, (server.m_ctf or server.m_collect) and 2
  spawns = map.mf(function(i, _, ment)
    return (ment.attr2 == tag1 or ment.attr2 == tag2) and vec3(ment.o) or nil, { i = i, attr1 = ment.attr1, attr2 = ment.attr2 }
  end, ents.enum(server.PLAYERSTART))
end

return module
