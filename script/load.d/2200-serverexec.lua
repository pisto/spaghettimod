--[[

  Create a unix socket domain to which you can connect:
    socat READLINE,history=.exechistory UNIX-CLIENT:serverexec
  You get a shell which resembles the lua standalone interpreter (history, line editing).
  Fails gracefully if luasocket is not available.

]]--

local Lr = require"utils.lambda".Lr

local fname = "serverexec-" .. require"std.servertag".tag
os.execute("rm -f " .. fname)
local ok, unix, cmdpipe = pcall(Lr"require'socket.unix', require'std.cmdpipe'")
if not ok then return engine.writelog("Cannot load luasocket with unix socket domain support, serverexec will not be available.") end
local pipein = unix()
if pipein:bind(fname) ~= 1 or pipein:listen() ~= 1 then
  pipein:close()
  engine.writelog("Cannot listen on " .. fname)
else
  local unixpipe = cmdpipe.create(pipein, true, Lr"engine.writelog((_2 and 'new' or 'closed') .. ' connection to serverexec (' .. _1:getfd() .. ')')")
  spaghetti.addhook("shuttingdown", function()
    os.execute("rm -f " .. fname)
    unixpipe:close()
  end)
  unixpipe:selfservice()
end
