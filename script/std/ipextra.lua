--[[

  Persistent extra table attached to an IP, analogous to the extra table of a clientinfo.

]]--

local fp, iterators = require"utils.fp", require"std.iterators"
local module, extras, ttl = {}, {}, 10 * 60 * 1000

local function ciip(ci)
  return engine.ENET_NET_TO_HOST_32(engine.getclientip(ci.clientnum))
end

local pruner
pruner = function(ip)
  return function()
    for ci in iterators.clients() do
      if ciip(ci) == ip then spaghetti.later(ttl, pruner(ip)) return end
    end
    extras[ip] = nil
  end
end

spaghetti.addhook("enetconnect", function(info)
  local ip = engine.ENET_NET_TO_HOST_32(info.event.peer.address.host)
  if extras[ip] then return end
  extras[ip] = {}
  spaghetti.later(ttl, pruner(ip))
end)

spaghetti.addhook("clientconnect", function(info)
  info.ci.extra.ipextra = extras[ciip(info.ci)]
end)

function module.ttl(reqttl, force)
  ttl = (reqttl > ttl or force) and reqttl or ttl
end

function module.find(ip)
  return extras[ip]
end

return module
