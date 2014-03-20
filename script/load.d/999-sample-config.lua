--[[

  This is the configuration I use on my server.

]]--

if not os.getenv("PISTOVPS") then return end
engine.writelog("Applying the sample configuration.")

local fp, lambda = require"utils.fp", require"utils.lambda"
local map, range, fold, last, I, L, Lr = fp.map, fp.range, fp.fold, fp.last, fp.I, lambda.L, lambda.Lr
local abuse, playermsg, n_client = require"std.abuse", require"std.playermsg", require"std.n_client"

--make sure you delete the next two lines, or I'll have admin on your server.
cs.serverauth = "pisto"
cs.adduser("pisto", "pisto", "+8ce1687301aea5c4500df0042849191f875c70555c3cc4c9", "a")

spaghetti.addhook("log", function(info)
  info.s = os.date("%c | ") .. info.s
end)

cs.serverdesc = "\f7github:\f2spaghettimod"
local trap = require"std.light-trap"
trap.set(0.2, "github.com/pisto/spaghettimod   ")

cs.lockmaprotation = 2
cs.maprotationreset()
--copied from data/menus.cfg
local ffamaps, capturemaps = table.concat({
  "aard3c academy akaritori alithia alloy aqueducts arbana bvdm_01 castle_trap collusion complex corruption curvedm curvy_castle darkdeath deathtek depot",
  "dirtndust DM_BS1 dock douze duel7 duel8 dune elegy fanatic_quake force fragplaza frostbyte frozen fury guacamole gubo hades",
"hashi hog2 industry injustice island justice kalking1 katrez_d kffa killfactory kmap5 konkuri-to ksauer1 legazzo lostinspace masdm mbt10",
  "mbt2 mbt9 memento metl2 metl3 metl4 moonlite neondevastation neonpanic nmp8 nucleus oasis oddworld ogrosupply orbe orion osiris",
  "ot outpost paradigm park pgdm phosgene pitch_black powerplant refuge renegade rm5 roughinery ruby ruine sauerstruck sdm1 shadowed",
  "shindou shinmei1 shiva simplicity skrdm1 stemple suburb tartech teahupoo tejen thetowers thor torment tumwalk turbine wake5 wdcd"
}, " "), table.concat({
  "abbey akroseum alithia arabic asgard asteroids c_egypt c_valley campo capture_night caribbean collusion core_refuge core_transfer corruption cwcastle damnation",
  "dirtndust donya duomo dust2 eternal_valley evilness face-capture fb_capture fc3 fc4 fc5 forge frostbyte hades hallo haste hidden",
  "infamy killcore3 kopenhagen lostinspace mbt12 mercury monastery nevil_c nitro nmp4 nmp8 nmp9 nucleus ogrosupply paradigm ph-capture reissen",
  "relic river_c serenity snapper_rocks spcr subterra suburb tempest tortuga turbulence twinforts urban_c valhalla venice xenon"
}, " ")

