--[[

  Attract dumb players with a fancy server description.
  Showcasing functional programming utilities, hooks with deletion, bind to low level sauer structures and cubescript

]]--

local fp = require"utils.fp"
local map, range = fp.map, fp.range


local pingers

spaghetti.addhook("ping", function(info)
  if not pingers then return end
  local req, p = info.req, info.p
  local clientmillis = req:getint()
  if clientmillis == 0 then req.len = 0 return end
  local pinger = pingers[engine.pongaddr.host] or {}
  pinger.lastping = engine.totalmillis
  pinger.millisoffset = clientmillis - engine.totalmillis
  pinger.port = engine.pongaddr.port
  pingers[engine.pongaddr.host] = pinger
  info.skip = true
end)


local interval, lastcheck, desctable, currentpos

local function set(f, wow)
  if not f or f == 0 then pingers, interval, lastcheck, desctable, currentpos = nil return end
  pingers = pingers or {}
  local oldlen = desctable and #desctable
  local concat = wow:rep(math.ceil(25/#wow) + 1)
  desctable = map.f(function(i)
    return concat:sub(i, i + 24)
  end, range.z(1, #wow))
  if oldlen ~= #wow then currentpos = 1 end
  local newinterval = 1000 / (f*#wow)
  if interval ~= newinterval then
    interval = newinterval
    lastcheck = -1000
  end
end


local fakereq, fakep = engine.packetbuf(1, 0), engine.packetbuf(5000, 0)
fakereq:putint(1)

spaghetti.addhook("tick", function()
  if not interval or engine.totalmillis - lastcheck < interval then return end
  lastcheck = engine.totalmillis
  local origdesc, orighost, origport = cs.serverdesc, engine.pongaddr.host, engine.pongaddr.port
  cs.serverdesc = desctable[currentpos]
  map.pn(function(ip, pinger)
    if lastcheck - pinger.lastping > 10000 then pingers[ip] = nil return end
    fakereq.len, fakep.len = 0, 0
    fakep:putint(pinger.millisoffset + lastcheck)
    engine.pongaddr.host, engine.pongaddr.port = ip, pinger.port
    server.serverinforeply(fakereq, fakep)
  end, pingers)
  cs.serverdesc = origdesc
  currentpos = currentpos % #desctable + 1
end)


return {set = set}
