--[[

  Send a goodbye message to clients before shutdown.

]]--

spaghetti.addhook("shuttingdown", function(info)
  engine.writelog(info.servererror and "Shutting down (fatal)." or "Shutting down.")
  if info.servererror then return end
  server.sendservmsg("\f6This server is shutting down or rebooting.\nTry \f0/reconnect\f6 in a few seconds.")
  engine.flushserver(true)
  engine.enet_host_flush(engine.serverhost)
  os.execute("sleep .1")
end)

