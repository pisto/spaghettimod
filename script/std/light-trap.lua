--[[

  Attract dumb players with a fancy server description.
  Showcasing functional programming utilities, hooks with deletion, bind to low level sauer structures and cubescript

]]--

local fp = require"utils.fp"
local map, range = fp.map, fp.range


local state
local cslfourcc = 1312969299 --CSL sends this always

local function set(f, wow)
  if not f or f == 0 then
    if not state then return end
    map.nv(spaghetti.removehook, state.pinghook, state.tickhook)
    state = nil
  end

  state = state or { pingers = {} }
  local oldlen = state.descs and #state.descs
  local concat = wow:rep(math.ceil(25/#wow) + 1)
  state.descs = map.f(function(i)
    return concat:sub(i, i + 24)
  end, range.z(1, #wow))
  if oldlen ~= #wow then state.nextdesc = 1 end
  local newinterval = 1000 / (f*#wow)
  if state.interval ~= newinterval then
    state.interval = newinterval
    state.lastcheck = -1000
  end

  if not state.pinghook then
    local fakereq, fakep = engine.packetbuf(1, 0), engine.packetbuf(5000, 0)
    fakereq:putint(1)

    state.pinghook = spaghetti.addhook("ping", function(info)
      local req, p = info.req, info.p
      local clientmillis = req:getint()
      if clientmillis == 0 or clientmillis == cslfourcc then req.len = 0 return end
      local pinger = state.pingers[engine.pongaddr.host] or {}
      pinger.lastping = engine.totalmillis
      pinger.millisoffset = clientmillis - engine.totalmillis
      pinger.port = engine.pongaddr.port
      state.pingers[engine.pongaddr.host] = pinger
      info.skip = true
    end)

    state.tickhook = spaghetti.addhook("tick", function()
      if engine.totalmillis - state.lastcheck < state.interval then return end
      state.lastcheck = engine.totalmillis
      local origdesc, orighost, origport = cs.serverdesc, engine.pongaddr.host, engine.pongaddr.port
      cs.serverdesc = state.descs[state.nextdesc]
      map.pn(function(ip, pinger)
        if state.lastcheck - pinger.lastping > 10000 then state.pingers[ip] = nil return end
        fakereq.len, fakep.len = 0, 0
        fakep:putint(pinger.millisoffset + state.lastcheck)
        engine.pongaddr.host, engine.pongaddr.port = ip, pinger.port
        server.serverinforeply(fakereq, fakep)
      end, state.pingers)
      cs.serverdesc = origdesc
      state.nextdesc = state.nextdesc % #state.descs + 1
    end)

  end

end


return {set = set}
