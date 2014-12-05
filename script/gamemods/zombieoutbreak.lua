--[[

  Zombie outbreak mode: you get chaingun to stop a horde of bots with rockets and grenades, in slow motion.
  The "ammo" parameter must be a callback which takes a clientinfo and fills its state.
  Mode needs to be efficteam or tacteam.

]]--

local fp, lambda, iterators, playermsg, putf, servertag, jsonpersist, n_client, ents, spawnat = require"utils.fp", require"utils.lambda", require"std.iterators", require"std.playermsg", require"std.putf", require"utils.servertag", require"utils.jsonpersist", require"std.n_client", require"std.ents", require"std.spawnat"
local map, range, pick, breakk, L, Lr = fp.map, fp.range, fp.pick, fp.breakk, lambda.L, lambda.Lr

require"std.saveteam".on(true)
require"std.lastpos"

local module = {}
local hooks, healthdrops, gracetime = {}, {}

local function blockteams(info)
  if not server.m_teammode or info.skip or info.ci.privilege >= server.PRIV_ADMIN then return end
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

function module.on(config, persist)
  map.np(L"spaghetti.removehook(_2)", hooks)
  server.MAXBOTS, hooks, healthdrops = 32, {}, {}
  if not config then return end

  hooks.autoteam = spaghetti.addhook("autoteam", function(info)
    if not server.m_teammode or info.skip then return end

    info.skip = true
    server.addteaminfo("good") server.addteaminfo("evil")
    map.nf(function(ci)
      ci.extra.zombiescores, ci.extra.teamrestored = nil
      changeteam(ci, "good")
    end, iterators.clients())
  end)
  hooks.changemap = spaghetti.addhook("changemap", function()
    server.MAXBOTS, healthdrops = 32, {}
    if not server.m_teammode then return end
    server.MAXBOTS = 128
    server.aiman.setbotbalance(nil, false)
    gracetime = true
    spaghetti.latergame(3000, function() server.sendservmsg(config.banner and config.banner or "\f3ZOMBIE OUTBREAK IN 10 SECONDS\f7! Take cover!") end)
    spaghetti.latergame(10000, L"server.sendservmsg('\f3Zombies in \f23...')")
    spaghetti.latergame(11000, L"server.sendservmsg('\f22...')")
    spaghetti.latergame(12000, L"server.sendservmsg('\f21...')")
    spaghetti.latergame(13000, function()
      server.changegamespeed(config.speed, nil)
      server.sendservmsg('\f3Kill the zombies!')
      gracetime = nil
      server.aiman.addai(0, -1)
      spaghetti.latergame(config.spawninterval, L"server.aiman.addai(0, -1)", true)
    end)
  end)

  hooks.setteam = spaghetti.addhook(server.N_SETTEAM, blockteams)
  hooks.switchteam = spaghetti.addhook(server.N_SWITCHTEAM, blockteams)
  hooks.delbot = spaghetti.addhook(server.N_DELBOT, function(info)
    if not server.m_teammode or info.skip or info.ci.privilege >= server.PRIV_ADMIN then return end
    info.skip = true
    playermsg("Only admins can delete zombies", info.ci)
  end)
  hooks.addbot = spaghetti.addhook(server.N_ADDBOT, function(info)
    if not server.m_teammode or info.skip then return end
    info.skip = true
    if info.ci.privilege < server.PRIV_ADMIN then playermsg("Only admins can add zombies", info.ci) end
    server.aiman.reqadd(info.ci, info.skill)
  end)
  hooks.botbalance = spaghetti.addhook(server.N_BOTBALANCE, function(info)
    if not server.m_teammode or info.skip then return end
    info.skip = true
    playermsg("Bot balance cannot be set in zombie mode", info.ci)
  end)

  hooks.damaged = spaghetti.addhook("notalive", function(info)
    if not server.m_teammode then return end
    map.nf(L"_.state.frags = (_.extra.zombiescores or { kills = 0 }).kills server.sendresume(_)", iterators.all())
    local ci = info.ci
    if ci.team == "evil" or gracetime then return end
    changeteam(ci, "evil")
    guydown(ci, ci.state.state ~= engine.CS_DEAD, persist)
  end)
  hooks.spawnstate = spaghetti.addhook("spawnstate", function(info)
    if not server.m_teammode or info.skip then return end
    info.skip = true
    config.ammo(info.ci)
    info.ci.state.lifesequence = (info.ci.state.lifesequence + 1) % 0x80
  end)
  hooks.dodamage = spaghetti.addhook("dodamage", function(info)
    if not server.m_teammode or info.skip then return end
    if info.target.team == info.actor.team then
      if info.target.team == "evil" then info.skip = true
      else
        info.target = info.actor
        local push = info.hitpush
        push.x, push.y, push.z = 0, 0, 0
      end
      return
    end

    if info.target.team ~= "evil" then return end
    local scores = info.actor.extra.zombiescores or { slices = 0, kills = 0 }
    if info.gun == server.GUN_FIST then
      scores.slices = scores.slices + 1
      info.damage = 90
      if ents.active() and math.random() < (config.healthdrop or 1) then
        local dropent = ents.newent(server.I_HEALTH, info.target.state.o)
        if dropent then
          healthdrops[dropent] = true
          ents.setspawn(dropent, true)
        end
      end
    end
    if info.target.state.health - info.damage <= 0 then scores.kills = scores.kills + 1 end
    info.actor.extra.zombiescores = scores
  end)
  hooks.pickup = spaghetti.addhook("pickup", function(info) return healthdrops[info.i] and ents.delent(info.i) end)
  hooks.canspawn = spaghetti.addhook("canspawnitem", function(info)
    info.can = (server.m_teammode and info.type == server.I_HEALTH) and true or info.can
  end)


  hooks.damageeffects = spaghetti.addhook("damageeffects", function(info)
    local ci = info.target
    local lastpos = ci.extra.lastpos
    if not server.m_teammode or not ents.active() or info.skip or ci.team == "evil" or info.actor.team ~= "evil" or ci.state.health > 0 or not lastpos then return end
    changeteam(ci, "evil")
    info.actor.state.damage = info.actor.state.damage + info.damage
    server.spawnstate(ci)
    spawnat(ci, lastpos.pos, lastpos.yaw)

    --resend the last N_POS from this client to make others have him with CS_ALIVE instead of CS_SPAWNING (might not be delivered in order)
    local p = putf({ #lastpos.buf }, { buf = lastpos.buf }):finalize()
    local start, hooks, repeater, cleanup = engine.totalmillis
    cleanup = function() map.np(L"spaghetti.removehook(_2)", hooks) spaghetti.cancel(repeater) end
    local function left(_) return _.ci.clientnum == ci.clientnum and cleanup() end
    hooks = {
      spaghetti.addhook(server.N_POS, function(info)
        if not info.cp or not info.cp.clientnum ~= ci.clientnum then return end
        cleanup()
        spaghetti.later(1, L"engine.flushserver(true)")
      end),
      spaghetti.addhook("notalive", left),
      spaghetti.addhook("clientdisconnect", left),
      spaghetti.addhook("botleave", left)
    }
    local lastrepeat
    repeater = function()
      local now = engine.totalmillis
      if now - start > 30 then cleanup() return end
      if now == lastrepeat then return end
      lastrepeat = now
      engine.sendpacket(-1, 0, p, ci.ownernum)
      engine.enet_host_flush(engine.serverhost)
    end
    engine.enet_host_flush(engine.serverhost)
    repeater()
    repeater = spaghetti.later(1, repeater, true)

    guydown(ci, false, persist)
  end)

  hooks.connected = spaghetti.addhook("connected", function(info)
    if not server.m_teammode then return end
    if not info.ci.extra.saveteam then changeteam(info.ci, "good") end
    if info.ci.state.state ~= engine.CS_SPECTATOR then server.sendspawn(info.ci) end
  end)
  hooks.botjoin = spaghetti.addhook("reinitai", function(info)
    if not server.m_teammode or info.skip then return end
    info.ci.team, info.ci.name = "evil", "zombie"
    server.aiman.changeteam(info.ci)
  end)
  hooks.disconnect = spaghetti.addhook("clientdisconnect", function(info)
    if not server.m_teammode or gracetime then return end
    changeteam(info.ci, "evil")
    guydown(info.ci, true, persist)
  end)
  hooks.botleave = spaghetti.addhook("botleave", function(info)
    if not server.m_teammode or gracetime then return end
    info.ci.team = "evil"
    guydown(info.ci, true, persist)
  end)
  hooks.noclients = spaghetti.addhook("noclients", function(info)
    return server.m_teammode and not spaghetti.quit and server.checkvotes(true)
  end)

end

return module
