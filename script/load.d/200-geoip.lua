--[[

  Geoip banner on connect. Gracefully fails if the libs or database do not exist.

]]--

local Lr = require"utils.lambda".Lr

local ok, geoip, geoipcity = pcall(Lr"require'geoip', require'geoip.city'")
if not ok then return engine.writelog("Cannot load the geoip module:\n" .. geoip) end
local fn = os.getenv("GEOIPDB") or "/usr/share/GeoIP/GeoLiteCity.dat"
local db = geoipcity.open(fn, geoip.MMAP_CACHE + geoip.CHECK_CACHE)
if not db then return engine.writelog("Cannot load the geoip database (" .. fn .. "), adjust the GEOIPDB environment variable.") end
db:set_charset(geoip.UTF8)
os.execute("cat '" .. fn .. "' >/dev/null 2>/dev/null")        --force the mmaped region to be in the file cache

local playermsg, commands = require"std.playermsg", require"std.commands"

spaghetti.addhook("connected", function(info)
  local record = db:query_by_ipnum(engine.ENET_NET_TO_HOST_32(engine.getclientip(info.ci.clientnum)))
  if not record or not record.country_name then return end
  info.ci.extra.geoip = record.city and ("%s, %s"):format(engine.decodeutf8(record.city), engine.decodeutf8(record.country_name)) or ("%s"):format(engine.decodeutf8(record.country_name))
  server.sendservmsg(("%s(%d) connects from %s"):format(info.ci.name, info.ci.clientnum, info.ci.extra.geoip))
end)

commands.add("geoip", function(info)
  local cn = tonumber(info.args)
  if not cn then return playermsg("Usage: #geoip cn", info.ci) end
  local gci = engine.getclientinfo(cn)
  if not gci then return playermsg("cn " .. cn .. " not found or bot.", info.ci) end
  playermsg(info.ci.extra.geoip and info.ci.extra.geoip or "No geoip record.", info.ci)
end,"Usage: #geoip cn")
