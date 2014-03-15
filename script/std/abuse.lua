--[[

  Miscellaneous anti abuse tools.

]]--


--block public masters from kicking
local kicktoken, blockmastervalue
local function blockmasterkick(value)
  if value and not kicktoken then
    kicktoken = spaghetti.addhook(server.N_KICK, function(info)
      if info.skip or info.ci.privilege > server.PRIV_MASTER then return end
      info.skip = true
      engine.sendpacket(info.ci.clientnum, 1,
        engine.packetbuf(30, engine.ENET_PACKET_FLAG_RELIABLE)
          :putint(server.N_SERVMSG)
          :sendstring(type(blockmastervalue) == "string" and blockmastervalue or "You need a higher privilege to kick.")
          :finalize()
      , -1)
    end)
  elseif not value and kicktoken then
    spaghetti.removehook(server.N_KICK, kicktoken)
    kicktoken = nil
  end
  blockmastervalue = value
end


return {blockmasterkick = blockmasterkick}
