--[[

  Correct spawn sequence.

]]--

return function(ci)
  if ci.state.state == engine.CS_DEAD then
    if ci.state.deadflush ~= 0 then
      server.flushevents(ci, ci.state.deadflush)
      ci.state:respawn()
    end
    server.cleartimedevents(ci)
  end
  server.sendspawn(ci)
end
