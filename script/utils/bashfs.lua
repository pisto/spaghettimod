--[[

  Some hacky but safe file manipulation functions to use bash instead of requiring lua-filesystem.

]]--

local module = {}

function module.mkdir(fname)
  local bash = io.popen('mkdir -p "$(cat)" &>/dev/null', "w")
  bash:write(fname)
  local result = bash:close()
  return result == true or result == 0
end

function module.echo(fname, string, append)
  local f, err = io.open(fname, append and "a" or "w")
  if not f then return false, err end
  local ok, err = f:write(string)
  f:close()
  if not ok then return false, err end
  return true
end

return module

