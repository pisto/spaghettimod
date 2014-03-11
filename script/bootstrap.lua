--Load the Eclipse LDT debugger if present
pcall(function() require("debugger")() end)
--this table is not enforced but you shouldn't remove it, or you'll get a flood of errrors in the log.
hooks = {}

--simple hook multiplexer
local function makehookgroup()
  return setmetatable({}, { __call = function(hookgroup, ...)
    for _, v in ipairs(hookgroup) do v(...) end
  end})
end
rawset(spaghetti, "makehookgroup", makehookgroup)



hooks.tick = spaghetti.makehookgroup()
local lastsecond = -1
table.insert(hooks.tick, function()
  local now = math.ceil(engine.totalmillis/1000)
  if now <= lastsecond then return end
  lastsecond = now
  engine.writelog(lastsecond % 2 == 1 and "tick!" or "tock!")
end)

hooks.shuttingdown = spaghetti.makehookgroup()
table.insert(hooks.shuttingdown, function(info)
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


hooks[20] = spaghetti.makehookgroup() --N_GUNSELECT
table.insert(hooks[20], function(p)
  local selects, gun = p.cq.extra.gunselects or {}, p.gunselect
  selects[gun] = (selects[gun] or 0) + 1
  engine.writelog(string.format("cn %d selected weapon %d %d times", p.cq.clientnum, gun, selects[gun]))
  p.cq.extra.gunselects = selects
end)

hooks[86] = spaghetti.makehookgroup() --N_AUTHTRY
table.insert(hooks[86], function(p)
  engine.writelog(string.format("cn %d tried auth (%s) as %s. Blocking it.", p.ci.clientnum, p.desc == "" and "<global>" or p.desc, p.name))
  p.skip = true
end)

