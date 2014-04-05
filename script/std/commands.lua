--[[

  Parse user commands.

]]--

local playermsg = require"std.playermsg"

local function checkstring(info, prefix)
  if info.skip then return end
  local full, lcmd = info.text:lower():match("^( *" .. prefix .. "([%l%d]+) *)")
  if not full or lcmd == "" then return end
  info.skip = true
  local ev = "commands." .. lcmd
  if not spaghetti.hooks[ev] then return playermsg("Unrecognized command " .. lcmd, info.ci) end
  local args = info.text:sub(#full + 1)
  engine.writelog(("command: %s(%d) %s %s"):format(info.ci.name, info.ci.clientnum, lcmd, args:gsub('\n', "\\n")))
  spaghetti.hooks[ev]({ ci = info.ci, command = lcmd, args = args })
end

spaghetti.addhook(server.N_TEXT, function(info) checkstring(info, "[\\#!]") end)
spaghetti.addhook(server.N_SERVCMD, function(info) checkstring(info, "") end)
