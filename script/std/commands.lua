--[[

  Parse user commands.

]]--

local module = {}

local fp = require"utils.fp"
local map, I = fp.map, fp.I

local playermsg = require"std.playermsg"

local function checkstring(info, prefix)
  if info.skip then return end
  local full, lcmd = info.text:lower():match("^( *" .. prefix .. "([%l%_%d]+) *)")
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

local helps = {}
function module.add(cmd, fn, help)
  spaghetti.addhook("commands." .. cmd, fn)
  helps[cmd] = help or true
end

module.add("help", function(info)
  local lcmd = info.args:lower():match("^([%l%d]*)")
  if lcmd == "" then
    local cmds = table.concat(map.lp(I, helps), ", ")
    playermsg(cmds ~= "" and ("Commands: " .. cmds) or "No command configured.", info.ci)
    return
  end
  local help = helps[lcmd]
  playermsg(type(help) == "string" and help or (help and "No help for this command." or "Command not found."), info.ci)
end, "Usage: #help [command]")


return module
