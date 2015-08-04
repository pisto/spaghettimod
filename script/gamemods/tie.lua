--[[

  Implement tie breaker with either extra time or "golden goal" (zero extra time).

]]--

local fp, L, iterators, putf = require"utils.fp", require"utils.lambda", require"std.iterators", require"std.putf"
local map = fp.map

local hooks

local function tie()
  local scores
  if server.m_teammode then
    scores = {}
    for ci in iterators.players() do scores[ci.team] = server.smode and server.smode:getteamscore(ci.team) or server.addteaminfo(ci.team).score end
  else scores = map.mf(L"_, _.state.frags", iterators.players()) end
  scores = table.sort(map.lp(L"_2", scores), L"_1 > _2")
  return scores[1] == scores[2]
end

local function tie_checkall()
  local scores = {}
  for ci in iterators.players() do
    local score = server.m_teammode and (server.smode and server.smode:getteamscore(ci.team) or server.addteaminfo(ci.team).score) or ci.state.frags
    if scores[score] then return true end
    scores[score] = true
  end
end

local function cleanup()
  map.np(L"spaghetti.removehook(_2)", hooks)
  hooks = nil
end

return function(extratime, checkall, msg)
  if hooks then cleanup() end
  if not extratime then return end
  assert(extratime >= 0, "invalid extratime value")
  hooks = {}
  hooks.changemap = spaghetti.addhook("changemap", cleanup)
  hooks.intermission = spaghetti.addhook("intermission", cleanup)
  hooks.noclients = spaghetti.addhook("noclients", L"server.checkvotes(true)")
  hooks.preintermission = spaghetti.addhook("preintermission", function(info)
    if info.skip or not (checkall and tie_checkall or tie)() then return end
    info.skip = true
    if extratime > 0 then
      server.gamelimit = server.gamelimit + extratime
      engine.sendpacket(-1, 1, putf({10, r=1}, server.N_TIMEUP, math.max(extratime/1000, 1)):finalize(), -1)
      server.sendservmsg(msg or "TIE! Time has been extended!")
      return
    end
    if hooks.worldupdate then return end
    hooks.worldupdate = spaghetti.addhook("worldupdate", function()
      if not (checkall and tie_checkall or tie)() then return end
      server.gamelimit = server.gamemillis + 1
    end)
    server.gamelimit = server.gamemillis + 1
    server.sendservmsg(msg or "TIE! First to score wins!")
  end)
end
