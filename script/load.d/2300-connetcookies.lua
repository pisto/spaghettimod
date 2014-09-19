--[[

  Protect against connect flood attacks with spoofed source ip.

]]--

local later = require"utils.later"

later.later(1, function()
  if not engine.serverhost then return end
  if engine.enet_host_connect_cookies(engine.serverhost, 0) ~= 0 then return end
  later.later(engine.ENET_HOST_DEFAULT_CONNECTING_PEER_TIMEOUT, function()
    local connrate = engine.serverhost.connectsInWindow / engine.ENET_HOST_DEFAULT_CONNECTING_PEER_TIMEOUT * 1000
    if connrate < 100 then return end
    engine.writelog("Connection flood " .. connrate .. "/s \t(window size " .. engine.serverhost.connectsWindow .. " " .. math.modf(engine.serverhost.connectsInWindow / engine.serverhost.connectsWindow * 100) .. "% used)")
  end, true)
end)

