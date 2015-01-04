--[[

  Attach an entity to a player.

]]--

local fp, lambda, ents, n_client, putf, iterators = require"utils.fp", require"utils.lambda", require"std.ents", require"std.n_client", require"std.putf", require"std.iterators"
local map, first, L = fp.map, fp.first, lambda.L

require"std.lastpos"
local module = {}

function module.remove(ci, i)
  local trackents = ci.extra.trackents
  if not trackents or not trackents.ents[i] then return end
  trackents.ents[i] = nil
  ents.delent(i)
  if not next(trackents.ents) then
    map.np(L"spaghetti.removehook(_2)", trackents.hooks)
    ci.extra.trackents = nil
  end
end

local function trackclean(ci)
  if not ci.extra.trackents then return end
  for i in pairs(ci.extra.trackents.ents) do module.remove(ci, i) end
end
local function trackclean_hook(info) trackclean(info.ci) end

function module.add(ci, entityupdater, reliable, hideself)
  local trackent, owner = {}, ci.clientnum
  local i = ents.newent(server.NOTUSED, nil, 0, 0, 0, 0, 0, function(i, who)
    local _, sent, ment = ents.getent(i)
    local p = n_client(putf({ 20, reliable and engine.ENET_PACKET_FLAG_RELIABLE or 0 }, server.N_EDITENT, i, ment.o.x * server.DMF, ment.o.y * server.DMF, ment.o.z * server.DMF, ment.type, ment.attr1, ment.attr2, ment.attr3, ment.attr4, ment.attr5), first(iterators.all()))
    engine.sendpacket(who and who.clientnum or -1, 1, p:finalize(), hideself and ci.ownernum or -1)
  end)
  if not i then return end

  if not ci.extra.trackents then
    ci.extra.trackents = { hooks = {
      position = spaghetti.addhook("positionupdate", function(info)
        if info.cp.clientnum ~= owner then return end
        for i, entityupdater in pairs(ci.extra.trackents.ents) do entityupdater(i, info.lastpos) end
      end),
      notalive = spaghetti.addhook("notalive", trackclean_hook),
      clientdisconnect = spaghetti.addhook("clientdisconnect", trackclean_hook),
      botleave = spaghetti.addhook("botleave", trackclean_hook),
      changemap = spaghetti.addhook("changemap", function() trackclean(ci) end)
    }, ents = {} }
  end
  ci.extra.trackents.ents[i] = entityupdater
  return i

end

return module
