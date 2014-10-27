--[[

  Run a full garbage colection cycle when the map has been changed.

]]--

spaghetti.addhook("changemap", function()
  local pre = math.floor(collectgarbage"count")
  collectgarbage()
  engine.writelog("gc: " .. pre .. " KiB -> " .. math.floor(collectgarbage"count") .. " KiB")
end)
