--[[

  Wrap an ENetPacket with a __gc metamethod.

]]--

local lambda = require"utils.lambda"
local L, Lr = lambda.L, lambda.Lr

local meta = { __index = Lr"_1.p[_2]", __newindex = L"_1.p[_2] = _3", __gc = function(p)
  p.referenceCount = p.referenceCount - 1
  return p.referenceCount <= 0 and engine.enet_packet_destroy(p.p)
end }

local function ref(p) p.referenceCount = p.referenceCount + 1 end
local function unref(p) p.referenceCount = p.referenceCount - 1 end

return function(p)
  p.referenceCount = p.referenceCount + 1
  return setmetatable({ p = p, ref = ref, unref = unref }, meta)
end
