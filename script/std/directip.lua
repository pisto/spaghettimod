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
module.directIP["127.0.0.1"] = true

do
  engine.writelog("Trying to get public ip...")
  local curl = io.popen("dig +short @resolver1.opendns.com myip.opendns.com")
  local extrenalip = curl and curl:read()
  if curl then curl:close() end
  if not extrenalip or not ip.ip(extrenalip) then engine.writelog("Failed to get public IP.")
  else
    module.directIP[extrenalip] = true
    engine.writelog("... done, public IP is " .. extrenalip)
  end
end

function module.directclient(ci)
  local peer = engine.getclientpeer(ci.clientnum)
  return peer and module.directIP[engine.ENET_NET_TO_HOST_32(peer.localAddress.host)]
end

return module
