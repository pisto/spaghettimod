--[[

  Mute spectators without privileges.

]]--

local map, L, iterators, putf, n_client = require"utils.fp", require"utils.lambda", require"std.iterators", require"std.putf", require"std.n_client"
local fp = map.fp

local hooks
local module = {}

function module.cantalkto(from, to)
  return not hooks or from.privilege > server.PRIV_NONE or from.state.state ~= engine.CS_SPECTATOR or to.state.state == engine.CS_SPECTATOR
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
    local sendercn = info.ci.clientnum
    local textp = n_client(putf({30, r=1}, server.N_TEXT, engine.filtertext(info.text, true, true)), info.ci):finalize()
    for sp in iterators.spectators() do if sendercn ~= sp.clientnum then engine.sendpacket(sp.clientnum, 1, textp, -1) end end
    server.recordpacket(1, textp.data)
  end)
  hooks.text = spaghetti.addhook(server.N_SAYTEAM, function(info)
    if info.skip or not hooks or info.ci.privilege > server.PRIV_NONE or info.ci.state.state ~= engine.CS_SPECTATOR then return end
    info.skip = true
    local sendercn = info.ci.clientnum
    local textp = putf({30, r=1}, server.N_SAYTEAM, sendercn, engine.filtertext(info.text, true, true)):finalize()
    for sp in iterators.spectators() do if sendercn ~= sp.clientnum and info.ci.team == sp.team then engine.sendpacket(sp.clientnum, 1, textp, -1) end end
  end)
end

return setmetatable(module, {__call = on})
