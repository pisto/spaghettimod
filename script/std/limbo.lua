--[[

  Put the client in a limbo as if it wasn't sending N_CONNECT.
  Takes over control of server full and server password. Disables effectively ban checking, which must be reimplemented (see std.ban).

]]--

local L = require"utils.lambda"
local putf, iterators, playermsg, parsepacket = require"std.putf", require"std.iterators", require"std.playermsg", require"std.parsepacket"

local unlock_token = {}
local function locksetter(limbo, req, value)
  local meta = getmetatable(limbo)
  if meta[req] then
    if value ~= unlock_token then spaghetti.cancel(meta[req]) end
    meta.__count, meta[req] = meta.__count - 1, nil
  end
  if value and value ~= unlock_token and value > 0 then
    meta.__count, meta[req] = meta.__count + 1, spaghetti.later(value, function() limbo[req] = unlock_token end)
  elseif meta.__count == 0 and not meta.__entering then
    local ci = meta.__ci
    setmetatable(limbo, nil)
    ci.extra.limbo = false
    server.connected(ci)
  end
end

spaghetti.addhook("clientdisconnect", function(info)
  local limbo = info.ci.extra.limbo
  if not limbo then return end
  local meta = getmetatable(limbo)
  setmetatable(limbo, nil)
  meta.__count, meta.__newindex, meta.__ci, meta.__entering, meta.__maxclients = nil
  for _, unlocker in pairs(meta) do spaghetti.cancel(unlocker) end
end)

spaghetti.addhook(server.N_CONNECT, function(info)
  local ci = info.ci
  if ci.extra.limbo then info.skip = true return
  elseif ci.extra.limbo == false then return end
  info.skip = true
  ci.playermodel, ci.name = info.playermodel, engine.filtertext(info.text):sub(1, server.MAXNAMELEN):gsub("^$", "unnamed")
  local meta = { __ci = ci, __newindex = locksetter, __count = 0, __entering = true }
  ci.extra.limbo = setmetatable({}, meta)
  engine.sendpacket(ci.clientnum, 1, putf({ 1, r = 1}, server.N_WELCOME):finalize(), -1)
  if cs.serverpass ~= "" and not server.checkpassword(ci, cs.serverpass, info.password) then
    ci.extra.limbo.password = 1/0
    playermsg("You can use your (g)auth to bypass the password protection.", ci)
  end
  if server.numclients(-1, false, true) >= cs.maxclients then
    ci.extra.limbo.maxclients = 1/0
    getmetatable(ci.extra.limbo).__maxclients = true
    playermsg("Server is full. Use your (g)auth to join nevertheless.", ci)
  end
  spaghetti.hooks.enterlimbo{ ci = ci, password = info.password }
  if meta.__count == 0 then
    setmetatable(ci.extra.limbo, nil)
    ci.extra.limbo = false
    server.connected(ci)
  else
    meta.__entering = nil
    engine.writelog(("limbo: requested nick %s (%d)"):format(ci.name, ci.clientnum))
  end
end)

spaghetti.addhook("enterlimbo", function(info)
  return cs.adminpass ~= "" and server.checkpassword(info.ci, cs.adminpass, info.password) and server.setmaster(info.ci, true, "", nil, nil, server.PRIV_ADMIN, true)
end)

spaghetti.addhook("master", function(info)
  local limbo = info.ci.extra.limbo
  if not limbo or not (info.privilege == server.PRIV_ADMIN or info.authname) then return end
  limbo.password, limbo.maxclients = 0, 0
end)

spaghetti.addhook("connected", function()
  if server.numclients(-1, false, true) < cs.maxclients then return end
  for ci in iterators.connects() do
    ci.extra.limbo.maxclients = 1/0
    local meta = getmetatable(ci.extra.limbo)
    if not meta.__maxclients then
      playermsg("Server is full. Use your (g)auth to join nevertheless.", ci)
      meta.__maxclients = true
    end
  end
end)

spaghetti.addhook("clientdisconnect", function()
  if server.numclients(-1, false, true) - 1 >= cs.maxclients then return end
  for ci in iterators.connects() do
    ci.extra.limbo.maxclients = 0
    if server.numclients(-1, false, true) - 1 >= cs.maxclients then return end
  end
end)

--privileges gained during limbo aren't broadcast, synch when the client appears
spaghetti.addhook("connected", function(info)
  if info.ci.privilege == server.PRIV_NONE then return end
  local p = putf({ 30, r = 1}, server.N_CURRENTMASTER, server.mastermode)
  for ci in iterators.minpriv(server.PRIV_MASTER) do putf(p, ci.clientnum, ci.privilege) end
  engine.sendpacket(-1, 1, p:putint(-1):finalize(), -1)
end)

spaghetti.addhook("martian", function(info)
  if info.ci.connected or info.skip then return end
  info.skip = true
  if info.type == server.N_SETMASTER and not parsepacket(info) and info.mn == info.ci.clientnum and info.val == 1 then
    if cs.serverpass ~= "" and server.checkpassword(info.ci, cs.serverpass, info.text) then info.ci.extra.limbo.password = 0 end
    if cs.adminpass ~= "" and server.checkpassword(info.ci, cs.adminpass, info.text) then server.setmaster(info.ci, true, "", nil, nil, server.PRIV_ADMIN, true) end
  end
  local p = info.p
  p.len = p.maxlen
end)
