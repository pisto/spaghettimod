--[[

  Miscellaneous anti abuse tools.

]]--

local module = { blockmasterkick = false }

spaghetti.addhook(server.N_KICK, function(info)
  if not module.blockmasterkick or info.ci.privilege > server.PRIV_MASTER then return end
  info.skip = true
  engine.sendpacket(info.ci.clientnum, 1,
    engine.packetbuf(30, engine.ENET_PACKET_FLAG_RELIABLE)
    :putint(server.N_SERVMSG)
    :sendstring(type(module.blockmasterkick) == "string" and module.blockmasterkick or "You need a higher privilege to kick.")
    :finalize()
  , -1)
end)


return module

