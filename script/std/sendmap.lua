--[[

  Force a sendmap on the client. Has rcs support to send a map and copy an existing cfg file.

]]--

local fp, L, rcs, putf, fence = require"utils.fp", require"utils.lambda", require"std.rcs", require"std.putf", require"std.fence"
local map = fp.map

local module = {}

local mapdata
local function sendmap(ci)
  if not mapdata then
    local mapdataf = assert(io.open("packages/base/" .. server.smapname .. ".ogz"), "Cannot open map " .. server.smapname)
    mapdata = assert(mapdataf:read("*a"), "Cannot read map " .. server.smapname)
    mapdataf:close()
  end
  engine.sendpacket(ci.clientnum, 2, putf({1 + #mapdata, r = 1}, server.N_SENDMAP, {buf = mapdata}):finalize(), -1)
end
spaghetti.addhook("changemap", function() mapdata = nil end)

local function removehooks(ci)
  if ci.extra.sendmaphooks then map.np(L"spaghetti.removehook(_2)", ci.extra.sendmaphooks) end
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

function module.sendcurrent(ci, usercs, mapcfg)
  assert(server.m_edit or (usercs and ci.extra.rcs), "Cannot send a map in non edit mode without rcs support")
  removehooks(ci)
  local hooks = {}
  if server.m_edit then
    sendmap(ci)
    hooks.sendhook = spaghetti.addhook(server.N_MAPCRC, function(info)
      if info.ci.clientnum ~= ci.clientnum or server.smapname ~= info.text or server.mcrc ~= 0 and server.mcrc ~= info.id % 2^32 then return end
      removehooks(ci)
      server.sendwelcome(ci)
    end)
  else
    engine.sendpacket(ci.clientnum, 1, putf({#server.smapname + 4, r=1}, server.N_MAPCHANGE, server.smapname, 1, 0):finalize(), -1)
    local fence = fence(ci)
    hooks.sendhook = spaghetti.addhook("fence", function(info)
      if info.ci.clientnum ~= ci.clientnum or fence ~= info.fence then return end
      sendmap(ci)
      spaghetti.removehook(hooks.sendhook)
      hooks.sendhook = spaghetti.addhook(server.N_MAPCRC, function(info)
        if info.ci.clientnum ~= ci.clientnum or server.smapname ~= info.text or server.mcrc ~= 0 and server.mcrc ~= info.crc % 2^32 then return end
        if not ci.extra.rcs then
          engine.writelog("sendmap: suddenly no rcs support " .. server.colorname(ci, nil))
          removehooks(ci)
          return
        end
        local id = math.random(2^32)
        rcs.send(ci, (mapcfg and cs_copycfg:format(mapcfg, server.smapname) or "") .. cs_savemap:format(server.smapname, id))
        spaghetti.removehook(hooks.sendhook)
        hooks.sendhook = spaghetti.addhook(server.N_SERVCMD, function(info)
          if info.ci.clientnum ~= ci.clientnum or info.text ~= "sendmap_restoremode_" .. id then return end
          info.skip = true
          server.sendwelcome(ci)
          spaghetti.removehook(hooks.sendhook)
          hooks.sendhook = spaghetti.addhook(server.N_MAPCRC, function(info)
            if info.ci.clientnum ~= ci.clientnum or server.smapname ~= info.text then return end
            if info.crc ~= 0 then info.crc, info.ci.warned = server.mcrc, false end
            removehooks(ci)
          end)
        end, true)
      end)
    end)
  end
  hooks.changemap = spaghetti.addhook("changemap", function() removehooks(ci) end)
  hooks.disconnect = spaghetti.addhook("clientdisconnect", function(info) return info.ci.clientnum == ci.clientnum and removehooks(ci) end)
  ci.extra.sendmaphooks = hooks
end

return module
