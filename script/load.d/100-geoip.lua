--[[

  Geoip banner on connect. Gracefully fails if the libs or database do not exist.
  Waiting for a luarocks release, you can download the module here https://github.com/pisto/mmdblua

]]--

local L = require"utils.lambda"

local ok, db = pcall(L"require'mmdb'.open(os.getenv('GEOIPDB') or 'GeoLite2-City.mmdb')")
if not ok then return engine.writelog("Cannot load the geoip module:\n" .. db) end

local playermsg, commands, ip = require"std.playermsg", require"std.commands", require"utils.ip"

local function prettygeoip(geoip)
  local format, country, region, city = {}, geoip.country and (geoip.country.names and geoip.country.names.en) or geoip.country.en, geoip.subdivisions and geoip.subdivisions[1].names.en, geoip.city and geoip.city.names.en
  if city then table.insert(format, engine.decodeutf8(city)) end
  if region and not (city and region:find(city, 1, true)) then table.insert(format, engine.decodeutf8(region)) end
  if country then table.insert(format, engine.decodeutf8(country)) end
  return table.concat(format, ", ")
end

spaghetti.addhook("connected", function(info)
  local record = db:search_ipv4(tostring(ip.ip(info.ci)))
  if not record then return end
  info.ci.extra.geoip = record
  local pretty = prettygeoip(record)
  return pretty and server.sendservmsg(server.colorname(info.ci, nil) .. " connects from " .. pretty)
end)

commands.add("geoip", function(info)
  local cn = tonumber(info.args)
  if not cn then return playermsg("Usage: #geoip cn", info.ci) end
  local gci = engine.getclientinfo(cn)
  if not gci then return playermsg("cn " .. cn .. " not found or bot.", info.ci) end
  playermsg(gci.extra.geoip and prettygeoip(gci.extra.geoip) or "No geoip record.", info.ci)
end,"Usage: #geoip <cn>")
