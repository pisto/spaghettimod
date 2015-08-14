--[[

  Commands to set various attributes of a player.

]]--

local fp, L, commands, playermsg, spawnat, putf, lastpos = require"utils.fp", require"utils.lambda", require"std.commands", require"std.playermsg", require"std.spawnat", require"std.putf", require"std.lastpos"
local map = fp.map

local module = { cmdprivilege = server.PRIV_ADMIN }

local function makesetter(field)
  commands.add("set" .. field, function(info)
    if not module.cmdprivilege or info.ci.privilege < module.cmdprivilege then playermsg("Insufficient privilege.", info.ci) return end
    local ct, increment, score = info.args:match("^%s*(%d+)%s+([%-%+]?)(%d+)%s*$")
    if not ct then playermsg("Invalid format.", info.ci) return end
    ct = server.getinfo(tonumber(ct) or -1)
    if not ct then playermsg("Player not found.", info.ci) return end
    ct.state[field] = (increment ~= "" and ct.state[field] or 0) + (increment == "-" and -1 or 1) * score
    server.sendresume(ct)
    server.sendservmsg(server.colorname(info.ci, nil) .. " set " .. score .. " " .. field .. " to player " .. server.colorname(ct, nil))
  end, ("#set%s <cn> [+-]<%s>: set %s of player (+/- for increment)"):format(field, field, field))
end

makesetter("frags")
makesetter("flags")
makesetter("health")
makesetter("maxhealth")
makesetter("armour")

function module.syncammo(ci)
  if server.m_capture then
    ci.state.lifesequence = (ci.state.lifesequence + 1) % 0x80
    server.sendresume(ci)
    local p, st = { 20, r = 1}, ci.state
    for type = server.GUN_SG, server.GUN_PISTOL do p = putf(p, server.N_BASEREGEN, ci.clientnum, st.health, st.armour, type, st.ammo[type]) end
    engine.sendpacket(-1, 1, p:finalize(), -1)
  else spawnat(ci, ci.state.o, ci.extra.lastpos.yaw) end
end

local ammonames = map.mv(L"_, server['GUN_' .. _:upper()]", "sg", "cg", "rl", "rifle", "gl", "pistol")
ammonames.shotgun, ammonames.chaingun, ammonames.rockets, ammonames.rocket, ammonames.rocketlauncher, ammonames.grenades, ammonames.grenade, ammonames.grenadelauncher = ammonames.sg, ammonames.cg, ammonames.rl, ammonames.rl, ammonames.rl, ammonames.gl, ammonames.gl, ammonames.gl
commands.add("setammo", function(info)
  if not module.cmdprivilege or info.ci.privilege < module.cmdprivilege then playermsg("Insufficient privilege.", info.ci) return end
  local ct, ammoname, increment, ammo = info.args:match("^%s*(%d+)%s+(%S+)%s+([%-%+]?)(%d+)%s*$")
  if not ct then playermsg("Invalid format.", info.ci) return end
  ct = server.getinfo(tonumber(ct) or -1)
  if not ct then playermsg("Player not found.", info.ci) return end
  local ammoidx = ammonames[ammoname:lower()]
  if not ammoidx then playermsg("Ammo type not found.", info.ci) return end
  local st = ct.state
  st.ammo[ammoidx] = (increment ~= "" and st.ammo[ammoidx] or 0) + (increment == "-" and -1 or 1) * ammo
  module.syncammo(ct)
  server.sendservmsg(server.colorname(info.ci, nil) .. " set " .. ammo .. " " .. ammoname .. " to player " .. server.colorname(ct, nil))
end, "#setammo <cn> <ammoname> [+-]<ammo>: set ammo of player (+/- for increment)")

local armournames = map.mv(L"_, server['A_' .. _:upper()]", "blue", "green", "yellow")
armournames.ba, armournames.ga, armournames.ya = armournames.blue, armournames.green, armournames.yellow
local armourdefaults = { [server.A_BLUE] = 50, [server.A_GREEN] = 100, [server.A_YELLOW] = 200 }
commands.add("setarmourtype", function(info)
  if not module.cmdprivilege or info.ci.privilege < module.cmdprivilege then playermsg("Insufficient privilege.", info.ci) return end
  local ct, type, value = info.args:match("^%s*(%d+)%s+(%S+)%s*(%d*)%s*$")
  if not ct then playermsg("Invalid format.", info.ci) return end
  ct = server.getinfo(tonumber(ct) or -1)
  if not ct then playermsg("Player not found.", info.ci) return end
  local armouridx = armournames[type:lower()]
  if not armouridx then playermsg("Armour type not found.", info.ci) return end
  local st = ct.state
  st.armourtype, st.armour = armouridx, value ~= "" and value or armourdefaults[armouridx]
  server.sendresume(ct)
  server.sendservmsg(server.colorname(info.ci, nil) .. " set armour " .. type .. ":" .. st.armour .. " to player " .. server.colorname(ct, nil))
end, "#setammo <cn> <armourtype> [value]: set armour type of player (omitted armour value means default pickup value)")

return module
