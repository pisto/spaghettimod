--Load the Eclipse LDT debugger if present
pcall(function() require("debugger")() end)

--ease require"my.module"
package.path = "./script/?.lua;" .. package.path

--simple hook multiplexer
local function addhook(type, callback)
  spaghetti.hooks[type] = spaghetti.hooks[type] or
    setmetatable({}, { __call = function(hookgroup, ...)
      for _, v in ipairs(hookgroup) do v(...) end
    end})
  table.insert(spaghetti.hooks[type], callback)
  return callback
end
rawset(spaghetti, "addhook", addhook)



--simple module loader
local loadd = {}
for i, v in ipairs(engine.listdir"script/load.d") do
  local load = {v:match("^((%d+)%-.+%.[Ll][Uu][Aa])$")}
  if #load == 2 then table.insert(loadd, load) end
end
table.sort(loadd, function(a, b) return a[2]>b[2] end)
for _, v in ipairs(loadd) do dofile("script/load.d/"..v[1]) end
