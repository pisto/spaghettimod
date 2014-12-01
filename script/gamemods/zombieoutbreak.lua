--[[

  Zombie outbreak mode: you get chaingun to stop a horde of bots with rockets and grenades, in slow motion.
  Mode needs to be efficteam or tacteam.

]]--

local fp, lambda, iterators, playermsg, putf, servertag, jsonpersist = require"utils.fp", require"utils.lambda", require"std.iterators", require"std.playermsg", require"std.putf", require"utils.servertag", require"utils.jsonpersist"
local map, range, breakk, L, Lr = fp.map, fp.range, fp.breakk, lambda.L, lambda.Lr

require"std.saveteam".on(true)

local module = {}
local hooks, active, gracetime = {}

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
  local utf8map = engine.encodeutf8(server.smapname)
  local record, new = mapsrecords[utf8map] or { score = 0 }
  if topscore > record.score then mapsrecords[utf8map], record.score, record.who, new = record, topscore, map.li(Lr"engine.encodeutf8(_2)", players), true end
  return topscore, players, record.score, record.who, new
end

local function guydown(ci, chicken, persist)
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
      server.sendservmsg("\f2Slices record holder\f7: " .. table.concat(map.li(Lr"engine.decodeutf8(_2)", oldslicers), ", ") .. " (" .. oldslices .. ")")
    end
    local kills, killers, oldkills, oldkillers, recordkills = countscore("kills", record)
    if recordkills or kills ~= 0 then
      server.sendservmsg((recordkills and "\f6NEW MAP RECORD! " or "") .. "\f3Rambo: \f7" .. table.concat(killers, ", ") .. " (" .. kills .. " zombies slayed)")
    end
    if oldkills ~= 0 and not recordkills then
      server.sendservmsg("\f2Rambo record holder\f7: " .. table.concat(map.li(Lr"engine.decodeutf8(_2)", oldslicers), ", ") .. " (" .. oldkills .. ")")
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
    if not active then return end
    server.aiman.setbotbalance(nil, false)
    gracetime = true
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
    if info.ci.team == "good" then st.ammo[server.GUN_CG], st.gunselect, st.health = 9999, server.GUN_CG, 200
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
        push.x, push.y, push.z = -push.x, -push.y, -push.z
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

  hooks.connected = spaghetti.addhook("connected", function(info)
    if not active then return end
    if not info.ci.extra.saveteam then changeteam(info.ci, "good") end
    server.sendspawn(info.ci)
  end)
  hooks.disconnect = spaghetti.addhook("clientdisconnect", function(info)
    if not active then return end
    local tot = 0
    map.nf(function() tot = tot + 1 end, iterators.clients())
    if tot == 1 then server.checkvotes(true) return end
    if gracetime then return end
    changeteam(info.ci, "evil")
    guydown(info.ci, true, persist)
  end)

end

return module
