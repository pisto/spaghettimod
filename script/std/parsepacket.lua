--[[

  Emulated parser for packets, to be used by martian hooks.

]]--

local getf = require"std.getf"

local parser = {
  [server.N_CONNECT] = function(info) info.text, info.playermodel, info.password, info.authdesc, info.authname = getf(info.p, "sisss") end,
  [server.N_TEXT] = function(info) info.text = getf(info.p, "s") end,
  [server.N_AUTHTRY] = function(info) info.desc, info.name = getf(info.p, "ss") end,
  [server.N_AUTHANS] = function(info) info.desc, info.id, info.ans = getf(info.p, "sis") end,
}

return function(info)
  if info.malformed == nil then
    assert(parser[info.type], "Parsing not implemented for type " .. info.type)(info)
    info.malformed = info.malformed or info.p:overread()
  end
  return info.malformed
end
