--[[

  ip/netrange object, ipset for matching ranges.

]]--

local fp, L = require"utils.fp", require"utils.lambda"
local map, pick = fp.map, fp.pick

local hasbit32, bit32 = pcall(L"require'bit32'")
local clearbits = hasbit32
  and function(ip, mask) local s = 32 - mask return bit32.lshift(bit32.rshift(ip, s), s) end
  or L"_1 - _1 % (2 ^ (32 - _2))"
local function matches(ipmask, ip) return clearbits(ip.ip, ipmask.mask) == ipmask.ip end
local function ipstring(ipmask)
  local ip, mask = ipmask.ip, ipmask.mask
  return string.format("%d.%d.%d.%d%s", math.modf(ip/0x1000000)%0x100, math.modf(ip/0x10000)%0x100, math.modf(ip/0x100)%0x100, ip%0x100, mask < 32 and ('/' .. mask) or '')
end
local function sameip(ip1, ip2) return ip1.ip == ip2.ip and ip1.mask == ip2.mask end

local function newip(desc, _mask)
  local ip, mask = 0
  if type(desc) == "string" then
    local octs = { desc:match("^([0-9]+)%.([0-9]+)%.([0-9]+)%.([0-9]+)(/?([0-9]*))$") }
    for i = 1, 4 do
      local o = tonumber(octs[i])
      if not o or o ~= math.floor(o) or o < 0 or o > 255 then return end
      ip = ip * 0x100 + o
    end
    if octs[5] ~= "" then mask = tonumber(octs[6]) end
  elseif type(desc) == "table" then ip, mask = desc.ip, desc.mask
  elseif type(desc) == "userdata" then ip = engine.ENET_NET_TO_HOST_32(engine.getclientip(desc.clientnum))
  else
    if desc < 0 or desc > 0x100000000 or math.floor(desc) ~= desc then return end
    ip = desc
  end
  mask = _mask or mask or 32
  if mask < 0 or mask > 32 or math.floor(mask) ~= mask then return end
  return setmetatable({}, {__newindex = L"", __tostring = ipstring, __eq = sameip, __index =
    { ip = clearbits(ip, mask), mask = mask, matches = matches }
  })
end

--ipset

local function ip2key(ip, mask, shadow) return (shadow and 0x4000000000 or 0) + (clearbits(ip, mask) * 0x40 + mask) end
local function key2ip(key) return newip(math.modf(key / 0x40), key % 0x40) end

local function matches(ipset, ip)
  local matchestable, shadowed = {}, ipset[ip2key(ip.ip, ip.mask, true)]
  if shadowed then for key in pairs(shadowed) do matchestable[key2ip(key)] = ipset[key] end end
  if next(matchestable) then return matchestable end
  for mask = ip.mask, ipset.min, -1 do
    local value = ipset[ip2key(ip.ip, mask)]
    if value ~= nil then matchestable[newip(ip.ip, mask)] = value break end
  end
  return matchestable
end

local function remove(ipset, ip)
  local realkey = ip2key(ip.ip, ip.mask)
  local value = ipset[realkey]
  if value == nil then return false, matches(ipset, ip) end
  for mask = ip.mask - 1, ipset.min, -1 do
    local shadowkey = ip2key(ip.ip, mask, true)
    local shadowtable = ipset[shadowkey]
    shadowtable[realkey] = nil
    if not next(shadowtable) then ipset[shadowkey] = nil end
  end
  ipset[realkey] = nil
  return true, value
end

local function put(ipset, ip, value)
  assert(ip.mask >= ipset.min, "Cannot insert an ip with mask smaller than ipset.min")
  local shadows = ipset:matches(ip)
  if next(shadows) then return false, shadows end
  local realkey = ip2key(ip.ip, ip.mask)
  for mask = ip.mask - 1, ipset.min, -1 do
    local shadowkey = ip2key(ip.ip, mask, true)
    local shadowtable = ipset[shadowkey] or {}
    shadowtable[realkey] = true
    ipset[shadowkey] = shadowtable
  end
  value = value == nil and true or value
  ipset[realkey] = value
  return true, value
end

local function enum(ipset)
  return map.zf(function(key, value) return key2ip(key), value end, pick.zp(L"_ < 0x4000000000", ipset))
end

local function ipset(min)
  min = min ~= nil and tonumber(min) or 0
  assert(math.floor(min) == min and min >= 0 and min <= 32, "Invalid minimum mask specification")
  return setmetatable({}, {__index =
    { matches = matches, remove = remove, put = put, min = min, enum = enum }})
end

--lipset, does not check for shadowed ranges when putting/removing.

local function remove_l(ipset, ip)
  local key = ip2key(ip.ip, ip.mask)
  local value = ipset[key]
  if value == nil then return false, matches(ipset, ip) end
  ipset[key] = nil
  return true, value
end

local function put_l(ipset, ip, value)
  local key = ip2key(ip.ip, ip.mask)
  local evalue = ipset[key]
  if evalue ~= nil then return false, { [ip] = evalue } end
  value = value == nil and true or value
  ipset[key] = value
  return true, value
end

local function enum_l(ipset)
  return map.zp(function(key, value) return key2ip(key), value end, ipset)
end

local function ipset_l(min)
  min = min ~= nil and tonumber(min) or 0
  return setmetatable({}, {__index =
    { matches = matches, remove = remove_l, put = put_l, min = min, enum = enum_l }})
end

return {ip = newip, ipset = ipset, lipset = ipset_l}
