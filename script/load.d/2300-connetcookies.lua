--[[

  Protect against connect flood attacks with spoofed source ip.

]]--

local later = require"utils.later"

later.later(1, function()
  if not engine.serverhost then return end
  engine.enet_host_connect_cookies(engine.serverhost, 0)
end)