math.randomseed(last(fold.f(Lr"_1 + _2", 0, map.zp(Lr"type(_2) == 'number' and _2 or 0", os.date("*t")))))
ffamaps, capturemaps = map.uv(function(maps)
  local t = map.f(I, maps:gmatch("[^ ]+"))
  map.nf(function(i)
    local j = math.random(i)
    local s = t[j]
    t[j] = t[i]
    t[i] = s
  end, range.z(2, #t))
  return table.concat(t, " ")
end, ffamaps, capturemaps)

cs.maprotation("?ffa ?effic ?tac", ffamaps, "?regencapture ?capture", capturemaps)
cs.publicserver = 2

spaghetti.addhook(server.N_KICK, function(info)
  if info.skip or info.ci.privilege > server.PRIV_MASTER then return end
  info.skip = true
  playermsg("No. Use gauth.", info.ci)
end)
spaghetti.addhook(server.N_SOUND, function(info)
  if info.skip or abuse.clientsound(info.sound) then return end
  info.skip = true
  playermsg("I know I used to do that but... whatever.", info.ci)
end)
abuse.ratelimit({ server.N_TEXT, server.N_SAYTEAM }, 0.5, 10, Lr"nil, 'I don\\'t like spam.'")
abuse.ratelimit(server.N_SWITCHNAME, 1/30, 4, Lr"nil, 'You\\'re a pain.'")
abuse.ratelimit(server.N_MAPVOTE, 1/10, 3, Lr"nil, 'That map sucks anyway.'")
abuse.ratelimit(server.N_SPECTATOR, 1/30, 5, Lr"_.ci.clientnum ~= _.spectator, 'Can\\'t even describe you.'") --self spec
abuse.ratelimit(server.N_MASTERMODE, 1/30, 5, Lr"_.ci.privilege == server.PRIV_NONE, 'Can\\'t even describe you.'")
abuse.ratelimit({ server.N_AUTHTRY, server.N_AUTHKICK }, 1/60, 4, Lr"nil, 'Are you really trying to bruteforce a 192 bits number? Kudos to you!'")
abuse.ratelimit(server.N_CLIENTPING, 4.5) --no message as it could be cause of network jitter

--ratelimit just gobbles the packet. Use the selector to add a tag to the exceeding message, and append another hook to send the message
local function warnspam(packet)
  if not packet.ratelimited or type(packet.ratelimited) ~= "string" then return end
  playermsg(packet.ratelimited, packet.ci)
end
map.nv(function(type) spaghetti.addhook(type, warnspam) end,
  server.N_TEXT, server.N_SAYTEAM, server.N_SWITCHNAME, server.N_MAPVOTE, server.N_SPECTATOR, server.N_MASTERMODE, server.N_AUTHTRY, server.N_AUTHKICK, server.N_CLIENTPING
)

--force the client back to his original name if his N_SWITCHNAME packet has been ratelimited
spaghetti.addhook(server.N_SWITCHNAME, function(info)
  if not info.ratelimited then return end
  local newname = engine.filtertext(info.text):sub(1, server.MAXNAMELEN):gsub("^$", "unnamed")
  if newname == info.ci.name then return end
  local rename = engine.packetbuf(1 + #info.ci.name, engine.ENET_PACKET_FLAG_RELIABLE):putint(server.N_SWITCHNAME):sendstring(info.ci.name)
  rename = n_client(rename, info.ci)
  engine.sendpacket(info.ci.clientnum, 1, rename:finalize(), -1)
end)

--people are impatient
spaghetti.addhook(server.N_TEXT, function(info)
  if info.skip or (info.text ~= "#help" and info.text ~= "#version" and info.text ~= "#info") then return end
  playermsg("spaghettimod is a reboot of hopmod for programmers. Will be used for SDoS.\nKindly brought to you by pisto.", info.ci)
end)

--track players better in the log: more informative connect/disconnect messages, track name changes
spaghetti.addhook("log", function(info)
  if info.s:match"^client connected" or info.s:match"^client [^ ]+ disconnected" or info.s:match"^disconnected client" then info.skip = true return end   --will make a better message in the clientdisconnect hook
end, true)

local ip = require"utils.ip"
local function conninfo(client)
  local name, cn= "", client
  if type(client) ~= 'number' then name, cn = client.name, client.clientnum end
  local peer = engine.getclientpeer(cn)
  return string.format('%s(%d) %s:%d:%d', name, cn, tostring(ip.ip(peer.address.host)), peer.address.port, peer.incomingPeerID)
end
spaghetti.addhook("clientconnect", function(info)
  engine.writelog("connect: " .. conninfo(info.ci))
end)
spaghetti.addhook("connected", function(info)
  engine.writelog("join: " .. conninfo(info.ci))
end)
spaghetti.addhook(server.N_SWITCHNAME, function(info)
  if info.skip then return end
  engine.writelog(string.format('rename: %s -> %s(%d)', conninfo(info.ci), engine.filtertext(info.text):sub(1, server.MAXNAMELEN):gsub("^$", "unnamed"), info.ci.clientnum))
end)
spaghetti.addhook("clientdisconnect", function(info)
  engine.writelog(string.format("disconnecting: %s %s", conninfo(info.ci), engine.disconnectreason(info.reason) or "none"))
end)
spaghetti.addhook("enetevent", function(info)
  if info.skip or info.event.type ~= engine.ENET_EVENT_TYPE_DISCONNECT then return end
  if info.ci then engine.writelog("disconnected: " .. conninfo(info.ci))
  else
    local peer = info.event.peer
    engine.writelog(string.format('disconnected: %s:%d:%d', tostring(ip.ip(peer.address.host)), peer.address.port, peer.incomingPeerID))
  end
end)
