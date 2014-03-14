--[[

  Prepend os.date() to all logoutf calls.

]]--

local module = { on = "%c" }

spaghetti.addhook("log", function(info)
  if module.on then info.s = os.date(type(module.on) == "string" and module.on or nil) .. ' | ' .. info.s end
end)

return module
