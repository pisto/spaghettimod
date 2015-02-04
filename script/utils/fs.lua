--[[

  Some hacky but safe file manipulation functions instead of requiring lua-filesystem.

]]--

local L = require"utils.lambda"
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

local ok, posix = pcall(L"require'posix'")
if ok then
  function module.readsome(file, n)
    local fd = posix.fileno(file)
    local flags = posix.fcntl(fd, posix.F_GETFL)
    posix.fcntl(fd, posix.F_SETFL, flags + posix.O_NONBLOCK)
    local ret, errstr, errno = posix.read(fd, n or 10240)
    posix.fcntl(fd, posix.F_SETFL, flags)
    if ret then return #ret ~= 0 and ret or nil
    elseif errno == posix.EAGAIN then return "" end
    return nil, errstr, errno
  end
end

return module
