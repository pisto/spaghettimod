hooks = {}
function hooks.shuttingdown()
	server.sendservmsg("\f6spaghetti finished :/")
	engine.enet_host_flush(engine.serverhost)
	print("bye")
end

