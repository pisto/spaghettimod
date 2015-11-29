--[[

  Ban vpngate.net proxies.

]]--

local fp, ip = require"utils.fp", require"utils.ip"
local map = fp.map

local fs = require"utils.fs"
if not fs.readsome then engine.writelog("Cannot ban vpngate.net, missing lua-posix") return end
local ban = require"std.ban"

local function parselist(bans)
  local vpngate = map.sf(ip.ip, bans:gmatch("(.-)\n"))
  ban.clear("vpngate.net")
  ban.fill("vpngate.net", vpngate)
end

local reloadlist
reloadlist = function()
  local wget, err = io.popen"wget -qO- http://www.vpngate.net/api/iphone/ | tail -n +3 | cut -d , -f 2"
  if not wget then
    engine.writelog("Cannot fetch vpngate.net list: " .. err)
    spaghetti.later(10 * 60 * 1000, reloadlist)
    return
  end
  local bans, fetcher = ""
  fetcher = spaghetti.later(100, function()
    local str = fs.readsome(wget)
    if not str then
      local ret = wget:close()
      spaghetti.cancel(fetcher)
      spaghetti.later(10 * 60 * 1000, reloadlist)
      if ret == true or ret == 0 then parselist(bans)
      else engine.writelog("Cannot fetch vpngate.net list (wget failed)") end
      return
    end
    bans = bans .. str
  end, true)
end

local playermsg = require"std.playermsg"
ban.newlist(
  "vpngate.net",
  "Your IP appears to be a vpngate.net proxy. For complaints and whitelisting requests: \fs\f0blaffablaffa@gmail.com\fr",
  { client = server.PRIV_ADMIN, bypass = { server.PRIV_AUTH, ["ASkidban-bypass"] = true } },
  nil,
  true
)
spaghetti.addhook("commands.banenum", function(info)
  if info.args:match"^[^ ]*" ~= "vpngate.net" then return end
  info.skip = true
  playermsg("The vpngate.net list can be browsed here: http://www.vpngate.net", info.ci)
end, true)

spaghetti.later(5000, reloadlist)
table.insert(require"std.auth".preauths, "ASkidban-bypass")
