--[[

  Flush the server at a different rate than the default.

]]--

local fp, later = require"utils.fp", require"utils.later"

local repeater

local function flusher()
  engine.flushserver(true)
  server.lastsend = engine.enet_time_get()
end

local function set(interval)
  if repeater then server.lastsend, repeater = nil, later.cancel(repeater) end
  if interval == 33 then return end
  flusher()
  repeater = later.later(interval, flusher, true)
end

return {set = set}

