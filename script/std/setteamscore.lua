--[[

  Set and sync scores for teams.

]]--

local fp, L, putf, iterators, commands, playermsg = require"utils.fp", require"utils.lambda", require"std.putf", require"std.iterators", require"std.commands", require"std.playermsg"
local map = fp.map

local module = { cmdprivilege = server.PRIV_ADMIN }

function module.syncscore(team)
  assert(server.m_teammode, "Not in a teamed mode")
  server.pruneteaminfo()
  local p
  if server.smode then
    p = putf({30,r=1})
    server.smode:initclient(nil, p, true)
  elseif not team then
    p = putf({30,r=1}, server.N_TEAMINFO)
    for team in pairs(map.sf(L"_.team", iterators.all())) do
      local teaminfo = team ~= "" and server.addteaminfo(team)
      putf(p, team, teaminfo and teaminfo.frags or 0)
    end
    putf(p, "")
  else
    local teaminfo = team ~= "" and server.addteaminfo(team)
    p = putf({30,r=1}, server.N_TEAMINFO, team, teaminfo and teaminfo.frags or 0)
  end
  engine.sendpacket(-1, 1, p:finalize(), -1)
end

function module.set(team, score)
  assert(server.m_teammode, "Not in a teamed mode")
  local teamindx = server.ctfteamflag(team)
  if server.m_ctf then server.ctfmode:setscore(teamindx, score)
  elseif server.m_capture then server.capturemode:findscore(team).total = score
  elseif server.m_collect then server.collectmode:setscore(teamindx, score)
  else assert(team ~= "" and server.addteaminfo(team), "Cannot create teaminfo").frags = score end
  module.syncscore(team)
end

local teammatch = "^%s*(" .. ("%S?"):rep(server.MAXTEAMLEN) .. ")%s+([%-%+]?)(%d+)%s*$"
commands.add("setteamscore", function(info)
  if not module.cmdprivilege or info.ci.privilege < module.cmdprivilege then playermsg("Insufficient privilege.", info.ci) return end
  if not server.m_teammode then playermsg("Not in a teamed mode.", info.ci) return end
  local team, increment, score = info.args:match(teammatch)
  if not team then playermsg("Invalid format.", info.ci) return end
  if server.smode and not server.smode:canchangeteam(nil, nil, team) then playermsg("Cannot set score of team '" .. team .. "'", info.ci) return end
  if increment ~= "" then
    local origscore = server.smode and server.smode:getteamscore(team)
    if not origscore then
      local teaminfo = team ~= "" and server.addteaminfo(team)
      origscore = teaminfo and teaminfo.frags or 0
    end
    score = origscore + (increment == "+" and 1 or -1) * score
  end
  module.set(team, score)
  server.sendservmsg(server.colorname(info.ci, nil) .. " set the score of team " .. team .. " to " .. score)
end, "#setteamscore <team> [+-]<score>: set the score of a team (+/- for increment)")

return module
