--[[

  Quad armour: replace the standard quad with a temporary armour that cannot be consumed.
  Damage divisor and armour type/points after the effect vanishes can be set.
  Uses a fake armourtype = 11 because the client doesn't check for bounds when drawing the icon, so it will draw a quad.

]]--

local fp, lambda, putf, n_client, playermsg, uuid, sound = require"utils.fp", require"utils.lambda", require"std.putf", require"std.n_client", require"std.playermsg", require"std.uuid", require"std.sound"
local map, L, Lr = fp.map, lambda.L, lambda.Lr

local hooks, quadarmours = {}, {}

local A_QUAD = 11

local function quadarmourcleanup(ci)
  local quadarmour = ci.extra.quadarmour
  if not quadarmour then return end
  map.np(L"spaghetti.removehook(_2)", quadarmour.hooks)
  ci.extra.quadarmour = nil
end

local function on(probability, divisor, armour, duration, maxduration, decayarmourtype, decayarmour)
  map.np(L"spaghetti.removehook(_2)", hooks)
  hooks, quadarmours = {}, {}
  if not probability or probability <= 0 then return end
  probability = math.min(probability, 1)

  local function decay(ci)
    local st = ci.state
    st.armourtype, st.armour = decayarmourtype, decayarmour
    server.sendresume(ci)
    sound(ci, server.S_PUPOUT)
    if st.aitype == server.AI_NONE then playermsg("\f2quad armour is over", ci) end
    quadarmourcleanup(ci)
  end

  hooks.announce = spaghetti.addhook("announce", function(info)
    if info.ent.type ~= server.I_QUAD or math.random() > probability then return end
    quadarmours[info.i] = true
    server.sendservmsg"\f2It will be a \f6QUAD ARMOUR\f2!"
  end)

  hooks.pickup = spaghetti.addhook("prepickup", function(info)
    if not quadarmours[info.i] then return end
    info.skip = true

    local sent = server.sents[info.i]
    if (server.m_timed and server.gamemillis >= server.gamelimit) or not sent.spawned or not info.ci or (info.ci.extra.quadarmour and info.ci.extra.quadarmour.millis >= maxduration) then return end
    sent.spawned, sent.spawntime = false, server.spawntime(sent.type)
    engine.sendpacket(-1, 1, putf({4, engine.ENET_PACKET_FLAG_RELIABLE}, server.N_ITEMACC, info.i, -1):finalize(), -1)

    local st = info.ci.state
    st.quadmillis, st.armourtype, st.armour = 0, A_QUAD, armour > 0 and armour or 9001
    server.sendresume(info.ci)
    sound(info.ci, server.S_V_QUAD)
    if st.aitype == server.AI_NONE then playermsg("\f2you got the \f6QUAD ARMOUR\f2!", info.ci) end
    quadarmours[info.i] = nil
    local quadarmour = info.ci.extra.quadarmour or {}
    quadarmour.millis = math.min((quadarmour.millis or 0) + duration, maxduration)
    if quadarmour.hooks then return end
    info.ci.extra.quadarmour = quadarmour

    local ci = info.ci
    quadarmour.hooks = {}
    quadarmour.hooks.updatehook = spaghetti.addhook("worldupdate", function()
      if quadarmour.millis - engine.curtime <= 0 then decay(ci)
      else quadarmour.millis = quadarmour.millis - engine.curtime end
    end)
    quadarmour.hooks.changemaphook = spaghetti.addhook("changemap", function() quadarmourcleanup(ci) end)
    quadarmour.hooks.notalive = spaghetti.addhook("notalive", function(info) quadarmourcleanup(info.ci) end)
    quadarmour.hooks.disconnect = spaghetti.addhook("clientdisconnect", function(info) quadarmourcleanup(info.ci) end)
    quadarmour.hooks.botleave = spaghetti.addhook("botleave", function(info) quadarmourcleanup(info.ci) end)

    local pickuphooks = spaghetti.hooks.pickup
    if pickuphooks then pickuphooks({ quadarmour = true, sender = info.sender, i = info.i, ci = info.ci }) end
  end)

  hooks.dodamage = spaghetti.addhook("dodamage", function(info)
    if info.skip or not info.target.extra.quadarmour then return end
    info.skip = true
    local st, absorbed, actual = info.target.state
    if armour > 0 then
      absorbed = info.damage * (1 - 1 / divisor)
      if absorbed > st.armour then st.armour, actual = 0, info.damage - st.armour / (1 - 1 / divisor)
      else st.armour, actual = st.armour - absorbed, info.damage - absorbed end
    else actual = info.damage / divisor end
    st.health = st.health - actual
    if st.armour == 0 then decay(info.target) end
  end)

end

return { on = on, isquadarmour = function(i) return not not quadarmours[i] end }
