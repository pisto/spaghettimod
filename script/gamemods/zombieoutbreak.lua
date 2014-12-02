--[[

  Zombie outbreak mode: you get chaingun to stop a horde of bots with rockets and grenades, in slow motion.
  Mode needs to be efficteam or tacteam.

]]--

local fp, lambda, iterators, playermsg, putf, servertag, jsonpersist, n_client, ents, vec3, hitpush = require"utils.fp", require"utils.lambda", require"std.iterators", require"std.playermsg", require"std.putf", require"utils.servertag", require"utils.jsonpersist", require"std.n_client", require"std.ents", require"utils.vec3", require"std.hitpush"
local map, range, pick, breakk, L, Lr = fp.map, fp.range, fp.pick, fp.breakk, lambda.L, lambda.Lr

require"std.saveteam".on(true)

local module = {}
local hooks, active, gracetime, startent = {}

local function spawnzombie()
  local added = server.aiman.addai(0, -1)
  if not added then return end
  map.nf(L"_.name = 'zombie' _.team = 'evil'", iterators.bots())
end

local function blockteams(info)
  if not active or info.skip or info.ci.privilege >= server.PRIV_ADMIN then return end
  info.skip = true
  playermsg("Only admins can set teams in zombie mode", info.ci)
end

local function changeteam(ci, team)
  ci.team = engine.filtertext(team, false):sub(1, server.MAXTEAMLEN)
  server.addteaminfo(ci.team)
  server.aiman.changeteam(ci)
  engine.sendpacket(-1 ,1, putf({ 10, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_SETTEAM, ci.clientnum, ci.team, -1):finalize(), -1)
end

local function countscore(fieldname, mapsrecords)
  local topscore, players = 0, {}
  map.nf(function(ci)
    local scores = ci.extra.zombiescores
    if not scores then return end
    local score = scores[fieldname]
    if score == 0 then return end
    if score > topscore then topscore, players = score, {} end
    if score == topscore then table.insert(players, ci.name) end
  end, iterators.players())
  if not mapsrecords then return topscore, players end
  local record, new = mapsrecords[server.smapname] or {}
  record[fieldname] = record[fieldname] or {score = 0}
  if topscore > record[fieldname].score then mapsrecords[server.smapname], record[fieldname], new = record, { score = topscore, who = players }, true end
  return topscore, players, record[fieldname].score, record[fieldname].who, new
end

local function guydown(ci, chicken, persist)
  if server.interm > 0 then return end
  local hasgoods
  map.nf(function(ci)
    if ci.team == "good" and ci.state.state ~= engine.CS_SPECTATOR then hasgoods = true breakk() end
  end, iterators.players())
  if hasgoods then return not chicken and server.sendservmsg(server.colorname(ci, nil) .. " is now \f3zombie\f7!")
  else
    engine.sendpacket(-1, 1, putf({ 10, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_TEAMINFO, "evil", 666, "good", 0, ""):finalize(), -1)
    server.startintermission()
    server.sendservmsg("\f3" .. server.colorname(ci, nil) .. (chicken and " chickened out" or " died") .. "\f7, all hope is lost!")

    local record = persist and jsonpersist.load(servertag.fntag .. "zombierecords") or {}
    local slices, slicers, oldslices, oldslicers, recordslices = countscore("slices", record)
    if recordslices or slices ~= 0 then
      server.sendservmsg((recordslices and "\f6NEW MAP RECORD! " or "") .. "\f3Top zombie slicer: \f7" .. table.concat(slicers, ", ") .. " (" .. slices .. " zombie slices)")
    end
    if oldslices ~= 0 and not recordslices then
      server.sendservmsg("\f2Slices record holder\f7: " .. table.concat(oldslicers, ", ") .. " (" .. oldslices .. ")")
    end
    local kills, killers, oldkills, oldkillers, recordkills = countscore("kills", record)
    if recordkills or kills ~= 0 then
      server.sendservmsg((recordkills and "\f6NEW MAP RECORD! " or "") .. "\f3Rambo: \f7" .. table.concat(killers, ", ") .. " (" .. kills .. " zombies slayed)")
    end
    if oldkills ~= 0 and not recordkills then
      server.sendservmsg("\f2Rambo record holder\f7: " .. table.concat(oldkillers, ", ") .. " (" .. oldkills .. ")")
    end

    if recordslices or recordkills then jsonpersist.save(record, servertag.fntag .. "zombierecords") end
  end
end

function module.on(speed, spawninterval, persist)
  map.np(L"spaghetti.removehook(_2)", hooks)
  hooks = {}
  if not speed then return end

  hooks.autoteam = spaghetti.addhook("autoteam", function(info)
    active = server.m_teammode and (server.m_efficiency or server.m_tactics)
    if not active or info.skip then return end

    info.skip = true
    server.addteaminfo("good") server.addteaminfo("evil")
    map.nf(function(ci)
      ci.extra.zombiescores, ci.extra.teamrestored = nil
      changeteam(ci, "good")
    end, iterators.clients())
  end)
  hooks.changemap = spaghetti.addhook("changemap", function()
    startent = nil
    if not active then return end
    server.aiman.setbotbalance(nil, false)
    gracetime = true
    startent = ents.active() and ents.newent(server.NOTUSED, nil, 0, 0, 0, 0, 0, Lr"")
    spaghetti.latergame(3000, L"server.sendservmsg('\f3ZOMBIE OUTBREAK IN 10 SECONDS\f7! Take cover!\\n\f0Chainsaw is instakill\f7!')")
    spaghetti.latergame(10000, L"server.sendservmsg('\f3Zombies in \f23...')")
    spaghetti.latergame(11000, L"server.sendservmsg('\f22...')")
    spaghetti.latergame(12000, L"server.sendservmsg('\f21...')")
    spaghetti.latergame(13000, function()
      server.changegamespeed(speed, nil)
      server.sendservmsg('\f3Kill the zombies!')
      gracetime = nil
      spawnzombie()
      spaghetti.latergame(spawninterval, spawnzombie, true)
    end)
  end)

  hooks.setteam = spaghetti.addhook(server.N_SETTEAM, blockteams)
  hooks.switchteam = spaghetti.addhook(server.N_SWITCHTEAM, blockteams)
  hooks.delbot = spaghetti.addhook(server.N_DELBOT, function(info)
    if not active or info.skip or info.ci.privilege >= server.PRIV_ADMIN then return end
    info.skip = true
    playermsg("Only admins can delete zombies", info.ci)
  end)
  hooks.addbot = spaghetti.addhook(server.N_ADDBOT, function(info)
    if not active or info.skip then return end
    info.skip = true
    if info.ci.privilege < server.PRIV_ADMIN then playermsg("Only admins can add zombies", info.ci) end
    server.aiman.reqadd(info.ci, info.skill)
    map.nf(L"_.name = 'zombie' _.team = 'evil'", iterators.bots())
  end)
  hooks.botbalance = spaghetti.addhook(server.N_BOTBALANCE, function(info)
    if not active or info.skip then return end
    info.skip = true
    playermsg("Bot balance cannot be set in zombie mode", info.ci)
  end)

  hooks.damaged = spaghetti.addhook("notalive", function(info)
    if not active then return end
    map.nf(L"_.state.frags = (_.extra.zombiescores or { kills = 0 }).kills server.sendresume(_)", iterators.all())
    local ci = info.ci
    if ci.team ~= "good" or gracetime then return end
    changeteam(ci, "evil")
    guydown(ci, ci.state.state ~= engine.CS_DEAD, persist)
  end)
  hooks.spawnstate = spaghetti.addhook("spawnstate", function(info)
    if not active or info.skip then return end
    info.skip = true
    local st = info.ci.state
    for i = 0, server.NUMGUNS - 1 do st.ammo[i] = 0 end
    if info.ci.team == "good" then st.ammo[server.GUN_CG], st.ammo[server.GUN_SG], st.gunselect, st.health = 9999, 5, server.GUN_CG, 200
    else st.ammo[server.GUN_RL], st.ammo[server.GUN_GL], st.gunselect, st.health = 9999, 9999, server.GUN_RL, 90 end
    st.ammo[server.GUN_FIST], st.lifesequence, st.armourtype, st.armour = 1, (st.lifesequence + 1) % 0x80, server.A_BLUE, 0
  end)
  hooks.dodamage = spaghetti.addhook("dodamage", function(info)
    if not active or info.skip then return end
    if info.target.team == info.actor.team then
      if info.target.team == "evil" then info.skip = true
      else
        info.target = info.actor
        local push = info.hitpush
        push.x, push.y, push.z = 0, 0, 0
      end
      return
    end

    if info.actor.team ~= "good" then return end
    local scores = info.actor.extra.zombiescores or { slices = 0, kills = 0 }
    if info.gun == server.GUN_FIST then
      scores.slices = scores.slices + 1
      info.damage = 90
    end
    if info.target.state.health - info.damage <= 0 then scores.kills = scores.kills + 1 end
    info.actor.extra.zombiescores = scores
  end)


  hooks.npos = spaghetti.addhook(server.N_POS, function(info)
    if not active or info.skip or not info.cp then return end
    info.cp.extra.lastyaw = info.yaw
  end)
  hooks.damageeffects = spaghetti.addhook("damageeffects", function(info)
    local ci = info.target
    if not active or not startent or info.skip or ci.team == "evil" or info.actor.team == "good" or ci.state.health > 0 then return end
    changeteam(ci, "evil")
    info.actor.state.damage = info.actor.state.damage + info.damage
    server.spawnstate(ci)

    --ridiculous hack to force the player to spawn at a specified location: delete all player spawns, spawn, restore them.

    local p = { 100, engine.ENET_PACKET_FLAG_RELIABLE }
    local spawnpoints = map.sf(Lr"_", pick.zf(Lr"server.ments[_].type == server.PLAYERSTART and server.ments[_].attr2 == 0", range.z(0, server.ments:length() - 1)))
    map.np(function(i)   --suppress all spawnpoints
      p = putf(p, server.N_EDITENT, i, 0, 0, 0, server.NOTUSED, 0, 0, 0, 0, 0)
    end, spawnpoints)
    --inject position and yaw
    local o = ci.state.o
    p = putf(p, server.N_EDITENT, startent, o.x * server.DMF, o.y * server.DMF, o.z * server.DMF, server.PLAYERSTART, ci.extra.lastyaw or 0, 0, 0, 0, 0)
    --send spawn
    p = putf(p, server.N_SPAWNSTATE, ci.clientnum)
    server.sendstate(ci.state, p)
    --delete fake spawnpoint
    p = putf(p, server.N_EDITENT, startent, 0, 0, 0, server.NOTUSED, 0, 0, 0, 0, 0)
    --restore all spawnpoints
    map.np(function(i)   --restore them
      local ment = server.ments[i]
      local o = ment.o
      p = putf(p, server.N_EDITENT, i, o.x * server.DMF, o.y * server.DMF, o.z * server.DMF, server.PLAYERSTART, ment.attr1, 0, 0, 0, 0)
    end, spawnpoints)
    engine.sendpacket(ci.clientnum, 1, n_client(p, ci):finalize(), -1)
    --send notice to other clients too
    p = putf({ 30, engine.ENET_PACKET_FLAG_RELIABLE }, server.N_SPAWN)
    server.sendstate(ci.state, p)
    engine.sendpacket(-1, 1, n_client(p, ci):finalize(), ci.clientnum)

    guydown(ci, false, persist)
  end)
  hooks.shot = spaghetti.addhook("shot", function(info)
    if not active or info.this.gun ~= server.GUN_SG then return end
    info.dohits = false

    local p = {30, engine.ENET_PACKET_FLAG_RELIABLE}
    for i = 1, 5 do p = putf(p, server.N_SOUND, server.S_RLHIT) end
    engine.sendpacket(-1, 1, n_client(p, info.ci):finalize(), -1)

    local from, to = info.this.from, info.this.to
    local shotdir = vec3(to):sub(from):normalize()
    hitpush(info.ci, vec3(shotdir):mul(-200))
    map.nf(function(ci)
      if ci.state.state ~= engine.CS_ALIVE or ci.clientnum == info.ci.clientnum then return end
      local dist = vec3(ci.state.o):sub(from)
      local cos, distmag = shotdir:dot(vec3(dist):normalize()), dist:magnitude()
      if cos <= 0 or distmag >= 300 then return end
      local power = 1000 * cos * (1 - (distmag / 300)^3)
      hitpush(ci, vec3(shotdir):mul(power))
    end, iterators.players())
  end)

  hooks.connected = spaghetti.addhook("connected", function(info)
    if not active then return end
    if not info.ci.extra.saveteam then changeteam(info.ci, "good") end
    if info.ci.state.state ~= engine.CS_SPECTATOR then server.sendspawn(info.ci) end
  end)
  hooks.disconnect = spaghetti.addhook("clientdisconnect", function(info)
    if not active then return end
    local tot = 0
    map.nf(function(ci) tot = tot + (info.ci.clientnum == ci.clientnum and 0 or 1) end, iterators.clients())
    if tot == 0 then server.checkvotes(true) return end
    if gracetime then return end
    changeteam(info.ci, "evil")
    guydown(info.ci, true, persist)
  end)

end

return module
