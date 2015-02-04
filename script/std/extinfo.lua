--[[

  A module to tamper with extinfo.

]]--

spaghetti.addhook("ping", function(info)
  local hooks = spaghetti.hooks.extinfo
  if not hooks then return end
  local req = info.req
  local millis = req:getint()
  local data = { millis = millis, code = millis and req:getint() or nil, skip = false, req = req, p = info.p }
  hooks(data)
  if data.skip then info.skip = true
  else req.len = 0 end
end)
