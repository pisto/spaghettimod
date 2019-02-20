--[[

  Protect the privacy of players by either sending no ip, or sending a fake ip with corresponding geolocalization.

]]--

local fp, ip, it, putf = require"utils.fp", require"utils.ip", require"std.iterators", require"std.putf"
local map = fp.map

engine.writelog("Generating fake extinfo IPs from geoip CSV data...")

local geoips = io.open(os.getenv"GEOIPCSV" or "GeoLite2-Country-Blocks-IPv4.csv")
if not geoips then
  cs.extinfoip = 0
  engine.writelog("Cannot load the geoip CSV, adjust the GEOIPCSV environment variable or issue ./updategeoip")
  engine.writelog("Disabled extinfo ip completely.")
  return 
end

local locations = {}
for line in geoips:lines() do
  local _ip, id, regid = line:match('^([%d%.]+)/?%d*,(%d+),(%d+).*$')
  id, regid = tonumber(id), tonumber(regid)
  if _ip and id and id == regid and not locations[id] and ip.ip(_ip).ip % 256 == 0 then 
    locations[id] = ip.ip(_ip).ip 
  end
end
geoips:close()

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
    local ip = locations[ci.extra.geoip and ci.extra.geoip.country and ci.extra.geoip.country.geoname_id] or 0
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
