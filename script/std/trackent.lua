--[[

  Attach an entity to a player.

]]--

local fp, L, ents, n_client, putf = require"utils.fp", require"utils.lambda", require"std.ents", require"std.n_client", require"std.putf"
local map, first = fp.map, fp.first

require"std.lastpos"
require"std.notalive"
local module = {}

function module.remove(ci, i, nodelent)
  local trackents = ci.extra.trackents
  if not trackents or not trackents.ents[i] then return end
  trackents.ents[i] = nil
  if not nodelent then ents.delent(i) end
  if not next(trackents.ents) then
    map.np(L"spaghetti.removehook(_2)", trackents.hooks)
    ci.extra.trackents = nil
  end
end

local function trackclean(ci, nodelent)
  if not ci.extra.trackents then return end
  for i in pairs(ci.extra.trackents.ents) do module.remove(ci, i, nodelent) end
end
local function trackclean_hook(info) trackclean(info.ci) end

local function makeeditent(p, i)
  local _, _, ment = ents.getent(i)
  return putf(p, server.N_EDITENT, i, ment.o.x * server.DMF, ment.o.y * server.DMF, ment.o.z * server.DMF, ment.type, ment.attr1, ment.attr2, ment.attr3, ment.attr4, ment.attr5)
end

local function makenonblindlist(blindcns)
  local l = {}
  for i = 0, server.clients:length() - 1 do
    local ci = server.clients[i]
    if ci.state.aitype == server.AI_NONE and not blindcns[ci.clientnum] then l[ci.clientnum] = true end
  end
  return l
end

function module.add(ci, updater, reliable, hideself, blindcns)
  local trackent, owner = {}, ci.clientnum
  local i = ents.newent(server.NOTUSED, nil, 0, 0, 0, 0, 0, L"")
  if not i then return end

  if not ci.extra.trackents then
    ci.extra.trackents = { hooks = {
      position = spaghetti.addhook("positionupdate", function(info)
        if info.cp.clientnum ~= owner then return end
        for i, track in pairs(ci.extra.trackents.ents) do track.updater(i, info.lastpos) end
      end, true),
      worldstate_pos = spaghetti.addhook("worldstate_pos", function(info)
        if info.ci.clientnum ~= ci.clientnum or ci.position:empty() then return end
        local p = { [true] = {}, [false] = {} }
        for i, t in pairs(ci.extra.trackents.ents) do
          if t.blindcns and next(t.blindcns) then
            local nonblindlist = makenonblindlist(t.blindcns)
            nonblindlist[ci.clientnum] = not t.hideself or nil
            if next(nonblindlist) then
              local p = n_client(makeeditent({r = t.reliable}, i), ci):finalize()
              for cn in pairs(nonblindlist) do engine.sendpacket(cn, 1, p, -1) end
              server.recordpacket(1, p.data)
            end
          elseif t.reliable and t.hideself then makeeditent(ci.messages, i)
          else p[t.hideself][t.reliable] = makeeditent(p[t.hideself][t.reliable] or { r = t.reliable }, i) end
        end
        for hideself, p in pairs(p) do for _, p in pairs(p) do
          engine.sendpacket(-1, 1, n_client(p, ci):finalize(), hideself and ci.clientnum or -1)
        end end
      end, true),
      notalive = spaghetti.addhook("notalive", function(info)
        if info.ci.clientnum ~= ci.clientnum then return end
        for i in pairs(ci.extra.trackents.ents) do
          engine.sendpacket(-1, 1, n_client(putf({ 20, r = 1}, server.N_EDITENT, i, -1e7,  -1e7,  -1e7, server.NOTUSED, 0, 0, 0, 0, 0), ci):finalize(), -1)
        end
      end, true),
      clientdisconnect = spaghetti.addhook("clientdisconnect", trackclean_hook, true),
      botleave = spaghetti.addhook("botleave", trackclean_hook, true),
      changemap = spaghetti.addhook("changemap", function() trackclean(ci, true) end, true)
    }, ents = {} }
  end
  ci.extra.trackents.ents[i] = { updater = updater, reliable = not not reliable, hideself = not not hideself, blindcns = blindcns }
  return i

end

return module
