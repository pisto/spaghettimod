--[[

  Ban IPs belonging to AS numbers.

]]--

local fs = require"utils.fs"
if not fs.readsome then engine.writelog("Cannot ban AS numbers, missing lua-posix") return end
local ban = require"std.ban"

local function parselist(bans)
  ban.clear("ASkidban")
  for s in bans:gmatch("%d+") do
    local encoded = tonumber(s)
    if not encoded then engine.writelog("ASkidban: cannot parse " .. s)
    else ban.ban("ASkidban", {ip = math.modf(encoded / 0x40), mask = encoded % 0x40}, nil, nil, false, nil, nil, true) end
  end
  engine.writelog("ASkidban: updated")
end

local reloadlist
reloadlist = function()
  local wget, err = io.popen"wget -qO- https://raw.githubusercontent.com/pisto/ASkidban/master/compiled/ipv4_compact"
  if not wget then
    engine.writelog("Cannot fetch ASkidban list: " .. err)
    spaghetti.later(60 * 60 * 1000, reloadlist)
    return
  end
  local bans, fetcher = ""
  fetcher = spaghetti.later(100, function()
    local str = fs.readsome(wget)
    if not str then
      local ret = wget:close()
      spaghetti.cancel(fetcher)
      spaghetti.later(60 * 60 * 1000, reloadlist)
      if ret == true or ret == 0 then parselist(bans)
      else engine.writelog("Cannot fetch ASkidban list (wget failed)") end
      return
    end
    bans = bans .. str
  end, true)
end

local playermsg = require"std.playermsg"
ban.newlist("ASkidban", "Your IP appears to be a proxy, see \fs\f0github.com/pisto/ASkidban\fr for more info.\nIf this is an error please contact \fs\f0pisto\fr: \fs\f0blaffablaffa@gmail.com\fr", { client = server.PRIV_ADMIN }, nil, true)
spaghetti.addhook("commands.banenum", function(info)
  if info.args:match"^[^ ]*" ~= "ASkidban" then return end
  info.skip = true
  playermsg("The ASkidban list is too big to be enumerated in-game.\nView it on https://github.com/pisto/ASkidban", info.ci)
end, true)

reloadlist()
