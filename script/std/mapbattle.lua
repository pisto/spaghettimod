--[[

  Mapbattle script - A heavenly approach to sanity

  TODO: 	* maprotation checks 
  				- check if ffamaps, capturemaps, ctfmaps are present
  				- otherwise custom maprotation
  				- locked maprotation
]]--

--[[
	maplist copied from data/menus.cfg
	`- should be the same as in your 1000-servername-config.lua
	`- if you'd like to use a custom maplist, use a 1 whitespace seperated string and adjust the following functions with your stringname: str_to_l, prius, get_curmaprotation
]]
local ffamaps, capturemaps, ctfmaps = table.concat({
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
}, " "), table.concat({
  "abbey akroseum arbana asgard authentic autumn bad_moon berlin_wall bt_falls campo capture_night catch22 core_refuge core_transfer damnation desecration dust2",
  "eternal_valley europium evilness face-capture flagstone forge forgotten garden hallo haste hidden infamy kopenhagen l_ctf mach2 mbt1 mbt12",
  "mbt4 mercury mill nitro nucleus recovery redemption reissen sacrifice shipwreck siberia snapper_rocks spcr subterra suburb tejen tempest",
  "tortuga turbulence twinforts urban_c valhalla wdcd xenon fc4 fc5 gubo donya duomo"
}, " ")

-- load required libs
local abuse, playermsg, iterators = require"std.abuse", require"std.playermsg", require"std.iterators"
local L, fp = require"utils.lambda", require"utils.fp"

-- string to list
local function str_to_l (bigstring)	return fp.map.lf(L"_", bigstring:gmatch("%S+")) end
ffamaps, capturemaps, ctfmaps = str_to_l(ffamaps), str_to_l(capturemaps), str_to_l(ctfmaps)

-- make hybrid table from the maprotation 
local function prius (list) return fp.map.tsi(list, L"_2", list) end
ffamaps, capturemaps, ctfmaps = prius(ffamaps), prius(capturemaps), prius(ctfmaps)

-- retrieves the current maprotation
local function get_curmaprotation ()
	if cs.lockmaprotation ~= 2 then return end
	return server.m_ctf and ctfmaps or server.m_capture and capturemaps or ffamaps
end

-- validate if map is part of the maprotation
local function validate_map (map) local rot = get_curmaprotation(); return rot[map] end

local function registervote (info)
	if server.mastermode <= server.MM_VETO then return end 
	if info.text ~= server.smapname and server.m_valid(info.reqmode) and validate_map(info.text) then
		info.ci.extra.mapvote = info.text
		playermsg("\f7[\f0MAP\f7BAT\f3TLE\f7] Your vote was \f0accepted\f7 into map battle pool!", info.ci)
	else playermsg("\f7[\f0MAP\f7BAT\f3TLE\f7] \f3Error\f7! your vote was \f3not\f7 accepted into the map battle pool.", info.ci) 
	end
	info.skip = true
end
spaghetti.addhook(server.N_MAPVOTE, registervote)

-- delay the intermission for X amount of milliseconds
local function delayinterm (time) server.interm = server.interm + time end

-- mapbattle struct
local mb = {
	battletime = 15000, -- keep in mind normal interm time is already 10 sec
	suddendeathtime = 5000, 

	-- the following vars need to be cleaned up!
	map1 = nil,
	map2 = nil,
	winner = nil, -- cleaned with changemap hook
	votes = {		-- remove me
		map1 = 0,	-- only in catchvotes and mapbattle_cleanup and count_votes
		map2 = 0	-- or don't remove as we need a number how many votes are currently voted for 
	},				-- remove me
	running = false
}

-- catch votes during mapbattle
local function catchvotes(info) 
	if not mb.running then return end
	if info.ci.state.state == server.CS_SPECTATOR then playermsg("\f7[\f0MAP\f7BAT\f3TLE\f7] Spectators are \f3not\f3 allowed to participate in the vote!", info.ci); return end
	
	if not info.ci.extra.voted then
		if tonumber(info.text) == 1 then 
			mb.votes.map1 = mb.votes.map1 +1
			info.ci.extra.voted = 1
			server.sendservmsg("\f7[\f0MAP\f7BAT\f3TLE\f7] " .. info.ci.name .. " voted for\f0 " .. mb.map1 .. "\f7")
		elseif tonumber(info.text) == 2 then 
			mb.votes.map2 = mb.votes.map2 +1
			info.ci.extra.voted = 2
			server.sendservmsg("\f7[\f0MAP\f7BAT\f3TLE\f7] " .. info.ci.name .. " voted for\f0 " .. mb.map2 .. "\f7")
		end
	else
		if info.ci.extra.voted == 2 and tonumber(info.text) == 1 then 
			mb.votes.map1, mb.votes.map2 = mb.votes.map1 +1, mb.votes.map2 -1
			info.ci.extra.voted = 1
			server.sendservmsg("\f7[\f0MAP\f7BAT\f3TLE\f7] " .. info.ci.name .. " changed his vote to\f0 " .. mb.map1 .. "\f7")
		elseif info.ci.extra.voted == 1 and tonumber(info.text) == 2 then 
			mb.votes.map1, mb.votes.map2 = mb.votes.map1 -1, mb.votes.map2 +1
			info.ci.extra.voted = 2
			server.sendservmsg("\f7[\f0MAP\f7BAT\f3TLE\f7] " .. info.ci.name .. " changed his vote to\f0 " .. mb.map2 .. "\f7")
		elseif tonumber(info.text) == 1 or tonumber(info.text) == 2 then playermsg("\f7[\f0MAP\f7BAT\f3TLE\f7] Sorry, you have \f3only one vote\f7! You may \f0change\f7 it though...", info.ci) end
	end

	if tonumber(info.text) == 1 or tonumber(info.text) == 2 then 
		if mb.votes.map1 > mb.votes.map2 then server.sendservmsg("\f7[\f0MAP\f7BAT\f3TLE\f7] "..mb.map1..":\f0"..mb.votes.map1.."\f7 vs "..mb.map2..":\f3"..mb.votes.map2 .."\f7")
		elseif mb.votes.map1 < mb.votes.map2 then server.sendservmsg("\f7[\f0MAP\f7BAT\f3TLE\f7] "..mb.map1..":\f3"..mb.votes.map1.." \f7vs "..mb.map2..":\f0"..mb.votes.map2 .."\f7")
		else server.sendservmsg("\f7[\f0MAP\f7BAT\f3TLE\f7] "..mb.map1..":\f2"..mb.votes.map1.." \f7vs "..mb.map2..":\f2"..mb.votes.map2 .."\f7")	end
	end
