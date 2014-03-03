--this table is not enforced but you shouldn't remove it
hooks = {}

--simple hook multiplexer
local function makehookgroup()
	local hookgroup = {}
	return setmetatable(hookgroup, { __call = function(...)
		for _, v in ipairs(hookgroup) do v(...) end
	end})
end
rawset(spaghetti, "makehookgroup", makehookgroup)



hooks.shuttingdown = spaghetti.makehookgroup()

table.insert(hooks.shuttingdown, function()
	server.sendservmsg("\f6spaghetti finished :/")
	engine.enet_host_flush(engine.serverhost)
	--if the bye message and disconnect packet are received at the same time, the former is not processed
	os.execute("sleep .1")
  print("bye")
end)
