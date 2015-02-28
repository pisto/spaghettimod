--[[

  Zombie outbreak mode: you get chaingun to stop a horde of bots with rockets and grenades, in slow motion.
  The "ammo" parameter must be a callback which takes a clientinfo and fills its state.
  Mode needs to be efficteam or tacteam.

]]--

local fp, L, iterators, playermsg, putf, servertag, jsonpersist, n_client, ents, vec3, sound, commands = require"utils.fp", require"utils.lambda", require"std.iterators", require"std.playermsg", require"std.putf", require"utils.servertag", require"utils.jsonpersist", require"std.n_client", require"std.ents", require"utils.vec3", require"std.sound", require"std.commands"
local map, range, pick = fp.map, fp.range, fp.pick

require"std.lastpos"
require"std.notalive"

local module = {}
local hooks, healthdrops, spawnedhealths, fires, gracetime, killbasesp = {}

local function blockteams(info)
  if not server.m_regencapture or info.skip or info.ci.privilege >= server.PRIV_ADMIN then return end
  info.skip = true
  playermsg("Only admins can set teams in zombie mode", info.ci)
end

local function changeteam(ci, team)
  ci.team = engine.filtertext(team, false):sub(1, server.MAXTEAMLEN)
  server.aiman.changeteam(ci)
  engine.sendpacket(-1 ,1, putf({ 10, r = 1}, server.N_SETTEAM, ci.clientnum, ci.team, -1):finalize(), -1)
end

local function countscore(fieldname, mapsrecords)
  local topscore, players = 0, {}
  for ci in iterators.players() do
    local score = ci.extra.zombiescores and ci.extra.zombiescores[fieldname] or 0
    if score > 0 then
      if score > topscore then topscore, players = score, {} end
      if score == topscore then table.insert(players, ci.name) end
    end
  end
  if not mapsrecords then return topscore, players end
  local record, new = mapsrecords[server.smapname] or {}
  record[fieldname] = record[fieldname] or {score = 0}
  if topscore > record[fieldname].score then mapsrecords[server.smapname], record[fieldname], new = record, { score = topscore, who = players }, true end
  return topscore, players, record[fieldname].score, record[fieldname].who, new
end

local cachedrecord
local function guydown(ci, chicken, persist)
  if server.interm ~= 0 then return end
  local hasgoods
  for ci in iterators.players() do
    if ci.team == "good" and ci.state.state == engine.CS_ALIVE then hasgoods = true break end
  end
  if hasgoods then
    return not chicken and engine.sendpacket(-1, 1, putf({30, r = 1}, server.N_SERVMSG,server.colorname(ci, nil) .. " is now \f3zombie\f7!"):finalize(), ci.clientnum)
  else
    server.startintermission()
    server.sendservmsg("\f3" .. server.colorname(ci, nil) .. (chicken and " chickened out" or " died") .. "\f7, all hope is lost!")

    local record = persist and jsonpersist.load(servertag.fntag .. "zombierecords") or {}
    local slices, slicers, oldslices, oldslicers, recordslices = countscore("slices", record)
    if recordslices or slices ~= 0 then
      server.sendservmsg((persist and recordslices and "\f6NEW MAP RECORD! " or "") .. "\f3Top zombie slicer: \f7" .. table.concat(slicers, ", ") .. " (" .. slices .. " zombie slices)")
    end
    if oldslices ~= 0 and not recordslices then
      server.sendservmsg("\f2Slices record holder\f7: " .. table.concat(oldslicers, ", ") .. " (" .. oldslices .. ")")
    end
    local kills, killers, oldkills, oldkillers, recordkills = countscore("kills", record)
    if recordkills or kills ~= 0 then
      server.sendservmsg((persist and recordkills and "\f6NEW MAP RECORD! " or "") .. "\f3Rambo: \f7" .. table.concat(killers, ", ") .. " (" .. kills .. " zombies slain)")
    end
    if oldkills ~= 0 and not recordkills then
      server.sendservmsg("\f2Rambo record holder\f7: " .. table.concat(oldkillers, ", ") .. " (" .. oldkills .. ")")
    end

    if persist and (recordslices or recordkills) then
      jsonpersist.save(record, servertag.fntag .. "zombierecords")
      cachedrecord = record
    end
  end
