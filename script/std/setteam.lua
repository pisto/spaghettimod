--[[

  Properly set the team. No, it's not that easy.

]]--

local putf, iterators = require"std.putf", require"std.iterators"

local module = {}

function module.set(ci, team, reason, keepalive)
  assert(server.m_teammode, "Not in teamed mode")
  team = engine.filtertext(team, false):sub(1, server.MAXTEAMLEN)
  if ci.team == team then return end
  local oldteam = ci.team
  ci.team = team
  server.aiman.changeteam(ci)
  reason = reason or 1
  if ci.state.state == engine.CS_ALIVE and not keepalive then server.suicide(ci) end
  engine.sendpacket(-1 ,1, putf({ 10, r = 1}, server.N_SETTEAM, ci.clientnum, ci.team, reason):finalize(), -1)
  if server.smode then server.smode:changeteam(ci, oldteam, team)
  else server.addteaminfo(team) end
end

function module.sync()
  assert(server.m_teammode, "Not in teamed mode")
  local p
  for ci in iterators.all() do
    server.aiman.changeteam(ci)
    p = putf(p or { 20, r=1 }, server.N_SETTEAM, ci.clientnum, ci.team, -1)
    if server.smode then server.smode:changeteam(ci, ci.team, ci.team)
    else server.addteaminfo(ci.team) end
  end
  if p then engine.sendpacket(-1 ,1, p:finalize(), -1) end
end

return setmetatable(module, {__call = function(_, ...) return module.set(...) end })
