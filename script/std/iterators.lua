--[[

  Construct iterators of players matching criteria

]]--

local module = {}

local fp, lambda = require"utils.fp", require"utils.lambda"
local pick, range, map, Lr = fp.pick, fp.range, fp.map, lambda.Lr

function module.all()
  return map.fz(Lr"server.clients:length() > _ and server.clients[_] or error('accessing server.clients out of bonds')", range.z(0, server.clients:length() - 1))
end

function module.select(lambda)
  return pick.fz(lambda, module.all())
end

function module.minpriv(priv)
  return module.select(function(client) return client.privilege >= priv end)
end

function module.clients(priv)
  return module.select(function(client) return client.state.aitype ~= server.AI_BOT end)
end

function module.bots(priv)
  return module.select(function(client) return client.state.aitype == server.AI_BOT end)
end

function module.players()
  return module.select(Lr"_.state.state ~= server.CS_SPECTATOR")
end

function module.spectators()
  return module.select(Lr"_.state.state == server.CS_SPECTATOR")
end

function module.inteam(team)
  return module.select(function(client) return client.team == team end)
end

function module.connects()
  return map.fz(Lr"server.connects:length() > _ and server.connects[_] or error('accessing server.connects out of bonds')", range.z(0, server.connects:length() - 1))
end

return module