end

function module.on(config, persist)
  map.np(L"spaghetti.removehook(_2)", hooks)
  commands.remove"zombierecord"
  server.MAXBOTS, hooks, healthdrops, spawnedhealths, fires, killbasesp, cachedrecord = 32, {}, {}, {}, {}
  if not config then return end

  if persist then
    cachedrecord = jsonpersist.load(servertag.fntag .. "zombierecords")
    commands.add("zombierecord", function(info)
      local delete, map = info.args:match"(%-?)([^ ]*)"
      delete, map = delete == "-", map ~= "" and map or server.smapname
      if delete then
        if info.ci.privilege < server.PRIV_ADMIN then playermsg("You lack privileges to delete a record.", info.ci) return end
        cachedrecord = jsonpersist.load(servertag.fntag .. "zombierecords")
      end
      if not cachedrecord then playermsg("No records loaded.", info.ci) return end
      local records = cachedrecord[map]
      if not records then playermsg("No records found for map " .. map, info.ci) return end
      if delete then
        cachedrecord[map] = nil
        jsonpersist.save(cachedrecord, servertag.fntag .. "zombierecords")
        playermsg("Records on map " .. map .. " deleted.", info.ci)
        return
      end
      local msg = "Records on " .. map .. ":"
      for field, header in pairs({ kills = "Rambo", slices = "Zombie slicer" }) do if records[field] then
        msg = msg .. "\n\t" .. header .. ": " .. table.concat(records[field].who, ", ") .. " (" .. records[field].score .. ")"
      end end
      playermsg(msg, info.ci)
    end, "#zombierecord [map]: show record for the current map, or [map]. Prepend - to delete the record.")
  end

  hooks.autoteam = spaghetti.addhook("autoteam", function(info)
    if not server.m_regencapture or info.skip then return end
    info.skip = true
    if info.ci then info.ci.team = gracetime and "good" or "evil" return end
    for ci in iterators.clients() do
      ci.extra.zombiescores = nil
      changeteam(ci, "good")
    end
  end)
  hooks.servmodesetup = spaghetti.addhook("servmodesetup", function(info)
    server.MAXBOTS, healthdrops, spawnedhealths, fires, killbasesp = 32, {}, {}, {}
    if not server.m_regencapture or not ents.active() then return end
    info.skip = true
    server.MAXBOTS = 128
    server.aiman.setbotbalance(nil, false)
    gracetime = true
    if server.m_noitems then map.nf(ents.delent, ents.enum(server.I_HEALTH)) end
    spaghetti.latergame(3000, function() server.sendservmsg(config.banner and config.banner or "\f3ZOMBIE OUTBREAK IN 10 SECONDS\f7! Take cover!") end)
    spaghetti.latergame(10000, L"server.sendservmsg('\f3Zombies in \f23...')")
    spaghetti.latergame(11000, L"server.sendservmsg('\f22...')")
    spaghetti.latergame(12000, L"server.sendservmsg('\f21...')")
    spaghetti.latergame(13000, function()
      server.changegamespeed(config.speed, nil)
      gracetime = nil
      map.nf(L"_.state.state == engine.CS_DEAD and server.sendspawn(_)", iterators.clients())
      for i = 1, config.initialspawn or 1 do server.aiman.addai(0, -1) end
      spaghetti.latergame(config.spawninterval, L"server.aiman.addai(0, -1)", true)
      for ci in iterators.spectators() do changeteam(ci, "evil") end
      spaghetti.latergame(1, function() server.sendservmsg('\f3Kill the zombies!') end)
    end)
    server.capturemode:addscore(-1, "evil", 666)
    local numbases = 0
    for _ in ents.enum(server.BASE) do numbases = numbases + 1 end
    if numbases == 0 then return end
    local p = putf({ 30, r = 1}, server.N_BASES, numbases)
    for _ = 1, numbases do p = putf(p, 0, "", "", 0, 0) end
    killbasesp = p:finalize()
    engine.sendpacket(-1, 1, killbasesp, -1)
  end)
  hooks.clientbases = spaghetti.addhook(server.N_BASES, L"_.skip = true")

  hooks.setteam = spaghetti.addhook(server.N_SETTEAM, blockteams)
  hooks.switchteam = spaghetti.addhook(server.N_SWITCHTEAM, blockteams)
  hooks.delbot = spaghetti.addhook(server.N_DELBOT, function(info)
    if not server.m_regencapture or info.skip or info.ci.privilege >= server.PRIV_ADMIN then return end
    info.skip = true
    playermsg("Only admins can delete zombies", info.ci)
  end)
  hooks.addbot = spaghetti.addhook(server.N_ADDBOT, function(info)
    if not server.m_regencapture or info.skip then return end
    info.skip = true
    if info.ci.privilege < server.PRIV_ADMIN then playermsg("Only admins can add zombies", info.ci) return end
    server.aiman.reqadd(info.ci, info.skill)
  end)
  hooks.botbalance = spaghetti.addhook(server.N_BOTBALANCE, function(info)
    if not server.m_regencapture or info.skip then return end
    info.skip = true
    playermsg("Bot balance cannot be set in zombie mode", info.ci)
  end)

  hooks.notalive = spaghetti.addhook("notalive", function(info)
    if not server.m_regencapture then return end
    local ci = info.ci
    if ci.team == "evil" or gracetime then return end
    changeteam(ci, "evil")
    guydown(ci, ci.state.state ~= engine.CS_DEAD, persist)
  end)
  hooks.damaged = spaghetti.addhook("damaged", function(info)
    if not server.m_regencapture or info.target.state.state ~= engine.CS_DEAD then return end
    local actor = info.actor
    actor.state.frags = (actor.extra.zombiescores or { kills = 0 }).kills
    server.sendresume(actor)
  end)
  hooks.suicide = spaghetti.addhook("suicide", function(info)
    if not server.m_regencapture then return end
    local ci = info.ci
    ci.state.frags = (ci.extra.zombiescores or { kills = 0 }).kills
    server.sendresume(ci)
  end)
  hooks.spawnstate = spaghetti.addhook("spawnstate", function(info)
    if not server.m_regencapture or info.skip then return end
    info.skip = true
    config.ammo(info.ci)
    info.ci.state.lifesequence = (info.ci.state.lifesequence + 1) % 0x80
  end)
  hooks.dodamage = spaghetti.addhook("dodamage", function(info)
    if not server.m_regencapture or info.skip then return end
    if info.target.team == info.actor.team then
      if info.target.team == "evil" then info.skip = true
      else
        info.target = info.actor
        local push = info.hitpush
        push.x, push.y, push.z = 0, 0, 0
        sound(info.actor, server.S_PAIN6)
      end
      return
    end

    if info.target.team ~= "evil" then return end
    local scores = info.actor.extra.zombiescores or { slices = 0, kills = 0 }
    if info.gun == server.GUN_FIST then
      scores.slices = scores.slices + 1
      info.damage = info.target.state.health
      if config.healthdrop and math.random() < (config.healthprobability or 1) then
        local o, infire = info.target.state.o
        for _, fireo in pairs(fires) do if fireo:dist(o) < 12 then infire = true break end end
        local dropent = not infire and ents.newent(server.I_HEALTH, o)
        if dropent then
          healthdrops[dropent] = true
          ents.setspawn(dropent, true)
        end
      end
    end
    if info.target.state.health - info.damage <= 0 then scores.kills = scores.kills + 1 end
    info.actor.extra.zombiescores = scores
  end)
  hooks.pickup = spaghetti.addhook("prepickup", function(info)
    local i = info.i
    local _, sent = ents.getent(i)
    if server.gamemillis >= server.gamelimit or not sent or not sent.spawned or sent.type ~= server.I_HEALTH then return end
    info.skip = true
    if info.ci.team == "evil" or not info.ci.state:canpickup(server.I_HEALTH) then return end
    (spawnedhealths or {})[i] = nil
    if not healthdrops[i] then return end
    healthdrops[i] = nil
    ents.delent(i)
    info.ci.state.health = math.min(info.ci.state.health + config.healthdrop, info.ci.state.maxhealth)
    engine.sendpacket(-1, 1, putf({5, r=1}, server.N_ITEMACC, i, info.ci.clientnum):finalize(), -1);
    server.sendresume(info.ci)
  end)
  hooks.itemspawn = spaghetti.addhook("itemspawn", function(info)
    if not config.burnhealth then return end
    local _, _, ment = ents.getent(info.i)
    if ment.type ~= server.I_HEALTH then return end
    spawnedhealths[info.i] = ment
  end)
  hooks.npos = spaghetti.addhook("positionupdate", function(info)
    if not config.burnhealth or info.cp.team ~= "evil" then return end
    for i, ment in pairs(spawnedhealths) do
      local cio = vec3(info.lastpos.pos)
      if cio:dist(ment.o) < 12 then
        for i = 1, 3 do
          local flamei = ents.newent(server.PARTICLES, ment.o, 11, 400, 60, 0x600)
          if flamei then
            fires[flamei] = i == 1 and vec3(ment.o) or nil
            spaghetti.latergame(1000, function() ents.delent(flamei) fires[flamei] = nil end)
          end
        end
        ents.delent(i)
        spawnedhealths[i], healthdrops[i] = nil
      end
    end
  end)
  hooks.canspawn = spaghetti.addhook("canspawnitem", function(info)
    info.can = (server.m_regencapture and info.type == server.I_HEALTH) and true or info.can
  end)


  hooks.damageeffects = spaghetti.addhook("damageeffects", function(info)
    local ci = info.target
    local lastpos = ci.extra.lastpos
    if not server.m_regencapture or info.skip or ci.team == "evil" or info.actor.team ~= "evil" or ci.state.health > 0 or not lastpos then return end
    changeteam(ci, "evil")
    info.actor.state.damage = info.actor.state.damage + info.damage
    server.spawnstate(ci)
    local p, st = { 20, r = 1}, ci.state
    for ammotype = server.GUN_SG, server.GUN_PISTOL do p = putf(p, server.N_BASEREGEN, ci.clientnum, st.health, st.armour, ammotype, st.ammo[ammotype]) end
    p = putf(p, server.N_GUNSELECT, st.gunselect)
    engine.sendpacket(-1, 1, n_client(p, ci):finalize(), -1)
    server.sendresume(ci)
    if ci.state.aitype == server.AI_NONE then playermsg(server.colorname(info.actor, nil) .. " \f3ate your brain\f7!", ci) end
    guydown(ci, false, persist)
  end)

  hooks.connected = spaghetti.addhook("connected", function(info)
    if not server.m_regencapture then return end
    if info.ci.state.state ~= engine.CS_SPECTATOR then server.sendspawn(info.ci) end
    return killbasesp and engine.sendpacket(info.ci.clientnum, 1, killbasesp, -1)
  end)
  hooks.botjoin = spaghetti.addhook("reinitai", function(info)
    if not server.m_regencapture or info.skip then return end
    info.ci.team, info.ci.name = "evil", "zombie"
    server.aiman.changeteam(info.ci)
  end)
  hooks.disconnect = spaghetti.addhook("clientdisconnect", function(info)
    if not server.m_regencapture or gracetime then return end
    changeteam(info.ci, "evil")
    guydown(info.ci, true, persist)
  end)
  hooks.botleave = spaghetti.addhook("botleave", function(info)
    if not server.m_regencapture or gracetime or info.ci.aireinit ~= 0 or info.ci.team == "evil" then return end
    guydown(info.ci, true, persist)
  end)
  hooks.noclients = spaghetti.addhook("noclients", function(info)
    return server.m_regencapture and not spaghetti.quit and server.checkvotes(true)
  end)

end

return module
