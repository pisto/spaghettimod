--[[

  Delay resume after pause.

]]--

local L = require"utils.lambda"
local uuid = require"std.uuid"

local module = { delay = 0, canpause = L"_.privilege >= (cs.restrictpausegame == 1 and server.PRIV_ADMIN or server.PRIV_MASTER)" }

local delayrun
local function cleanup()
  if not delayrun then return end
  spaghetti.cancel(delayrun)
  delayrun = nil
end

spaghetti.addhook(server.N_PAUSEGAME, function(info)
  if info.skip or not module.canpause(info.ci) then return end
  info.skip = true
  if delayrun then
    cleanup()
    server.sendservmsg("Resume has been cancelled by " .. server.colorname(info.ci, nil))
    return
  end
  if not server.gamepaused or not module.delay or module.delay <= 0 then server.pausegame(info.val > 0, info.ci) return end
  server.sendservmsg("Game resume in " .. module.delay .. " seconds, started by " .. server.colorname(info.ci, nil))
  local resumeruuid, delay = info.ci.extra.uuid, module.delay
  delayrun = spaghetti.later(1000, function()
    delay = delay - 1
    if delay <= 0 then server.pausegame(false, uuid.find(resumeruuid) or nil)
    else server.sendservmsg(delay .. "...") end
  end, true)
end)

spaghetti.addhook("pausegame", function(info)
  return not info.val and delayrun and cleanup()
end)

spaghetti.addhook("noclients", cleanup)

return module
