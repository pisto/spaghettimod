--[[

  Flush the server at a different rate than the default.

]]--

local repeater, connected

local function flusher()
  if server.clients:length() == 0 then
    spaghetti.cancel(repeater)
    repeater = nil
    return
  end
  engine.flushserver(true)
  server.lastsend = engine.enet_time_get()
end

local function set(interval)
  if repeater then
    spaghetti.cancel(repeater)
    spaghetti.removehook(connected)
    repeater, connected = nil
  end
  if interval >= 33 then return end
  repeater = spaghetti.later(interval, flusher, true)
  connected = spaghetti.addhook("connected", function()
    repeater = repeater or spaghetti.later(interval, flusher, true)
  end)
end

return {set = set}

