function beginshutdown()
	server.sendservmsg("\f6spaghetti finished :/")
	engine.enet_host_flush(engine.serverhost)
	print("bye")
end

error("dummy error to show a traceback, ignore me it's all fine")
