--[[

  Edit variables manipulation and synch library, similar to std.ents.

]]--

local fp, iterators, putf, n_client = require"utils.fp", require"std.iterators", require"std.putf", require"std.n_client"
local first = fp.first

local module, updatedvars = {}, {}

local function defaultsync(var, value, who)
  local sender = who or first(iterators.all())
  if not sender then return end
  local t = type(value)
  if t == "string" then t = engine.ID_SVAR
  elseif t == "number" then t = engine.ID_VAR
  elseif t.float then t = engine.ID_FVAR
  else error("Unknown var type") end 
  local p = n_client(putf({ 20, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_EDITVAR, t, var, value), sender)
  engine.sendpacket(who and who.clientnum or -1, 1, p:finalize(), -1)
end

spaghetti.addhook("changemap", function() updatedvars = {} end)

function module.editvar(var, value, sync)
  updatedvars[var] = { value = value, sync = sync or defaultsync }
  updatedvars[var].sync(var, value)
end

spaghetti.addhook("connected", function(info)
  for var, v in pairs(updatedvars) do v.sync(var, v.value, info.ci) end
end)

return module
