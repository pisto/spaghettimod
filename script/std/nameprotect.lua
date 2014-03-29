--[[

  Name/team protect, regex and auth/domain based.

]]--

local module = {}

local fp, lambda, iterators, playermsg, n_client = require"utils.fp", require"utils.lambda", require"std.iterators", require"std.playermsg", require"std.n_client", require"std.allclaims"
local map, pick, first, L, Lr = fp.map, fp.pick, fp.first, lambda.L, lambda.Lr

local filtername = Lr"engine.filtertext(_):sub(1, server.MAXNAMELEN)"

local function checkchange(ci, name, db)
  local ciauths = ci.extra.allclaims or {}
  return not first(pick.zp(function(regex, auths)
    if not name:match(regex) then return end
    return not first(pick.zp(function(domain, users)
      if not ciauths[domain] then return end
      if users == true then return true end
      return first(pick.zp(function(user) return users[user] end, ciauths[domain]))
    end, auths))
  end, db))
end

local function warn(ci)
  playermsg("\"" .. ci.extra.nameprotect.name .. "\" is protected, so now you are seen as \"" .. ci.name .. "\". Please login with auth.", ci)
end

local function makeprotect(ci, name, db, authnamefunc)
  ci.extra.nameprotect = setmetatable({ name = name }, { __call = function()
    if not db or checkchange(ci, name, db, authnamefunc) then
      local rename = engine.packetbuf(2 + #name, engine.ENET_PACKET_FLAG_RELIABLE):putint(server.N_SWITCHNAME):sendstring(name)
      rename = n_client(rename, ci)
      engine.sendpacket(-1, 1, rename:finalize(), ci.clientnum)
      playermsg("You are now seen as " .. name .. ".", ci)
      ci.extra.nameprotect = nil
    end
  end})
end

local db, connecthook, switchhook, maploadedhook, masterhook
function module.on(on)
  if not on and db then
    map.nv(spaghetti.removehook, connecthook, switchhook, maploadedhook)
    db, connecthook, switchhook, maploadedhook = nil
    map.fn(Lr"_.extra.nameprotect and _.extra.nameprotect()", iterators.clients())
    return
  end

  if on and not db then
    require"std.maploaded"
    db = {}
    connecthook = spaghetti.addhook(server.N_CONNECT, function(info)
      local name = filtername(info.text)
      if not checkchange(info.ci, name, db) then
        info.text = "unauthed"
        makeprotect(info.ci, name, db)
      end
    end)
    switchhook = spaghetti.addhook(server.N_SWITCHNAME, function(info)
      local name = filtername(info.text)
      if info.skip or name == info.ci.name then return end
      if checkchange(info.ci, name, db) then info.ci.extra.nameprotect = nil return end
      info.skip = true
      makeprotect(info.ci, name, db)
      warn(info.ci)
    end)
    maploadedhook = spaghetti.addhook("maploaded", function(info)
      if info.ci.extra.nameprotect and not info.ci.extra.nameprotect.warned then warn(info.ci) info.ci.extra.nameprotect.warned = true end
    end)
    masterhook = spaghetti.addhook("master", Lr"_.authname and _.ci.extra.nameprotect and _.ci.extra.nameprotect()")
  end
  return on and db or nil
end

return module
