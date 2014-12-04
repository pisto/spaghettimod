--[[

  Demo record, flushed every second.

]]--

local folder, mkdir = "var/" .. require"utils.servertag".fntag .. "demos", require"utils.bashfs".mkdir
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

local lastflush = -1000
spaghetti.addhook("recordpacket", function(info)
  if engine.totalmillis - lastflush > 1000 then lastflush, info.flush = engine.totalmillis, true end
end)

local hadclients = false
spaghetti.addhook("connected", function()
  if not recordingdemo or hadclients then return end
  hadclients = true
  local notice = putf({ 40, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_SERVMSG, os.date("%c | resumed"))
  server.recordpacket(1, notice.buf:sub(1, notice.len))
  engine.writelog("demo resumed")
end)
spaghetti.addhook("noclients", function()
  hadclients = false
  local notice = putf({ 40, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_SERVMSG, os.date("%c | paused"))
  server.recordpacket(1, notice.buf:sub(1, notice.len))
  engine.writelog("demo paused")
end)

spaghetti.addhook("changemap", function() return recordingdemo or server.setupdemorecord() end)
spaghetti.addhook("shuttingdown", function() server.enddemorecord() end)
