--[[

  Geoip banner on connect. Gracefully fails if the libs or database do not exist.

]]--

local Lr = require"utils.lambda".Lr

local ok, geoip, geoipcity = pcall(Lr"require'geoip', require'geoip.city'")
if not ok then return engine.writelog("Cannot load the geoip module.") end
local fn = os.getenv("GEOIPDB") or "/usr/share/GeoIP/GeoLiteCity.dat"
local db = geoipcity.open(fn, geoip.MEMORY_CACHE)
if not db then return engine.writelog("Cannot load the geoip database (" .. fn .. "), adjust the GEOIPDB environment variable.") end
db:set_charset(geoip.UTF8)
spaghetti.addhook("connected", function(info)
  local record = db:query_by_ipnum(engine.ENET_NET_TO_HOST_32(engine.getclientip(info.ci.clientnum)))
  if not record or not record.country_name then return end
  if record.city then
    server.sendservmsg(string.format("%s(%d) connects from %s in %s", info.ci.name, info.ci.clientnum, engine.decodeutf8(record.city), engine.decodeutf8(record.country_name)))
  else
    server.sendservmsg(string.format("%s(%d) connects from %s", info.ci.name, info.ci.clientnum, engine.decodeutf8(record.country_name)))
  end
end)