end
spaghetti.addhook(server.N_TEXT, catchvotes)

local function mapbattle_cleanup ()
	-- called on changemap
	mb.map1, mb.map2, mb.winner = nil, nil, nil
	mb.votes.map1, mb.votes.map2 = 0,0 -- oohu, oohu, owl!
	for ci in iterators.all() do ci.extra.mapvote, ci.extra.voted = nil, nil end
end

-- and the winner is
local function count_votes() 
	if mb.votes.map1 > mb.votes.map2 then return mb.map1
	elseif mb.votes.map1 < mb.votes.map2 then return mb.map2
	else return nil end
end

-- returns true if map 1 is winner
-- returns false if map 1 is winner
-- returns nil if neither is winner
local function count_votes2 ()
	local map1_count, map2_count = 0,0 
	for ci in iterators.players() do 
		if ci.extra.voted < 2 then map1_count = 1 + (map1_count or 0)
		else map2_count = 1 + (map2_count or 0)
		end
		ci.extra.voted, ci.extra.mapvote = nil, nil
	end
	return map1_count > map2_count or nil
end

local function mapbattle_finalize ()
	if not mb.winner then 
		local rot = get_curmaprotation()
		mb.winner = rot[math.random(#rot)]
		server.sendservmsg("\n\f7[\f0MAP\f7BAT\f3TLE\f7] \f4SUDDEN DEATH \f3TIE\f7! Selecting a random map: \f0" .. mb.winner .."\f7\n") 
	else server.sendservmsg("\n\f7[\f0MAP\f7BAT\f3TLE\f7] Winner: \f0" .. mb.winner .. "\f7\n") end
	mb.running = false
	delayinterm(1000)
end

local function suddendeath ()
	delayinterm(mb.suddendeathtime)
	server.sendservmsg("\n\f7[\f0MAP\f7BAT\f3TLE\f7] It's a \f3TIE\f7! \f4SUDDEN DEATH\f7! Vote again fast!!! \f0" .. mb.map1 .. "\f7 (1) versus \f0" .. mb.map2 .. "\f7 (2)")
	for ci in iterators.players() do ci.extra.voted = nil end

	spaghetti.latergame(mb.suddendeathtime , function() mb.winner = count_votes(); mapbattle_finalize() end)
end

local function mapbattle_winner ()
	mb.winner = count_votes()
	if not mb.winner then suddendeath()	else mapbattle_finalize() end
end

spaghetti.addhook("prechangemap", function (info) if mb.winner then info.map = mb.winner end end)
spaghetti.addhook("changemap", mapbattle_cleanup)

local function player_cleanup (info)
	info.ci.extra.mapvote = nil
	info.ci.extra.voted = nil
	if info.ci.state.state == engine.CS_SPECTATOR then playermsg("\f7[\f0MAP\f7BAT\f3TLE\f7] Your mapvote has been \f3removed\f7! Only players may participate in voting!", ci) end
end

spaghetti.addhook("clientdisconnect", player_cleanup)
spaghetti.addhook("specstate", player_cleanup)

-- fight!
local function mapbattle (maps)
	mb.running = true
	delayinterm(mb.battletime + 100)
	
	if not next(maps, next(maps)) then -- get a second battlemap from the current maprotation
		mb.map1 = next(maps)
		local rot = get_curmaprotation()
		repeat 
			mb.map2 = rot[math.random(#rot)]
		until mb.map1 ~= mb.map2
	else -- select highest voted map as map1 and slip in a counter
		local c, mcount = 0,0 -- oohu, oohu, owl!
		for k, v in pairs(maps) do mcount = mcount + 1; if v > c then mb.map1, c = k, v end end
		maps[mb.map1] = nil
		-- and select second battle maps randomly from the pool of votes
		local secondary_maps = fp.map.lp(L"_", maps)
		mb.map2 = secondary_maps[math.random(mcount-1)]
	end

	server.sendservmsg("\n\f7[\f0MAP\f7BAT\f3TLE\f7]    \f0" .. mb.map1 .. "\f7 (\f91)  \f7vs.  \f0" .. mb.map2 .. "\f7 (\f92) \n\f7[\f0MAP\f7BAT\f3TLE\f7] Cast your votes now!\n")

	spaghetti.latergame(mb.battletime+10000, mapbattle_winner) -- I told you to remember 10000 ms from the engine! 
end

-- check if there are any mapvotes at intermission and if so initiate mapbattle
local function check_map_votes (info)
	local mapvotes = {}
	for ci in iterators.select(L"_.extra.mapvote") do mapvotes[ci.extra.mapvote] = 1 + (mapvotes[ci.extra.mapvote] or 0) end
    if next(mapvotes) then mapbattle(mapvotes) end
end
spaghetti.addhook("intermission", check_map_votes)
