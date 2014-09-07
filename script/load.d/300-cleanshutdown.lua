--[[

  Send a goodbye message to clients before shutdown, and use a hacky way to remove us from the master server quickly.

]]--

spaghetti.addhook("shuttingdown", function(info)
  if info.servererror then return end
  server.sendservmsg("\f6This server is shutting down or rebooting.\nTry \f0/reconnect\f6 in a few seconds.")
  engine.requestmaster(string.format("regserv %d\n", cs.serverport)) --this will fail in 15 seconds and causes our removal
  engine.flushmasteroutput()
  engine.flushserver(true)
  engine.enet_host_flush(engine.serverhost)
  os.execute("sleep .1")
end)

