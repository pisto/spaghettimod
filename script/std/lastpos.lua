--[[

  Record the last N_POS sent by a client.

]]--

local vec3 = require"utils.vec3"

spaghetti.addhook(server.N_POS, function(info)
  local cp = info.cp
  if info.skip or info.p:overread() or not cp or cp.state.state ~= engine.CS_ALIVE or math.modf(info.physstate / 8) % 2 ~= cp.state.lifesequence % 2 then return end
  local posstart = info.curmsg
  local possize = info.p.len - posstart
  info.p.len = posstart
  local lastpos = cp.extra.lastpos or { version = 0 }
  lastpos.buf = info.p:getbuf(possize)
  lastpos.physstate, lastpos.flags, lastpos.pos, lastpos.yaw, lastpos.pitch, lastpos.roll, lastpos.vel, lastpos.falling =
    info.physstate, info.flags, vec3(info.pos), info.yaw, info.pitch, info.roll, vec3(info.vel), vec3(info.falling)
  lastpos.version = lastpos.version + 1
  lastpos.millis = engine.totalmillis
  cp.extra.lastpos = lastpos
  local hooks = spaghetti.hooks.positionupdate
  if hooks then hooks({ cp = cp, lastpos = lastpos }) end
end)
