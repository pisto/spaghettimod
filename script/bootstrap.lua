--Load the Eclipse LDT debugger if present
pcall(function() require("debugger")() end)
--this table is not enforced but you shouldn't remove it, or you'll get a flood of errrors in the log.
hooks = {}

--simple hook multiplexer
local function makehookgroup()
  return setmetatable({}, { __call = function(hookgroup, ...)
    for _, v in ipairs(hookgroup) do v(...) end
  end})
end
rawset(spaghetti, "makehookgroup", makehookgroup)



--simple module loader
local loadd = {}
for i, v in ipairs(engine.listdir"script/load.d") do
  local load = {v:match("^((%d+)%-.+%.[Ll][Uu][Aa])$")}
  if #load == 2 then table.insert(loadd, load) end
end
table.sort(loadd, function(a, b) return a[2]>b[2] end)
for _, v in ipairs(loadd) do dofile("script/load.d/"..v[1]) end
