--[[

  Send cubescript through N_SERVCMD, after authenticating the server. Functionality found in my anticheat client.
  For historical reason the commands are prepended with a sdos_ prefix.

]]--

local putf, fence = require"std.putf", require"std.fence"

local module = {}

require"std.commands"

local function raise(type, ...)
  local hooks = spaghetti.hooks[type]
  return hooks and hooks(...)
end

local function checkstate(ci, state)
  if ci.extra.servcmd_cs.state == state then return true end
  ci.extra.servcmd_cs = nil
  raise("servcmd_cs.state", { ci = ci, state = false })
end

local commands = {
  sdos_authchal = function(info)
    if not checkstate(info.ci, "solicited") then return end
    local challenge = info.text:match("sdos_authchal (.*)") or ""
    engine.sendpacket(info.ci.clientnum, 1, putf({30,r=1}, server.N_SERVCMD, "sdos_authans " .. engine.answerchallenge(module.key, challenge)):finalize(), -1)
    local servcmd_cs = info.ci.extra.servcmd_cs
    servcmd_cs.state, servcmd_cs.fences = "answered", {[fence(info.ci)] = true}
  end,
  sdos_authed = function(info)
    if not checkstate(info.ci, "answered") then return end
    local servcmd_cs = info.ci.extra.servcmd_cs
    servcmd_cs.state, servcmd_cs.fences = "ok", {}
    raise("servcmd_cs.state", { ci = info.ci, state = true })
  end
}

spaghetti.addhook(server.N_SERVCMD, function(info)
  if info.skip or info.ci.state.aitype ~= server.AI_NONE then return end
  local cmd = commands[info.text:match"^[^ ]+"]
  if not cmd or not info.ci.extra.servcmd_cs then return end
  info.skip = true
  return module.key and cmd(info)
end, true)

spaghetti.addhook("fence", function(info)
  local servcmd_cs = info.ci.extra.servcmd_cs
  if not servcmd_cs or not servcmd_cs.fences[info.fence] then return end
  if not checkstate(info.ci, "ok") then return end
  servcmd_cs.fences[info.fence] = nil
  return raise("servcmd_cs.ack", { ci = info.ci, fence = info.fence })
end)

function module.activate(ci)
  assert(module.key, "Missing private key.")
  assert(not ci.extra.servcmd_cs, "servcmd_cs already initialized")
  engine.sendpacket(ci.clientnum, 1, putf({14,r=1}, server.N_SERVCMD, "sdos_authtry"):finalize(), -1)
  ci.extra.servcmd_cs = { state = "solicited", fences = { [fence(ci)] = true } }
end

function module.send(ci, cs)
  local servcmd_cs = ci.extra.servcmd_cs
  assert(servcmd_cs and servcmd_cs.state == "ok", "servcmd_cs is not initialized")
  local text = "sdos_exec\n" .. cs
  assert(#text < engine.MAXTRANS, "cubescript chunk too long")
  engine.sendpacket(ci.clientnum, 1, putf({#text+1,r=1}, server.N_SERVCMD, text):finalize(), -1)
  servcmd_cs.fences[fence(ci)] = true
end

return module
