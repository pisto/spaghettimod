--[[

  Persist a table as json in the var folder.

]]--

local json, fp = require"json", require"utils.fp"
local map = fp.map

local module = {}

local function utf8translate(transcode, checkrecursion)
  local self = {}
  self = function(obj, visited)
    local t = type(obj)
    if t == "string" then return transcode(obj)
    elseif t == "table" then
      local myvisited
      if checkrecursion then
        myvisited = visited or {}
        if myvisited[obj] then error("Recursive table encountered while encoding json") end
        myvisited[obj] = true
      end
      local transcoded = map.mp(function(k, v) return self(k, myvisited), self(v, myvisited) end, obj)
      if checkrecursion then myvisited[obj] = false end
      return transcoded
    else return obj end
  end
  return self
end
local toutf8, fromutf8 = utf8translate(engine.encodeutf8, true), utf8translate(engine.decodeutf8)

function module.save(t, fname, notranscode, options)
  local j = json.encode(notranscode and t or toutf8(t), options)
  local out, err, ok = io.open("var/" .. fname .. ".next", "w")
  if not out then error(err) end
  ok, err = pcall(out.write, out, j)
  out:close()
  if not ok then error(err) end
  ok, err = os.rename("var/" .. fname .. ".next", "var/" .. fname)
  if not ok then error(err) end
end

function module.load(fname, fatal, notranscode, options)
  local iin, err = io.open("var/" .. fname)
  if not iin then if fatal then error(err) end return end
  local ok, str = pcall(iin.read, iin, "*a")
  iin:close()
  if not ok then error(str) end
  local t = json.decode(str, options)
  return notranscode and t or fromutf8(t)
end

return module
