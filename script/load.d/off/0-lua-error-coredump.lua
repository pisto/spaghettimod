--[[

  Force a coredump on every lua error.

]]--

local posix = require"posix"
local origdumper = spaghetti.stackdumper
rawset(spaghetti, "stackdumper", function(...)
  local child = posix.fork()
  if child == 0 then return posix.abort() end
  spaghetti.later(10000, function() posix.wait(child) end)
  return origdumper(...)
end)
