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
  print(lastsecond % 2 == 1 and "tick!" or "tock!")
end)

hooks.shuttingdown = spaghetti.makehookgroup()
table.insert(hooks.shuttingdown, function(info)
  if info.servererror then return end
  server.sendservmsg("\f6spaghetti finished :/")
  engine.enet_host_flush(engine.serverhost)
  --if the bye message and disconnect packet are received at the same time, the former is not processed
  os.execute("sleep .1")
  print("bye")
end)


hooks[20] = spaghetti.makehookgroup() --N_GUNSELECT
table.insert(hooks[20], function(p)
  local selects, gun = p.cq.extra.gunselects or {}, p.gunselect
  selects[gun] = (selects[gun] or 0) + 1
  print(string.format("cn %d selected weapon %d %d times", p.cq.clientnum, gun, selects[gun]))
  p.cq.extra.gunselects = selects
end)

hooks[86] = spaghetti.makehookgroup() --N_AUTHTRY
table.insert(hooks[86], function(p)
  print(string.format("cn %d tried auth (%s) as %s. Blocking it.", p.ci.clientnum, p.desc == "" and "<global>" or p.desc, p.name))
  p.skip = true
end)

