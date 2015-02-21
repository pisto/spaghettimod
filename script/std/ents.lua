--[[

  Entities manipulation library. Works only if entity data is available, so server.ments can be populated. The module is active from the next mapchange.
  Adds an extra event "entsloaded" to do your initialization.
  Also keeps track of which entities have been modified and sends the list to new players.

]]--

local L, fp, playermsg, putf, n_client, iterators = require"utils.lambda", require"utils.fp", require"std.playermsg", require"std.putf", require"std.n_client", require"std.iterators"
local map, first, range, pick = fp.map, fp.first, fp.range, fp.pick

local ents, updatedents = {}

local function checkstate()
  if not updatedents then error"module is not initialized" end
  local len = server.sents:length()
  if len ~= server.ments:length() then error"length inconsistency between sents and ments" end
  return len
end

function ents.active() return updatedents ~= nil end

local function callinithook()
  local hook = spaghetti.hooks.entsloaded
  if hook then hook({}) end
end

local emptyment = engine.entity()
emptyment.o.x, emptyment.o.y, emptyment.o.z =  -1e7,  -1e7,  -1e7
map.nv(function(f) emptyment[f] = 0 end, 'attr1', 'attr2', 'attr3', 'attr4', 'attr5', 'type', 'reserved')
local emptysent = server.server_entity()
emptysent.type, emptysent.spawntime, emptysent.spawned = server.NOTUSED, 0, false

spaghetti.addhook("loaditems", function(info)
  updatedents = nil
  if server.m_edit then return end
  info.skip = true
  local crc = server.loadents(server.smapname, server.ments)
  if not crc then return end
  server.mcrc = crc
  for i = 0, server.ments:length() - 1 do
    server.sents:put(emptysent)
    local sent, ment = server.sents[i], server.ments[i]
    if server.canspawnitem(ment.type) then
      sent.type = server.ments[i].type
      if server.delayspawn(sent.type) then sent.spawntime = server.spawntime(sent.type)
      else sent.spawned = true end
    end
  end
  server.notgotitems = false
  updatedents = {}
  callinithook()
end)

spaghetti.addhook("connected", function(info)
  if not updatedents then return end
  for _, i in ipairs(table.sort(map.lp(L"_", updatedents))) do updatedents[i](i, info.ci) end
end)

local function defaultsync(i, who)
  local sender = who or first(iterators.all())
  if not sender then return end
  local _, sent, ment = ents.getent(i)
  if not sent then sent, ment = emptysent, emptyment end
  local p = n_client(putf({ 20, r = 1}, server.N_EDITENT, i, ment.o.x * server.DMF, ment.o.y * server.DMF, ment.o.z * server.DMF, ment.type, ment.attr1, ment.attr2, ment.attr3, ment.attr4, ment.attr5), sender)
  engine.sendpacket(who and who.clientnum or -1, 1, p:finalize(), -1)
  return server.canspawnitem(sent.type) and ents.setspawn(i, sent.spawned)
end



function ents.getent(i)
  local len = checkstate()
  if i >= len then return end
  return i, server.sents[i], server.ments[i]
end

function ents.newent(type, o, a1, a2, a3, a4, a5, customsync)
  local len, new, sent, ment = checkstate()
  for i = 0, len - 1 do
    if server.ments[i].type == server.NOTUSED and (not updatedents[i] or updatedents[i] == defaultsync) then
      new, sent, ment = i, server.sents[i], server.ments[i]
      break
    end
  end
  if not new then
    if len >= engine.MAXENTS then return end
    new = server.sents:length()
    server.sents:put(emptysent)
    server.ments:put(emptyment)
    sent, ment = server.sents[new], server.ments[new]
  end
  return ents.editent(new, type, o, a1, a2, a3, a4, a5, customsync)
end

function ents.editent(i, type, o, a1, a2, a3, a4, a5, customsync)
  local len = checkstate()
  if i >= len then return ents.newent(type, a1, a2, a3, a4, a5) end
  local i, sent, ment = ents.getent(i)
  updatedents[i] = customsync or customsync == nil and updatedents[i] or defaultsync
  type, o = type or server.NOTUSED, o or emptyment.o
  if type == server.MAPMODEL and _G.type(a2) == "string" then a2 = ents.mapmodels[a2] or -1 end
  ment.type, ment.o.x, ment.o.y, ment.o.z, ment.attr1, ment.attr2, ment.attr3, ment.attr4, ment.attr5 = type, o.x, o.y, o.z, a1 or 0, a2 or 0, a3 or 0, a4 or 0, a5 or 0
  if sent.type ~= ment.type then
    sent.type, sent.spawntime, sent.spawned = server.NOTUSED, 0, false
    if server.canspawnitem(type) then
      sent.type = ment.type
      if server.delayspawn(type) then sent.spawntime = server.spawntime(type) end
    end
  end
  updatedents[i](i)
  return i, sent, ment
end

function ents.moveent(i, o)
  local len = checkstate()
  local i, sent, ment = ents.getent(i)
  if not i then error"entity does not exist" end
  return ents.editent(i, ment.type, o, ment.attr1, ment.attr2, ment.attr3, ment.attr4, ment.attr5, updatedents[i])
end

function ents.giveto(i, ci)
  if ci then
    local prepickup = spaghetti.hooks.prepickup
    if prepickup then
      local info = { skip = false, i = i, sender = ci.clientnum, ci = ci }
      prepickup(info)
      if info.skip then return end
    end
  end
  local _, sent = ents.getent(i)
  if sent then sent.spawned = false end
  engine.sendpacket(-1, 1, putf({4, r = 1}, server.N_ITEMACC, i, ci and ci.clientnum or -1):finalize(), -1)
  if ci then
    local pickup = spaghetti.hooks.pickup
    if pickup then pickup({ i = i, sender = ci.clientnum, ci = ci }) end
  end
end

function ents.setspawn(i, on, force)
  local _, sent = ents.getent(i)
  if not sent or (sent.spawned == not not on and not force) then return end
  if not on then return ents.giveto(i) end
  local preitemspawn = spaghetti.hooks.preitemspawn
  if preitemspawn then
    local info = { skip = false, ent = sent, i = i }
    preitemspawn(info)
    if info.skip then return end
  end
  sent.spawned = true
  engine.sendpacket(-1, 1, putf({4, r = 1}, server.N_ITEMSPAWN, i):finalize(), -1)
  local itemspawn = spaghetti.hooks.itemspawn
  if itemspawn then itemspawn({ ent = sent, i = i }) end
end

function ents.delent(i)
  local len = checkstate()
  if i >= len then return end
  local _, sent, ment = ents.editent(i, server.NOTUSED, emptyment.o, 0, 0, 0, 0, 0, defaultsync)
  server.ments[i].reserved = 0
  return i, sent, ment
end

function ents.enum(type)
  local len, ments = checkstate(), server.ments
  return map.fz(ents.getent, pick.fz(function(i)
    return i < ments:length() and (not type or ments[i].type == type)
  end, range.z(0, len - 1)))
end

function ents.sync(...)
  if select('#', ...) == 0 then map.np(L"_2(_1)", updatedents)
  else map.nv(function(i) (updatedents[i] or defaultsync)(i) end, ...) end
end

local default_map_models_f = assert(io.open("script/std/default_map_models.cfg"))
local default_map_models = assert(default_map_models_f:read("*a"))
default_map_models_f:close()
ents.mapmodels = {}
local mindex = 0
for mname in default_map_models:gmatch'mmodel "([^"]+)"' do
  ents.mapmodels[mname], ents.mapmodels[mindex] = mindex, mname
  mindex = mindex + 1
end

return ents
