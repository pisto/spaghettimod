--[[

  Force a sendmap on the client. Has rcs support to send a map and copy an existing cfg file.

]]--

local fp, L, rcs, putf, fence, playermsg, parsepacket = require"utils.fp", require"utils.lambda", require"std.rcs", require"std.putf", require"std.fence", require"std.playermsg", require"std.parsepacket"
local map = fp.map

local module = {}

local mapdata
function module.hasmap()
  if not mapdata then
    local mapdataf = assert(io.open("packages/base/" .. server.smapname .. ".ogz"), "Cannot open map " .. server.smapname)
    mapdata = assert(mapdataf:read("*a"), "Cannot read map " .. server.smapname)
    mapdataf:close()
  end
  return not not mapdata
end

local function sendmap(ci)
  engine.sendpacket(ci.clientnum, 2, putf({1 + #mapdata, r = 1}, server.N_SENDMAP, {buf = mapdata}):finalize(), -1)
end
spaghetti.addhook("changemap", function() mapdata = nil end)

local function removehooks(ci)
  for _, hook in pairs(ci.extra.sendmaphooks or {}) do spaghetti.removehook(hook) end
  ci.extra.sendmaphooks = nil
end

local cs_copycfg = [[
textfocus "rcs_copycfg"
textload packages/base/%s.cfg
textsave packages/base/%s.cfg
]]

local cs_savemap = [[
local oldbak
oldback = $savebak
savebak 0
resetvar maptitle
savemap %s
savebak $oldbak
servcmd sendmap_restoremode_%d
]]

local editmsg = map.sv(L"server[_]", "N_REMIP", "N_NEWMAP", "N_GETMAP", "N_SENDMAP", "N_CLIPBOARD", "N_EDITENT", "N_EDITF", "N_EDITT", "N_EDITM", "N_FLIP", "N_COPY", "N_PASTE", "N_ROTATE", "N_REPLACE", "N_DELCUBE", "N_EDITVAR")

function module.forcecurrent(ci, keepedit, usercs, mapcfg)
  assert(not server.m_edit)
  assert(keepedit or usercs and ci.extra.rcs, "Cannot send map without rcs in non coopedit fake mode")
  assert(keepedit or not mapcfg or type(mapcfg) == "string", "mapcfg must be a filename to copy")
  removehooks(ci)
  local hooks = { martian = spaghetti.addhook("martian", function(info)
    if info.skip or not editmsg[info.type] or info.p:overread() then return end
    if info.type == server.N_EDITVAR then
      info.skip = not parsepacket(info)
      return
    end
    info.p.len, info.skip = info.p.maxlen, true
    playermsg("\f6You are not supposed to use edit commands\f7. Coopedit mode has been used only to send a custom map.", info.ci)
    engine.enet_peer_disconnect_later(engine.getclientpeer(ci.clientnum), engine.DISC_KICK)
    server.sendservmsg("\f4kicked for editing:\f7 " .. server.colorname(info.ci, nil))
  end) }
  engine.sendpacket(ci.clientnum, 1, putf({#server.smapname + 4, r=1}, server.N_MAPCHANGE, server.smapname, 1, 0):finalize(), -1)
  local fence = fence(ci)
  hooks.sendhook = spaghetti.addhook("fence", function(info)
    if info.ci.clientnum ~= ci.clientnum or fence ~= info.fence then return end
    sendmap(ci)
    spaghetti.removehook(hooks.sendhook)
    hooks.sendhook = spaghetti.addhook(server.N_MAPCRC, function(info)
      if info.ci.clientnum ~= ci.clientnum or server.smapname ~= info.text or server.mcrc ~= 0 and server.mcrc ~= info.crc % 2^32 then return end
      spaghetti.removehook(hooks.sendhook) hooks.sendhook = nil
      if keepedit then
        if ci.state.state ~= engine.CS_SPECTATOR then server.sendspawn(ci) end
        info.ci.warned = false
        local p
        for i = 0, server.sents:length() - 1 do
          p = p or {30, r=1}
          if server.sents[i].spawned then p = putf(p, server.N_ITEMSPAWN, i)
          else p = putf(p, server.N_ITEMACC, i, -1) end
        end
        if p then engine.sendpacket(ci.clientnum, 1, p:finalize(), -1) end
        if not mapcfg or not usercs or not ci.extra.rcs then return end
        local mapcfgf = io.open("packages/base/" .. server.smapname .. ".cfg")
        if not mapcfgf then return end
        rcs.send(ci, mapcfgf:read("*a"))
        mapcfgf:close()
      else
        if not ci.extra.rcs then
          engine.writelog("sendmap: suddenly no rcs support " .. server.colorname(ci, nil))
          removehooks(ci)
          server.sendwelcome(ci)
          return
        end
        local id = math.random(2^32)
        rcs.send(ci, (mapcfg and cs_copycfg:format(mapcfg, server.smapname) or "") .. cs_savemap:format(server.smapname, id))
        hooks.sendhook = spaghetti.addhook(server.N_SERVCMD, function(info)
          if info.ci.clientnum ~= ci.clientnum or info.text ~= "sendmap_restoremode_" .. id then return end
          info.skip = true
          server.sendwelcome(ci)
          spaghetti.removehook(hooks.sendhook)
          hooks.sendhook = spaghetti.addhook(server.N_MAPCRC, function(info)
            if info.ci.clientnum ~= ci.clientnum or server.smapname ~= info.text then return end
            if info.crc ~= 0 then info.crc, info.ci.warned = server.mcrc, false end
            spaghetti.removehook(hooks.sendhook) hooks.sendhook = nil
          end)
        end, true)
      end
    end)
  end)
  hooks.changemap = spaghetti.addhook("changemap", function() removehooks(ci) end)
  hooks.disconnect = spaghetti.addhook("clientdisconnect", function(info) return info.ci.clientnum == ci.clientnum and removehooks(ci) end)
  ci.extra.sendmaphooks = hooks
end

return module
