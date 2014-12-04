--[[

  Construct iterators of players matching criteria

]]--

local module = {}

local fp, lambda, uuid = require"utils.fp", require"utils.lambda", require"std.uuid"
local pick, map, Lr = fp.pick, fp.map, lambda.Lr

local function iterator(list)
  local todo, done = {}, {}
  local function pump()
    for i = 0, list:length() - 1 do
      local ciuuid = list[i].extra.uuid
      todo[ciuuid] = not done[ciuuid] or nil
    end
    return next(todo)
  end
  return function()
    local ci
    repeat
      local n = next(todo) or pump()
      if not n then return end
      ci, done[n], todo[n] = uuid.find(n), true
    until ci
    return ci
  end
end

function module.all() return iterator(server.clients) end

function module.connects() return iterator(server.connects) end

function module.select(lambda)
  return pick.fz(lambda, module.all())
end

function module.minpriv(priv)
  return module.select(function(client) return client.privilege >= priv end)
end

function module.clients()
  return module.select(function(client) return client.state.aitype ~= server.AI_BOT end)
end

function module.bots()
  return module.select(function(client) return client.state.aitype == server.AI_BOT end)
end

function module.players()
  return module.select(Lr"_.state.state ~= engine.CS_SPECTATOR")
end

function module.spectators()
  return module.select(Lr"_.state.state == engine.CS_SPECTATOR")
end

function module.inteam(team)
  return module.select(function(client) return client.team == team end)
end

return module
