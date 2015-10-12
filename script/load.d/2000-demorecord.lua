--[[

  Demo record.

]]--

local folder, mkdir = "var/" .. require"utils.servertag".fntag .. "demos", require"utils.fs".mkdir
if not mkdir(folder) then engine.writelog("Cannot create the demos folder (demos will be disabled)") return end

local putf = require"std.putf"

local recordingdemo

spaghetti.addhook("enddemorecord", function()
  engine.writelog("stopped recording " .. recordingdemo)
  recordingdemo = nil
end)

spaghetti.addhook("setupdemorecord", function(info)
  recordingdemo = os.date(folder .. "/%c - " .. server.modename(server.gamemode, '?') .. " - " .. server.smapname:gsub("[%c%p%s]", "_") .. ".dmo")
  info.filename = recordingdemo
  engine.writelog("started recording " .. recordingdemo)
end)

local hadclients = false
spaghetti.addhook("connected", function()
  if not recordingdemo or hadclients then return end
  hadclients = true
  local notice = putf({ 40 }, server.N_SERVMSG, os.date("%c | resumed"))
  server.recordpacket(1, notice.buf)
  engine.writelog("demo resumed")
end)
spaghetti.addhook("noclients", function()
  if not recordingdemo then return end
  hadclients = false
  local notice = putf({ 40 }, server.N_SERVMSG, os.date("%c | paused"))
  server.recordpacket(1, notice.buf)
  engine.writelog("demo paused")
end)

spaghetti.addhook("changemap", function() return recordingdemo or server.setupdemorecord() end, true)
spaghetti.addhook("entsloaded", function() return recordingdemo or server.setupdemorecord() end, true)
spaghetti.addhook("shuttingdown", function() server.enddemorecord() end)
