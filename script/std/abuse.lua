--[[

  Miscellaneous anti abuse tools.

]]--

local playermsg, module = require"std.playermsg", {}

--packet rate limit

local fp, lambda, tb = require"utils.fp", require"utils.lambda", require"utils.tokenbucket"
local noop, I, map, L, U = fp.noop, fp.I, fp.map, lambda.L, fp.U

function module.ratelimit(packets, rate, maxtokens, selector)
  if type(packets) == "number" then packets = {packets} end
  packets = setmetatable({U(packets)}, tokentag)
  packets.hooks = map.m(function(_, p)
    return p, spaghetti.addhook(p, function(info)
      local skip, desc, client
      if selector then
        skip, desc, client = selector(info)
        if skip then return end
      end
      local extra = (client or info.ci).extra
      extra.ratelimit = extra.ratelimit or {}
      extra.ratelimit[packets] = extra.ratelimit[packets] or tb(rate, maxtokens)
      if extra.ratelimit[packets]() then return end
      info.skip, info.ratelimited = true, (desc or true)

    end, true) end
  , packets)
  return packets
end

function module.delratelimit(token)
  map.pn(L"spaghetti.removehook(...)", token.hooks)
end


--Sound that can be sent by vanilla clients

local clientsounds = map.sv(I, server.S_JUMP, server.S_LAND, server.S_NOAMMO)
function module.clientsound(s)
  return (server.m_capture and s == server.S_ITEMAMMO) or clientsounds[s]
end



return module
