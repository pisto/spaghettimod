--[[

  Emulated parser for packets, to be used by martian hooks.

]]--

local getf = require"std.getf"

local parser = {
  [server.N_CONNECT] = function(info) info.text, info.playermodel, info.password, info.authdesc, info.authname = getf(info.p, "sisss") end,
  [server.N_TEXT] = function(info) info.text = getf(info.p, "s") end,
  [server.N_SERVCMD] = function(info) info.text = getf(info.p, "s") end,
  [server.N_AUTHTRY] = function(info) info.desc, info.name = getf(info.p, "ss") end,
  [server.N_AUTHANS] = function(info) info.desc, info.id, info.ans = getf(info.p, "sis") end,
  [server.N_EDITVAR] = function(info)
    info.stringval, info.numval, info.type_editvar, info.text = "", 0, getf(info.p, "is")
    if info.type_editvar == engine.ID_SVAR then info.stringval = getf(info.p, "s")
    elseif info.type_editvar == engine.ID_VAR then info.numval = getf(info.p, "i")
    elseif info.type_editvar == engine.ID_FVAR then info.numval = getf(info.p, "f") end
  end,
}

return function(info)
  if info.malformed == nil then
    assert(parser[info.type], "Parsing not implemented for type " .. info.type)(info)
    info.malformed = info.malformed or info.p:overread()
  end
  return info.malformed
end
