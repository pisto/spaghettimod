--[[

  Flush the server at a different rate than the default.

]]--

local fp = require"utils.fp"

local repeater

local function flusher()
  engine.flushserver(true)
  server.lastsend = engine.enet_time_get()
end

local function set(interval)
  if repeater then repeater = nil, spaghetti.cancel(repeater) end
  if interval == 33 then return end
  flusher()
  repeater = spaghetti.later(interval, flusher, true)
end

return {set = set}

