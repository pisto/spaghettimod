--[[

  Record the last N_POS sent by a client.

]]--

local vec3 = require"utils.vec3"

spaghetti.addhook(server.N_POS, function(info)
  if info.skip or not info.cp then return end
  local posstart = info.curmsg
  local possize = info.p.len - posstart
  info.p.len = posstart
  local lastpos = info.cp.extra.lastpos or { version = 0 }
  lastpos.buf = info.p:getbuf(possize)
  lastpos.physstate, lastpos.flags, lastpos.pos, lastpos.yaw, lastpos.pitch, lastpos.roll, lastpos.vel, lastpos.falling =
    info.physstate, info.flags, vec3(info.pos), info.yaw, info.pitch, info.roll, vec3(info.vel), vec3(info.falling)
  lastpos.version = lastpos.version + 1
  lastpos.millis = engine.totalmillis
  info.cp.extra.lastpos = lastpos
end)
