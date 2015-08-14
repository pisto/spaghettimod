--[[

  Utilities to check if the player connects to the public interfaces of the server.

]]--

local ip = require"utils.ip"

local module = {}
module.directIP = setmetatable({}, {
__newindex = function(t, ipstring, yes)
  rawset(t, assert(ip.ip(ipstring).ip, "Invalid IP"), yes)
end,
__index = function(t, ipstring)
  return rawget(t, ip.ip(ipstring).ip)
end,
})

function module.directclient(ci)
  if not next(module.directIP) then return true end
  local peer = engine.getclientpeer(ci.clientnum)
  return peer and module.directIP[engine.ENET_NET_TO_HOST_32(peer.localAddress.host)]
end

return module
