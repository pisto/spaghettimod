--Load the Eclipse LDT debugger if present
pcall(function() require("debugger")() end)

--ease require"my.module"
package.path = "./script/?.lua;" .. package.path

--Detect whether xpcall accepts the function arguments, otherwise fix that up
local _, xpcall52 = xpcall(function(...) return ... end, function() end, true)
if not xpcall52 then
  local origxpcall = xpcall
  xpcall = function(f, msgh, ...)
    local packargs = {..., n = select('#', ...)}
    return origxpcall(function() return f(unpack(packargs, 1, packargs.n)) end, msgh)
  end
end

--simple hook multiplexer
local function addhook(type, callback, prepend)
  spaghetti.hooks[type] = spaghetti.hooks[type] or
    setmetatable({}, { __call = function(hookgroup, ...)
      for _, v in ipairs(hookgroup) do
        local ok, msg = xpcall(v[1], spaghetti.stackdumper, ...)
        if not ok then engine.writelog("One hook of " .. type .. " resulted in an error: " .. msg) end
      end
    end})
  local token = {callback}
  table.insert(spaghetti.hooks[type], (prepend and 0 or #spaghetti.hooks[type]) + 1, token)
  return token
end

local function removehook(type, token)
  for k, v in ipairs(spaghetti.hooks[type]) do
    if v == token then
      table.remove(spaghetti.hooks[type])
      if #spaghetti.hooks[type] == 0 then spaghetti.hooks[type] = nil end
      return
    end
  end
end

rawset(spaghetti, "addhook", addhook)
rawset(spaghetti, "removehook", removehook)



--simple module loader
local loadd = {}
for i, v in ipairs(engine.listdir"script/load.d") do
  local load = {v:match("^((%d+)%-.+%.[Ll][Uu][Aa])$")}
  if #load == 2 then table.insert(loadd, load) end
end
table.sort(loadd, function(a, b) return a[2]<b[2] end)
for _, v in ipairs(loadd) do dofile("script/load.d/"..v[1]) end
