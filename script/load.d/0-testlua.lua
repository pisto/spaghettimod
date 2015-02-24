--[[

  If TESTLUA env variable is defined, read lua code from stdin, and quit when done.

]]--

if not os.getenv("TESTLUA") then return end

local function varP(...)
  local ret = {...}
  ret.n = select('#', ...)
  return ret
end

local function tryrun(chunk)
  local f, err = loadstring(chunk)
  if not f then return false, err, not err:match("%'?%<eof%>%'?$") end
  local var = varP(xpcall(f, spaghetti.stackdumper))
  if var.n == 1 then return true end
  local tosend = ""
  for i = 2, var.n do tosend = tosend .. (i == 2 and "" or '\t') .. tostring(var[i]) end
  return true, tosend, true
end

local buff = ""
while true do
  io.stdout:write(buff == "" and "> " or ">> ")
  io.flush()
  local line = io.read("*l")
  if not line then break end
  buff = buff .. line .. '\n'
  local ok, result, consumed = tryrun("return " .. buff)
  if not ok then ok, result, consumed = tryrun(buff) end
  if ok or consumed then
    buff = ""
    if result then print(result) end
  end
end

os.exit()
