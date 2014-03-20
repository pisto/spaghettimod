--[[

  Set a callback on when the client appears to have loaded the map

]]--

require"uuid"

local module = {}

local fp, lambda, iterators = require"utils.fp", require"utils.lambda", require"std.iterators"
local map, L = fp.map, lambda.L

local lastmapload = -1000

spaghetti.addhook("changemap", function()
  map.nf(L"_.extra.maploaded = nil", iterators.clients())
  lastmapload = engine.totalmillis
end)

spaghetti.addhook(server.N_MAPCRC, function(info)
  if info.ci.state.aitype ~= server.AI_NONE or info.ci.extra.maploaded or info.text ~= server.smapname then return end
  info.ci.extra.maploaded = true
  if spaghetti.hooks.maploaded then spaghetti.hooks.maploaded{ci = info.ci, crc = info.crc} end
end)

spaghetti.addhook(server.N_PING, function(info)
  if info.ci.state.aitype ~= server.AI_NONE or not info.ci.connected or info.ci.extra.maploaded
    or engine.totalmillis - lastmapload < engine.getclientpeer(info.ci.clientnum).roundTripTime then return end
  info.ci.extra.maploaded = true
  if spaghetti.hooks.maploaded then spaghetti.hooks.maploaded{ci = info.ci} end
end)
