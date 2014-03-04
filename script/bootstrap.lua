--these tables are not enforced but you shouldn't remove them, or you'll get a flood of errrors in the log.
hooks, pf = {}, {}

--simple hook multiplexer
local function makehookgroup()
	return setmetatable({}, { __call = function(hookgroup, ...)
		for _, v in ipairs(hookgroup) do v(...) end
	end})
end
rawset(spaghetti, "makehookgroup", makehookgroup)



hooks.tick = spaghetti.makehookgroup()
local lastsecond = -1
table.insert(hooks.tick, function(millis)
	local now = math.ceil(millis/1000)
	if now <= lastsecond then return end
	lastsecond = now
	print(lastsecond % 2 == 1 and "tick!" or "tock!")
end)

hooks.shuttingdown = spaghetti.makehookgroup()
table.insert(hooks.shuttingdown, function()
	server.sendservmsg("\f6spaghetti finished :/")
	engine.enet_host_flush(engine.serverhost)
	--if the bye message and disconnect packet are received at the same time, the former is not processed
	os.execute("sleep .1")
  print("bye")
end)
