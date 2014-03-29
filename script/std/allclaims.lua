--[[

  Record all auth accounts used by a client.

]]--

spaghetti.addhook("master", function(info)
  if not info.authname then return end
  local allclaims = info.ci.extra.allclaims or {}
  local domain = allclaims[info.authdesc or ""] or {}
  domain[info.authname] = true
  allclaims[info.authdesc or ""] = domain
  info.ci.extra.allclaims = allclaims
end)
