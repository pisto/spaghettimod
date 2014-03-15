--[[

  Miscellaneous anti abuse tools.

]]--

local playermsg = require"std.playermsg"

--block public masters from kicking
local kicktoken, blockmastervalue
local function blockmasterkick(value)
  if value and not kicktoken then
    kicktoken = spaghetti.addhook(server.N_KICK, function(info)
      if info.skip or info.ci.privilege > server.PRIV_MASTER then return end
      info.skip = true
      playermsg(type(blockmastervalue) == "string" and blockmastervalue or "You need a higher privilege to kick.", info.ci)
    end)
  elseif not value and kicktoken then
    spaghetti.removehook(server.N_KICK, kicktoken)
    kicktoken = nil
  end
  blockmastervalue = value
end



--packet rate limit

local fp, lambda, tb = require"utils.fp", require"utils.lambda", require"utils.tokenbucket"
local noop, I, map, L, U = fp.noop, fp.I, fp.map, lambda.L, fp.U

local function ratelimit(packets, rate, maxtokens, selector)
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

local function delratelimit(token)
  map.pn(L"spaghetti.removehook(...)", token.hooks)
end



return { blockmasterkick = blockmasterkick, ratelimit = ratelimit, delratelimit = delratelimit }
