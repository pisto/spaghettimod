--[[

  Create a unix socket domain to which you can connect:
    socat READLINE,history=.exechistory UNIX-CLIENT:serverexec
  You get a shell which resembles the lua standalone interpreter (history, line editing).
  Fails gracefully if luasocket is not available.

]]--

local Lr = require"utils.lambda".Lr

local fname = require"std.servertag".fntag .. "serverexec"
local ok, err, unix, cmdpipe = pcall(Lr"require 'socket', require'socket.unix', require'std.cmdpipe'")
if not ok then return engine.writelog("Cannot load luasocket with unix socket domain support, serverexec will not be available:\n" .. err) end
local test = unix()
if test:connect(fname) then
  test:close()
  engine.writelog("There is already a unix socket named " .. fname .. ", will not overwrite.")
  return
end
test:close()
os.execute("rm -f " .. fname)
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
