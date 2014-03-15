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


return {blockmasterkick = blockmasterkick}
