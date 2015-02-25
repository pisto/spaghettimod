--[[

  Use std.ban to handle everything about bans and kicks, and persist the kicks.

]]--

local ban, servertag = require"std.ban", require"utils.servertag"
pcall(function() ban.kickpersist(servertag.fntag .. "bans") end)
