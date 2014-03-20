--[[

  Generate an unique identifier for each connection.

]]--

local uuid, fp, lambda = require"uuid", require"utils.fp", require"utils.lambda"
local fold, last, Lr = fp.fold, fp.last, lambda.Lr

local urandom = io.open("/dev/urandom")
uuid.randomseed(last(fold.vz(Lr"_1 * 256 + _2", 0, urandom:read(7):byte())))
urandom:close()

local function adduuid(info) info.ci.extra.uuid = uuid() end

spaghetti.addhook("clientconnect", adduuid)
spaghetti.addhook("botjoin", adduuid)

for ci in require("std.iterators").all() do
  if not ci.extra.uuid then ci.extra.uuid = uuid() end
end
