--[[

  Record all auth accounts used by a client.

]]--

local module = {}

local fp = require"utils.fp"
local map = fp.map

function module.intersect(auths1, auths2)
  local intersect = map.mp(function(domain, names)
    if type(domain) ~= "string" or not auths2[domain] then return end
    return domain, map.sp(function(name)
      local found = auths2[domain]
      if type(found) ~= "table" then return found end
      return found[name]
    end, names)
  end, auths1)
  return next(intersect) and intersect
end

spaghetti.addhook("master", function(info)
  if not info.authname then return end
  local allclaims = info.ci.extra.allclaims or {}
  local domain = allclaims[info.authdesc or ""] or {}
  domain[info.authname] = true
  allclaims[info.authdesc or ""] = domain
  info.ci.extra.allclaims = allclaims
end)

return module
