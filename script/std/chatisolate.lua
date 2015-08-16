--[[

  Mute spectators without privileges.

]]--

local map, L, iterators, putf, n_client = require"utils.fp", require"utils.lambda", require"std.iterators", require"std.putf", require"std.n_client"
local fp = map.fp

local hooks
local module = {}

function module.cantalkto(from, to)
  return not hooks or from.privilege > server.PRIV_NONE or from.state.state ~= engine.CS_SPECTATOR or to.state.state == engine.CS_SPECTATOR or to.privilege >= server.PRIV_ADMIN
end

local function on(on)
  if not not hooks == not not on then return end
  if not on then
    map.np(L"spaghetti.removehook(_2)", hooks)
    hooks = nil
    return
  end
  hooks = {}
  hooks.text = spaghetti.addhook(server.N_TEXT, function(info)
    if info.skip or not hooks or info.ci.privilege > server.PRIV_NONE or info.ci.state.state ~= engine.CS_SPECTATOR then return end
    info.skip = true
    local sendercn, text = info.ci.clientnum, engine.filtertext(info.text, true, true)
    local textp = n_client(putf({30, r=1}, server.N_TEXT, text), info.ci):finalize()
    for ci in iterators.clients() do if sendercn ~= ci.clientnum and module.cantalkto(info.ci, ci) then
      engine.sendpacket(ci.clientnum, 1, textp, -1)
    end end
    server.recordpacket(1, textp.data)
    engine.writelog(("%s: %s"):format(server.colorname(info.ci, nil), text))
  end)
  hooks.text = spaghetti.addhook(server.N_SAYTEAM, function(info)
    if info.skip or not hooks or info.ci.privilege > server.PRIV_NONE or info.ci.state.state ~= engine.CS_SPECTATOR or not server.m_teammode or info.ci.team == "" then return end
    info.skip = true
    local sendercn, text = info.ci.clientnum, engine.filtertext(info.text, true, true)
    local textp = putf({30, r=1}, server.N_SAYTEAM, sendercn, text):finalize()
    for ci in iterators.clients() do if sendercn ~= ci.clientnum and module.cantalkto(info.ci, ci) and ci.team == info.ci.team then
      engine.sendpacket(ci.clientnum, 1, textp, -1)
    end end
    engine.writelog(("%s <%s>: %s"):format(server.colorname(info.ci, nil), info.ci.team, text))
  end)
end

return setmetatable(module, {__call = on})
