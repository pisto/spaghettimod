local compiled = {}

local function L(code)
	local cached, err = compiled[code]
	if not cached then
		cached, err = loadstring("local _1, _2, _3, _4, _5, _6, _7, _8, _9 = ... local _ = _1 " .. code, "<lambda>")
		if not cached then error(err) end
		compiled[code] = cached
	end
	return cached
end

local function Lr(code) return L("return " .. code) end

return {L = L, Lr = Lr}
