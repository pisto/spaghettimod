local fp, lambda = require"utils.fp", require"utils.lambda"
local map, noop, first, pick, range, I, breakk, Lr = fp.map,  fp.noop,  fp.first,  fp.pick,  fp.range,  fp.I, fp.breakk, lambda.Lr

local function matches(ipmask, ip)
	return ip.ip - ip.ip%ipmask.mod == ipmask.ip
end

local function ipstring(ipmask)
	return ipmask.desc
end

local function sameip(ip1, ip2) return ip1.ip == ip2.ip and ip1.mask == ip2.mask end

local function ip(desc, _mask)
	local ip, mask
	if type(desc)=="string" then
		local oct1, oct2, oct3, oct4, hasmask, dmask = desc:match("^([0-9]+)%.([0-9]+)%.([0-9]+)%.([0-9]+)(/?([0-9]*))$")
		oct1, oct2, oct3, oct4 = map.uv(function(o)
		  o = tonumber(o)
      return o and o == math.floor(o) and o >= 0 and o <= 255 and o or breakk() end
    , oct1, oct2, oct3, oct4)
		if not oct4 then return end
		if hasmask ~= "" then mask = tonumber(dmask) end
		ip = (oct1*0x1000000 + oct2*0x10000 + oct3*0x100 + oct4)
	elseif type(desc)=="table" then ip, mask = desc.ip, desc.mask
	else
		if desc < 0 or desc > 0x100000000 or math.floor(desc) ~= desc then return end
		ip = desc
	end
	mask = _mask or mask or 32
	if mask < 0 or mask > 32 or math.floor(mask) ~= mask then return end
	local mod = math.pow(2, 32-mask)
	ip = ip - ip%mod
	desc = string.format("%d.%d.%d.%d%s", (ip/0x1000000)%0x100, (ip/0x10000)%0x100, (ip/0x100)%0x100, ip%0x100, mask < 32 and ('/' .. mask) or '')
	return setmetatable({}, {__newindex = noop, __tostring = ipstring, __eq = sameip, __index =
		{ ip = ip, mask = mask, mod = mod, desc = desc, matches = matches }
	})
end


--ipset

local function ipkey(_ip, shadow)
	return _ip.ip*0x100 + _ip.mask*2 + (shadow and 1 or 0)
end

local function matcherof(ipset, _ip)
	local mask = first(pick.fz(function(mask) return ipset[ipkey(ip(_ip, mask))] end, range.z(_ip.mask, ipset.min, -1)))
	return mask and ipset[ipkey(ip(_ip, mask))]
end

local nulltable = {}
local function matchesof(ipset, _ip)
	local matchestable = ipset[ipkey(_ip, true)]
	return matchestable and map.sp(I, matchestable) or nulltable
end

local function remove(ipset, _ip)
	_ip = ipset[ipkey(_ip)]
	if not _ip then return end
	map.nf(function(mask)	--remove shadows
		local shadowkey = ipkey(ip(_ip, mask), true)
		local shadowtable = ipset[shadowkey]
		shadowtable[_ip] = nil
		if not next(shadowtable) then ipset[shadowkey] = nil end
	end, range.z(_ip.mask, ipset.min, -1))
	ipset[ipkey(_ip)] = nil
	return _ip
end

local function put(ipset, _ip)
	assert(_ip.mask >= ipset.min, "Cannot insert an ip with mask smaller than ipset.min")
	local matcher = ipset:matcherof(_ip)
	if matcher then return false, { matcher = matcher } end
	local matches = ipset:matchesof(_ip)
	if next(matches) then return false, matches end
	map.nf(function(mask)	--add shadows
		local shadowkey = ipkey(ip(_ip, mask), true)
		local shadowtable = ipset[shadowkey] or {}
		shadowtable[_ip] = true
		ipset[shadowkey] = shadowtable
	end, range.z(_ip.mask, ipset.min, -1))
	ipset[ipkey(_ip)] = _ip
	return _ip
end

local function enum(ipset)
  return map.zf(function(key) return ip(math.modf(key/0x100), (key%0x100)/2) end, pick.zp(Lr"type(_) == 'number' and _ % 2 == 0", ipset))
end

local function ipset(min)
	min = min ~= nil and tonumber(min) or 0
	assert(math.floor(min) == min and min >= 0 and min <=32, "Invalid minimum mask specification")
	return { matcherof = matcherof, matchesof = matchesof, remove = remove, put = put, min = min, enum = enum }
end

return {ip = ip, ipset = ipset}
