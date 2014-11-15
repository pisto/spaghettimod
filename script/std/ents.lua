--[[

  Entities manipulation library. Works only if entity data is available, so server.ments can be populated. The module is active from the next mapchange.
  Adds an extra event "entsloaded" to do your initialization.
  Also keeps track of which entities have been modified and sends the list to new players.

]]--

local lambda, fp, playermsg, putf, n_client = require"utils.lambda", require"utils.fp", require"std.playermsg", require"std.putf", require"std.n_client"
local map, L, Lr = fp.map, fp.last, fp.fold, lambda.L, lambda.Lr

local ents, updatedents = {}

local function checkstate()
  if not updatedents then error"module is not initialized" end
  local len = server.sents:length()
  if len ~= server.ments:length() then error"length inconsistency between sents and ments" end
  return len
end

local function callinithook()
  local hook = spaghetti.hooks.entsloaded
  if hook then hook({}) end
end

local emptyment = engine.entity()
emptyment.o.x, emptyment.o.y, emptyment.o.z =  -1e7,  -1e7,  -1e7
map.nv(function(f) emptyment[f] = 0 end, 'attr1', 'attr2', 'attr3', 'attr4', 'attr5', 'type', 'reserved')
local emptysent = server.server_entity()
emptysent.type, emptysent.spawntime, emptysent.spawned = server.NOTUSED, 0, 0

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
      else sent.spawned = 1 end
    end
  end
  server.notgotitems = false
  updatedents = {}
  callinithook()
end)

spaghetti.addhook("connected", function(info)
  if not updatedents then return end
  map.np(function(i, f) f(i, info.ci) end, updatedents)
end)

local function defaultsync(i, who)
  local sender = who or server.clients[0]
  if not sender then return end
  local _, sent, ment = ents.getent(i)
  if not sent then sent, ment = emptysent, emptyment end
  local p = n_client(putf({ 20, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_EDITENT, i, ment.o.x * server.DMF, ment.o.y * server.DMF, ment.o.z * server.DMF, ment.type, ment.attr1, ment.attr2, ment.attr3, ment.attr4, ment.attr5), sender)
  if server.canspawnitem(sent.type) then
    if sent.spawned ~= 0 then putf(p, server.N_ITEMSPAWN, i)
    else putf(server.N_ITEMACC, i, -1) end
  end
  engine.sendpacket(who and who.clientnum or -1, 1, p:finalize(), -1)
end



function ents.getent(i)
  local len = checkstate()
  if i >= len then return end
  return i, server.sents[i], server.ments[i]
end

function ents.newent(type, o, a1, a2, a3, a4, a5, customsync)
  local len, new, sent, ment = checkstate()
  for i = 0, len - 1 do
    if server.ments[i].type == server.NOTUSED then
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
  updatedents[i] = customsync or defaultsync
  o = o or emptyment.o
  ment.type, ment.o.x, ment.o.y, ment.o.z, ment.attr1, ment.attr2, ment.attr3, ment.attr4, ment.attr5 = type or server.NOTUSED, o.x, o.y, o.z, a1 or 0, a2 or 0, a3 or 0, a4 or 0, a5 or 0
  if sent.type ~= ment.type then
    sent.type, sent.spawntime, sent.spawned = server.NOTUSED, 0, 0
    if server.canspawnitem(type) then
      sent.type = ment.type
      if server.delayspawn(type) then sent.spawntime = server.spawntime(type)
      else sent.spawned = 1 end
    end
  end
  updatedents[i](i)
end

function ents.delent(i)
  local len = checkstate()
  if i >= len then return end
  ents.editent(i)
  server.ments[i].reserved = 0
end

function ents.sync(...)
  if select('#', ...) == 0 then map.np(L"_2(_1)", updatedents)
  else map.nv(function(i) (updatedents[i] or defaultsync)(i) end, ...) end
end

return ents
