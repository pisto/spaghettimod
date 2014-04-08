--[[

  Fork to background, and set SIGUSR1 handler for a full restart of the server.

]]--

local posix = require"posix"

--need an extra indirection, cannot call os.execute() from the signal handler because lua-posix blocks all the signals, and that would be inherited by the final restarted instance, making it unkillable
local reboot = false
spaghetti.addhook("shuttingdown", function()
  if not reboot then return end
  engine.writelog"Restarting..."
  os.execute[[
    ppid=`ps -p $$ -o ppid=`
    for fd in $(ls -r /proc/$$/fd); do eval "exec $fd>&-"; done
    (
      while kill -0 "$ppid"; do sleep 0.1; done
      RESTART=1 exec ./sauer_server
    ) &
  ]]
end)
posix.signal(posix.SIGUSR1, function() reboot, spaghetti.quit = true, true end)

--restarted, already detached
if os.getenv"RESTART" then
  engine.setlogfile(require"std.servertag".fntag .. "log")
  engine.writelog"Restarted."
  return
end

engine.writelog("Detaching...")
engine.setlogfile(require"std.servertag".fntag .. "log")
io.stdin:close()
io.stdout:close()
io.stderr:close()
if posix.fork() ~= 0 then os.exit(0) end
engine.writelog("Detached.")

