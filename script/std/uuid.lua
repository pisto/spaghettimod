--[[

  Generate an unique identifier for each connection.

]]--

local uuid, fp, lambda = require"uuid", require"utils.fp", require"utils.lambda"
local fold, first, last, pick, Lr = fp.fold, fp.first, fp.last, fp.pick, lambda.Lr

local urandom = io.open("/dev/urandom")
uuid.randomseed(last(fold.vz(Lr"_1 * 256 + _2", 0, urandom:read(7):byte())))
urandom:close()

local uuids = {}

local function adduuid(info)
  info.ci.extra.uuid = uuid()
  uuids[info.ci.extra.uuid] = info.ci
end

spaghetti.addhook("clientconnect", adduuid)
spaghetti.addhook("botjoin", adduuid)

for ci in require("std.iterators").all() do
  if not ci.extra.uuid then ci.extra.uuid = uuid() end
end

local iterators = require"std.iterators"
local function find(uuid)
  local function finder(...)
    return first(pick.zf(function(ci)
      return ci.extra.uuid == uuid
    end, ...))
  end
  return finder(iterators.all()) or finder(iterators.connects())
end

return { find = find }
