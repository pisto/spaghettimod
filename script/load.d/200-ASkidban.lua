--[[

  Ban IPs belonging to AS numbers.

]]--

local fp = require"utils.fp"
local map = fp.map

local fs = require"utils.fs"
if not fs.readsome then engine.writelog("Cannot ban AS numbers, missing lua-posix") return end
local ban = require"std.ban"

local function parselist(bans)
  local kidbans = map.sf(function(s)
    local encoded = tonumber(s)
    if not encoded then engine.writelog("ASkidban: cannot parse " .. s) return end
    return {ip = math.modf(encoded / 0x40), mask = encoded % 0x40}
  end, bans:gmatch("%d+"))
  ban.clear("ASkidban")
  ban.fill("ASkidban", kidbans)
end

local reloadlist
reloadlist = function()
  local wget, err = io.popen"wget -qO- https://codeload.github.com/pisto/ASkidban/tar.gz/master | tar -xzO ASkidban-master/compiled/ipv4_compact"
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
ban.newlist(
  "ASkidban",
  "Your IP appears to be a proxy, see \fs\f0github.com/pisto/ASkidban\fr. For complaints and whitelisting requests: \fs\f0blaffablaffa@gmail.com\fr",
  { client = server.PRIV_ADMIN, bypass = { server.PRIV_AUTH, ["ASkidban-bypass"] = true } },
  nil,
  true
)
spaghetti.addhook("commands.banenum", function(info)
  if info.args:match"^[^ ]*" ~= "ASkidban" then return end
  info.skip = true
  playermsg("The ASkidban list is too big to be enumerated in-game.\nView it on https://github.com/pisto/ASkidban", info.ci)
end, true)

spaghetti.later(5000, reloadlist)
table.insert(require"std.auth".preauths, "ASkidban-bypass")

cs.adduser("Cedii**", "ASkidban-bypass", "-4e75e0e92e6512415a8114e1db856af36d00e801615a3e98", "n")
cs.adduser("/dev/zero", "ASkidban-bypass", "-fbd614eb7d65a9422f40f329d093dc7ecf30c7d1e0130618", "n")
cs.adduser("lopi2", "ASkidban-bypass", "-e4ce067444a51914ce289b656154f172b367a64bb04f854a", "n")
cs.adduser("M0UL", "ASkidban-bypass", "+640728e15ab552342b68a293f2c6b3e15b5adf1be53fd4f2", "n")
cs.adduser("PowerKiller", "ASkidban-bypass", "-3f7e1ba3b1b2163ce7309613cb8b3e22ad5e9de3df677185", "n")
cs.adduser("Shag", "ASkidban-bypass", "+0b7e1cdadb84c545b4bb181c3f3744dd62fcc2d60eba3ecd", "n")
cs.adduser("Star", "ASkidban-bypass", "-c5f79ab9249cc896eb735464e91aa1ec8c4c08ddeed156ee", "n")
cs.adduser("xcb567", "ASkidban-bypass", "+41b02bfb90f87d403a864e722d2131a5c7941f2b35491d0f", "n")
