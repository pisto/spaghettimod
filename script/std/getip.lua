--[[

  #getip command.

]]--

local fp, L = require"utils.fp", require"utils.lambda"
local commands, ip, playermsg, iterators = require"std.commands", require"utils.ip", require"std.playermsg", require"std.iterators"
local map = fp.map

local function IPmsg(ci)
  return "IP: " .. ci.clientnum .. "\t\t" .. ci.name .. "\t\t" .. tostring(ip.ip(engine.ENET_NET_TO_HOST_32(engine.getclientip(ci.clientnum))))
end

commands.add("getip", function(info)
  if info.skip then return end
  if info.ci.privilege < server.PRIV_AUTH then playermsg("Insufficient privilege to get IPs.", info.ci) end
  if info.args:match("^ *$") then
    for _, ci in ipairs(table.sort(map.lf(L"_", iterators.clients()), L"_1.clientnum < _2.clientnum")) do playermsg(IPmsg(ci), info.ci) end
  else
    local tci = engine.getclientinfo(tonumber(info.args) or -1)
    if not tci then playermsg("Invalid cn " .. info.args, info.ci)
    else playermsg(IPmsg(tci), info.ci) end
  end
end, "#getip [cn]: get ip of cn, omit to get all IPs")
