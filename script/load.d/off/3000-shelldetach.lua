--[[

  Fork to background, and set SIGUSR1 handler for a full restart of the server.

]]--

local posix = require"posix"

--need an extra indirection, cannot call os.execute() from the signal handler because lua-posix blocks all the signals, and that would be inherited by the final restarted instance, making it unkillable
reboot = false
spaghetti.addhook("shuttingdown", function()
  if not reboot then return end
  engine.writelog"Restarting..."
  os.execute[[ exec bash -c '
    me=$$
    sauer=$PPID
    for fd in $(ls -r /proc/$me/fd); do eval "exec $fd>&-"; done
    (
      while kill -0 $sauer; do sleep 0.1; done
      RESTART=1 exec ./sauer_server
    ) &
  ' ]]
end)

local function setsignal(which, f)
  if type(posix.signal) == "function" then return posix.signal(which, f) end
  posix.signal[which] = f
end

setsignal(posix.SIGUSR1, function() reboot, spaghetti.quit = true, true end)

--restarted, already detached
if os.getenv"RESTART" then
  engine.setlogfile("var/" .. require"utils.servertag".fntag .. "log")
  engine.writelog"Restarted."
  return
end

spaghetti.later(100, function()
  if spaghetti.quit then return end
  engine.writelog("Detaching...")
  engine.setlogfile("var/" .. require"utils.servertag".fntag .. "log")
  if posix.fork() ~= 0 then spaghetti._Exit(0) end
  io.stdin:close()
  io.stdout:close()
  io.stderr:close()
  engine.writelog("Detached.")
  local hooks = spaghetti.hooks.forked
  return hooks and hooks{}
end)

