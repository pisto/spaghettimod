--[[

  Prepend os.date() to all logoutf calls.

]]--

local module = { on = false }

spaghetti.addhook("log", function(info)
  if module.on then info.s = os.date(type(module.on) == "string" and module.on or "%c | ") .. info.s end
end)

return module
