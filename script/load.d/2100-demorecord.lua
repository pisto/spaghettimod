--[[

  Demo record, flushed every second.

]]--

local putf = require"std.putf"

local recordingdemo

spaghetti.addhook("enddemorecord", function()
  engine.writelog("stopped recording " .. recordingdemo)
  recordingdemo = nil
end)

spaghetti.addhook("setupdemorecord", function(info)
  recordingdemo = "demos/" .. os.date(require"std.servertag".tag .. " - %c - " .. server.modename(server.gamemode, '?') .. " - " .. server.smapname:gsub("[%c%p%s]", "_") .. ".dmo")
  info.filename = recordingdemo
  engine.writelog("started recording " .. recordingdemo)
end)

local lastflush = -1000
spaghetti.addhook("recordpacket", function(info)
  if engine.totalmillis - lastflush > 1000 then lastflush, info.flush = engine.totalmillis, true end
end)

local laststep = server.shouldstep
spaghetti.addhook("tick", function()
  if not recordingdemo or server.shouldstep == laststep then return end
  laststep = server.shouldstep
  local notice = putf({ 40, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_SERVMSG, os.date("%c | " .. (laststep and "resumed" or "stopped")))
  server.recordpacket(1, notice.buf:sub(1, notice.len))
end)

spaghetti.addhook("changemap", function() return recordingdemo or server.setupdemorecord() end)
spaghetti.addhook("shuttingdown", function() server.enddemorecord() end)
