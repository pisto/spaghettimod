--[[

  Parse user commands.

]]--

local module = {}

local fp = require"utils.fp"
local map, I = fp.map, fp.I

local playermsg = require"std.playermsg"

local function checkstring(info, prefix)
  if info.skip or info.ci.state.aitype ~= server.AI_NONE then return end
  local full, lcmd = info.text:lower():match("^( *" .. prefix .. "([%l%_%d]+) *)")
  if not full or lcmd == "" then return end
  info.skip = true
  local ev = "commands." .. lcmd
  if not spaghetti.hooks[ev] then return playermsg("Unrecognized command " .. lcmd, info.ci) end
  local args = info.text:sub(#full + 1)
  engine.writelog(("command: %s (%d) %s %s"):format(info.ci.name, info.ci.clientnum, lcmd, args:gsub('\n', "\\n")))
  spaghetti.hooks[ev]({ ci = info.ci, command = lcmd, args = args })
end

spaghetti.addhook(server.N_TEXT, function(info) checkstring(info, "[\\#!]") end)
spaghetti.addhook(server.N_SERVCMD, function(info) checkstring(info, "") end)

local cmds = {}
function module.add(cmd, fn, help)
  if cmds[cmd] then spaghetti.removehook(cmds[cmd].hook) end
  cmds[cmd] = { hook = spaghetti.addhook("commands." .. cmd, fn), help = help }
end

function module.remove(cmd)
  if cmds[cmd] then spaghetti.removehook(cmds[cmd]) end
  cmds[cmd] = nil
end

module.add("help", function(info)
  local lcmd = info.args:lower():match("^([%l%d]*)")
  if lcmd == "" then
    local cmds = table.concat(table.sort(map.lp(I, cmds)), ", ")
    playermsg(cmds ~= "" and ("Commands: " .. cmds) or "No command configured.", info.ci)
    return
  end
  playermsg(not cmds[lcmd] and "Command not found." or cmds[lcmd].help or "No help for this command.", info.ci)
end, "Usage: #help [command]")


return module
