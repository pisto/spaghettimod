--[[

  Set time.

]]--

local commands, playermsg, putf = require"std.commands", require"std.playermsg", require"std.putf"

local module = {}

function module.set(millis)
  server.gamelimit = server.gamemillis + math.max(millis, 0)
  engine.sendpacket(-1, 1, putf({10, r=1}, server.N_TIMEUP, math.max(millis/1000, 1)):finalize(), -1)
end

commands.add("settime", function(info)
  if info.ci.privilege < (module.cmdprivilege or server.PRIV_MASTER) then playermsg("Insufficient privilege.", info.ci) return end
  local increment, minutes, seconds = info.args:match("^%s*([%+%-]?)%s*(%d-)%s*:?%s*(%d+)%s*$")
  if not increment then playermsg("Invalid argument " .. info.args, info.ci) return end
  increment = increment ~= "" and increment
  local millis = ((tonumber(minutes) or 0) * 60 + (tonumber(seconds) or 0)) * 1000
  if millis == 0 and increment then return end
  if increment then millis = math.max(server.gamelimit - server.gamemillis + (increment == "-" and -1 or 1) * millis, 0) end
  module.set(millis)
  if millis == 0 then server.sendservmsg(server.colorname(info.ci, nil) .. " terminated the match.") return end
  minutes, seconds = math.modf(math.floor(millis / 1000) /  60)
  server.sendservmsg(server.colorname(info.ci, nil) .. " set the time left to " .. minutes .. ":" .. math.floor(seconds * 60))
end, "#settime [+-][minutes:]seconds : set time left (absolute time, or increment)")

return module
