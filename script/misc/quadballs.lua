--[[

  Make a fireball follow a player with quad, like an aura.

]]--

local fp, lambda, ents, n_client, putf, iterators, quadarmour = require"utils.fp", require"utils.lambda", require"std.ents", require"std.n_client", require"std.putf", require"std.iterators", require"gamemods.quadarmour"
local map, L = fp.map, lambda.L

local function quadballclean(ci)
  if not ci.extra.quadball then return end
  map.np(L"spaghetti.removehook(_2)", ci.extra.quadball.hooks)
  ents.delent(ci.extra.quadball.i)
  ci.extra.quadball = nil
end
local function quadballleave_hook(info) quadballclean(info.ci) end

local colours = { 0xA40, 0xAA0, 0xAAA }   --quad, quadarmour, both
local function attachquadball(ci, colour)
  local sentreliable, owner, ciuuid = false, ci.clientnum, ci.extra.uuid
  local quadball = {}
  quadball.i = ents.newent(server.PARTICLES, ci.state.o, 3, 12, colour, 0, 0, function(i, who)
    local _, sent, ment = ents.getent(i)
    local p = n_client(putf({ 20, (sentreliable and not who) and 0 or engine.ENET_PACKET_FLAG_RELIABLE }, server.N_EDITENT, i, ment.o.x * server.DMF, ment.o.y * server.DMF, (ment.o.z + 4.5) * server.DMF, ment.type, ment.attr1, ment.attr2, ment.attr3, ment.attr4, ment.attr5), owner)
    engine.sendpacket(who and who.clientnum or -1, 1, p:finalize(), ci.ownernum)
    sentreliable = true
  end)

  quadball.hooks = {
    position = spaghetti.addhook(server.N_POS, function(info)
      if info.skip or info.pcn ~= owner or not info.cp then return end
      ents.moveent(quadball.i, info.pos)
    end),
    notalive = spaghetti.addhook("notalive", quadballleave_hook),
    clientdisconnect = spaghetti.addhook("clientdisconnect", quadballleave_hook),
    botleave = spaghetti.addhook("botleave", quadballleave_hook),
    endquad = spaghetti.addhook("worldupdate", function()
      colour = colours[(ci.state.quadmillis - engine.curtime > 0 and 1 or 0) + (ci.extra.quadarmour and 2 or 0)]
      if not colour then quadballclean(ci) end
    end),
    changemap = spaghetti.addhook("changemap", function() quadballclean(ci) end)
  }

  ci.extra.quadball = quadball

end

local module, hooks, itemballs = {}, {}, {}

function module.on(state)
  map.np(L"spaghetti.removehook(_2)", hooks)
  if ents.active() then map.np(function(i) ents.delent(i) end, itemballs) end
  hooks, itemballs = {}, {}
  map.nf(quadballclean, iterators.players())
  if not state then return end

  hooks.pickup = spaghetti.addhook("pickup", function(info)
    if not ents.active() then return end
    local _, qsent = ents.getent(info.i)
    if not qsent or qsent.type ~= server.I_QUAD then return end
    if info.ci.extra.quadball then return end
    if itemballs[info.i] then ents.delent(itemballs[info.i]) itemballs[info.i] = nil end
    attachquadball(info.ci, colours[info.quadarmour and 2 or 1])
  end)
  hooks.itemspawn = spaghetti.addhook("itemspawn", function(info)
    if not ents.active() then return end
    local _, sent, ment = ents.getent(info.i)
    if sent.type ~= server.I_QUAD then return end
    local o = {x = ment.o.x, y = ment.o.y, z = ment.o.z + 4}
    local i = ents.newent(server.PARTICLES, o, 3, 3, colours[quadarmour.isquadarmour(info.i) and 1 or 0])
    if i then itemballs[info.i] = i end
  end)
  
end

return module
