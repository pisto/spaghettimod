--[[

  Mitigate spawnkilling by modifying the yaw of spawns, pointing to the closest enemy.

]]--

local fp, lambda, iterators, ents, vec3, putf, n_client = require"utils.fp", require"utils.lambda", require"std.iterators", require"std.ents", require"utils.vec3", require"std.putf", require"std.n_client"
local map, L, Lr = fp.map, lambda.L, lambda.Lr

local hooks = {}

return { on = function(range)
  map.np(L"spaghetti.removehook(_2)", hooks)
  hooks = {}
  if not range then return end
  hooks.try = spaghetti.addhook(server.N_TRYSPAWN, function(info)
    if info.skip or not ents.active() then return end
    local spawning = info.cq or info.ci
    --no check of validity of N_TRYSPAWN, but modification is sent to this client only
    local teamspawns, teammode, ments, clients, p = server.m_ctf or server.m_collect, server.m_teammode, server.ments, server.clients
    local enemies = {}
    for i = 0, clients:length() - 1 do
      local ci = clients[i]
      if ci.state.state == server.CS_ALIVE and ci.clientnum ~= spawning.clientnum and (not teammode or ci.team ~= spawning.team) then enemies[ci] = true end
    end
    for i = 0, ments:length() - 1 do
      local ment = ments[i]
      if ment.type ~= server.PLAYERSTART or (teamspawns and ment.attr2 == 0) or (not teamspawns and ment.attr2 ~= 0) then return end
      local o, nearestdist, nearest = vec3(ment.o), 1/0
      for enemy in pairs(enemies) do
        local eo = enemy.state.o
        local dist = o:dist(eo)
        if dist >= nearestdist then return end
        nearestdist, nearest = dist, eo
      end
      --sauerbraten coordinate system is backward twice!
      local yaw = nearestdist <= range and nearest and math.atan2(o.x - nearest.x, nearest.y - o.y) * 180 / math.pi or ment.attr1
      o:mul(server.DMF)
      p = putf(p or { 30, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_EDITENT, i, o.x, o.y, o.z, ment.type, yaw, ment.attr2, 0, 0, 0)
    end
    if p then engine.sendpacket(info.ci.clientnum, 1, n_client(p, info.ci):finalize(), -1) end
  end)
end }
