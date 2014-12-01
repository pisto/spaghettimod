--[[

  Persist a table as json in the var folder.

]]--

local json = require"json"


local module = {}

function module.save(t, fname)
  local j = json.encode(t)
  local out, err, ok = io.open("var/" .. fname .. ".next", "w")
  if not out then error(err) end
  ok, err = pcall(out.write, out, j)
  out:close()
  if not ok then error(err) end
  ok, err = os.rename("var/" .. fname .. ".next", "var/" .. fname)
  if not ok then error(err) end
end

function module.load(fname, fatal)
  local iin, err = io.open("var/" .. fname)
  if not iin then if fatal then error(err) end return end
  local ok, str = pcall(iin.read, iin, "*a")
  iin:close()
  if not ok then error(str) end
  return json.decode(str)
end

return module
