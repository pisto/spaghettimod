--Load the Eclipse LDT debugger if present
pcall(function()
  require("debugger")()
  local origdumper = spaghetti.stackdumper
  rawset(spaghetti, "stackdumper", function(...)
    return origdumper(...) --place a breakpoint here to halt on errors
  end)
end)

--ease require"my.module"
package.path = "./script/?.lua;" .. package.path

--Detect whether xpcall accepts the function arguments, otherwise fix that up
local _, xpcall52 = xpcall(function(...) return ... end, function() end, true)
if not xpcall52 then
  local origxpcall = xpcall
  xpcall = function(f, msgh, ...)
    local packargs = {n = select('#', ...), ...}
    return origxpcall(function() return f((table.unpack or unpack)(packargs, 1, packargs.n)) end, msgh)
  end
end

--lua 5.3 workaround
loadstring = loadstring or load

--simple hook multiplexer
local meta = { __call = function(hookgroup, ...)
  table.insert(hookgroup.traverse, 1)
  local traverseIndex = #hookgroup.traverse
  while hookgroup.traverse[traverseIndex] <= #hookgroup do
    local ok, msg = xpcall(hookgroup[hookgroup.traverse[traverseIndex]][1], spaghetti.stackdumper, ...)
    if not ok then engine.writelog("One hook of " .. hookgroup.type .. " resulted in an error: " .. msg) end
    hookgroup.traverse[traverseIndex] = hookgroup.traverse[traverseIndex] + 1
  end
  hookgroup.traverse[traverseIndex] = nil
end}

local function addhook(type, callback, prepend)
  spaghetti.hooks[type] = spaghetti.hooks[type] or setmetatable({ type = type, traverse = {} }, meta)
  local token, hookgroup = {callback, type}, spaghetti.hooks[type]
  table.insert(hookgroup, (prepend and 0 or #spaghetti.hooks[type]) + 1, token)
  if prepend then for traverseIndex in ipairs(hookgroup.traverse) do
    hookgroup.traverse[traverseIndex] = hookgroup.traverse[traverseIndex] + 1
  end end
  return token
end

local function removehook(token)
  local type = token[2]
  local hookgroup = spaghetti.hooks[type]
  if not hookgroup then return end
  for k, v in ipairs(hookgroup) do
    if v == token then
      table.remove(hookgroup, k)
      for traverseIndex in ipairs(hookgroup.traverse) do
        hookgroup.traverse[traverseIndex] = hookgroup.traverse[traverseIndex] - (k <= hookgroup.traverse[traverseIndex] and 1 or 0)
      end
      if #hookgroup == 0 then spaghetti.hooks[type] = nil end
      return
    end
  end
end

rawset(spaghetti, "addhook", addhook)
rawset(spaghetti, "removehook", removehook)

local urandom, entropy = io.open("/dev/urandom"), 0
if not urandom then entropy = os.time()
else
  for _ = 1,7 do entropy = entropy * 0x10 + urandom:read(1):byte() end
  urandom:close()
end
math.randomseed(entropy % 0x7FFFFFFF)


--simple module loader
local loadd = {}
for _, v in ipairs(engine.listdir"script/load.d") do
  local load = {v:match("^((%d+)%-.+%.[Ll][Uu][Aa])$")}
  if #load == 2 then
    load[2] = tonumber(load[2])
    table.insert(loadd, load)
  end
end
table.sort(loadd, function(a, b) if a[2] ~= b[2] then return a[2] < b[2] else return a[1] < b[1] end end)
for _, v in ipairs(loadd) do dofile("script/load.d/"..v[1]) end

spaghetti.later(1, function() collectgarbage() end)
