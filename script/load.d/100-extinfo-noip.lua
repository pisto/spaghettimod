--[[

  Protect the privacy of players by either sending no ip, or sending a fake ip with corresponding geolocalization.

]]--

local fp, it, putf = require"utils.fp", require"std.iterators", require"std.putf"
local map = fp.map

local fakegeoip = io.open(os.getenv"GEOIPCSV" or "GeoIPCountryWhois.csv")
if not fakegeoip then
  cs.extinfoip = 0
  return engine.writelog("Cannot load GeoIPCountryWhois.csv, adjust the GEOIPCSV environment variable. Disabled extinfo ip completely.")
end

local exclusions = {  --skip inconsistencies between GeoLite2-City.mmdb and GeoIPCountryWhois.csv
}

engine.writelog("Generating fake extinfo IPs from geoip csv data...")
local locations = {}
for line in fakegeoip:lines() do
  local ip, code = line:match('"(%d+)","%d+","([A-Z][A-Z])"')
  if ip and not locations[code] and not exclusions[ip] then locations[code] = ip end
end
fakegeoip:close()
engine.writelog("... done")

require"std.extinfo"
spaghetti.addhook("extinfo", function(info)
  if info.code ~= server.EXT_PLAYERSTATS then return end
  local req, p = info.req, info.p
  info.skip = true
  putf(p, server.EXT_ACK, server.EXT_VERSION)
  local which = req:getint()
  local players = {}
  for ci in it.all() do
    if which < 0 or ci.clientnum == which then
      players[ci] = true
      if which >= 0 then break end
    end
  end
  if not next(players) and which >= 0 then
    p:putint(server.EXT_ERROR)
    engine.sendserverinforeply(p)
    return
  end
  p:putint(server.EXT_NO_ERROR)
  local q = p.len
  p:putint(server.EXT_PLAYERSTATS_RESP_IDS)
  for ci in pairs(players) do p:putint(ci.clientnum) end
  engine.sendserverinforeply(p)
  for ci in pairs(players) do
    p.len = q
    local ip = locations[ci.extra.geoip and ci.extra.geoip.country and ci.extra.geoip.country.iso_code] or 0
    putf(p,
      server.EXT_PLAYERSTATS_RESP_STATS,
      ci.clientnum,
      ci.ping,
      ci.name,
      ci.team,
      ci.state.frags,
      ci.state.flags,
      ci.state.deaths,
      ci.state.teamkills,
      ci.state.damage*100/math.max(ci.state.shotdamage, 1),
      ci.state.health,
      ci.state.armour,
      ci.state.gunselect,
      ci.privilege,
      ci.state.state,
      {math.floor(ip / 0x1000000) % 0x100},
      {math.floor(ip / 0x10000) % 0x100},
      {math.floor(ip / 0x100) % 0x100}
      )
    engine.sendserverinforeply(p)
  end
end)
