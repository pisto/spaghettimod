--[[

  Force a coredump on every lua error.

]]--

local posix = require"posix"
local origdumper = spaghetti.stackdumper
rawset(spaghetti, "stackdumper", function(...)
  if posix.fork() == 0 then return posix.abort() end
  return origdumper(...)
end)
