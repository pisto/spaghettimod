engine.writelog("Detaching...")
engine.setlogfile(require"std.servertag".fntag .. "log")
io.stdin:close()
io.stdout:close()
io.stderr:close()
if require"posix".fork() ~= 0 then os.exit(0) end
engine.writelog("Detached.")
