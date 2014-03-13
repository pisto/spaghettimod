local lastsecond = -1
spaghetti.addhook("tick", function()
  local now = math.ceil(engine.totalmillis/1000)
  if now <= lastsecond then return end
  lastsecond = now
  engine.writelog(lastsecond % 2 == 1 and "tick!" or "tock!")
end)

spaghetti.addhook("shuttingdown", function(info)
  if info.servererror then return end
  server.sendservmsg("\f6spaghetti finished :/")
  engine.flushserver(true)
  --trick to make the master server delete us in 15 seconds instead of one hour
  engine.requestmaster(string.format("regserv %d\n", cs.serverport))
  engine.flushmasteroutput()
  --make sure packets travel and are parsed before the shutdown packet
  os.execute("sleep .1")
  engine.writelog("bye")
end)

spaghetti.addhook(server.N_GUNSELECT, function(p)
  local selects, gun = p.cq.extra.gunselects or {}, p.gunselect
  selects[gun] = (selects[gun] or 0) + 1
  engine.writelog(string.format("cn %d selected weapon %d %d times", p.cq.clientnum, gun, selects[gun]))
  p.cq.extra.gunselects = selects
end)

spaghetti.addhook(server.N_AUTHTRY, function(p)
  engine.writelog(string.format("cn %d tried auth (%s) as %s. Blocking it.", p.ci.clientnum, p.desc == "" and "<global>" or p.desc, p.name))
  p.skip = true
end)
