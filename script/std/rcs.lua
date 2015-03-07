--[[

  Remote CubeScript: (ab)use the maptitle var to send cubescript to clients, which will run it thanks to code injected by rcs_pseudomaster.
  A runnable maptitle is composed by a magic header ("\fs\f4Remote CubeScript...\fr") followed by these fields:
    I      => the ID of the command to be acknowledged by the client with "servcmd rcsack $ID"
    P      => a "packetization" code. Since cubescript payloads may be larger than the client network buffer, it may be needed to split them.
            "R" (run)   => the payload is to be executed directly.
            "H" (head)  => first piece of a splitted and escaped cubescript payload (client clears and initializes the buffer with the payload)
            "M" (middle)=> additional pieces of cubescript (client appends)
            "T" (tail)  => last piece, client appends, unescapes, runs and clears the buffer.
    <data> => the cubescript payload, either raw or escaped.

]]--

local fp, L, putf, n_client, iterators = require"utils.fp", require"utils.lambda", require"std.putf", require"std.n_client", require"std.iterators"

local module = {}

local magicheader = "\f4Remote CubeScript... "
local magictitle = magicheader .. "IP"
local maxtransfer = 5000 - 1 - #magictitle

local function sendmaptitle(ci, value, to)
  engine.sendpacket(to and to.clientnum or ci.clientnum, 1, n_client(putf({ 20, r = 1}, server.N_EDITVAR, engine.ID_SVAR, "maptitle", value), ci):finalize(), -1)
end

spaghetti.addhook(server.N_EDITVAR, L"_.skip = true")
spaghetti.addhook(server.N_NEWMAP, function(info)
  if info.skip or not server.m_edit or info.ci.state.state == engine.CS_SPECTATOR or info.ci.privilege == server.PRIV_NONE then return end
  info.skip = true
  local p = n_client(putf({10, r=1}, server.N_NEWMAP, info.size), info.ci):finalize()
  for ci in iterators.clients() do if ci.clientnum ~= info.ci.clientnum then
    engine.sendpacket(ci.clientnum, 1, p, -1)
    if ci.extra.rcs and ci.extra.rcs.lastsent then sendmaptitle(ci, ci.extra.rcs.lastsent) end
  end end
end)

local function badrcs(ci, cmd, comment)
  local msg = ("rcs: bad command from %s: \"%s\""):format(server.colorname(ci), cmd)
  if comment then msg = msg .. " (" .. comment .. ')' end
  engine.writelog(msg)
end

local function dequeue(ci)
  local rcs = ci.extra.rcs
  local msg = table.remove(rcs.queued, 1):gsub("I", rcs.nextack, 1)
  sendmaptitle(ci, msg)
  rcs.waitack, rcs.lastsent = rcs.nextack, msg
  rcs.nextack = (rcs.nextack + 1) % 10
end

local function rcsack(ci, ack, args, fullcmd)
  if not ci.extra.rcs then badrcs(ci, fullcmd, "no rcshello received") return end
  local waitack = ci.extra.rcs.waitack
  if waitack ~= ack then badrcs(ci, fullcmd, waitack and "expected ack " .. waitack or "not expected") return end
  ci.extra.rcs.waitack, ci.extra.rcs.lastsent = nil
  return #ci.extra.rcs.queued > 0 and dequeue(ci)
end

function module.send(ci, cs)
  assert(ci.extra.rcs, "Cannot send rcs to a client that didn't acknowledge support")
  cs = engine.escapestring(cs):sub(2, -2)
  if #cs <= maxtransfer then table.insert(ci.extra.rcs.queued, magictitle:gsub("P", "R") .. cs)
  else
    local senthead, chunk
    repeat
      table.insert(ci.extra.rcs.queued, magictitle:gsub("P", not senthead and "H" or #cs <= maxtransfer and "T" or "M") .. cs:sub(1, maxtransfer))
      senthead, cs = true, cs:sub(1 + maxtransfer)
    until #cs == 0
  end
  return not ci.extra.rcs.waitack and dequeue(ci)
end

local handlers = {
  rcsack = function(ci, args, fullcmd)
    local version = args:match("^%d")
    if not version then badrcs(ci, fullcmd) return end
    rcsack(ci, tonumber(version), args, fullcmd)
  end,
  rcshello = function(ci, args, fullcmd)
    local version, installed = args:match("^(%d+) ?(.*)")
    if not version then badrcs(ci, fullcmd) return end
    version, installed = tonumber(version), installed == "installed"
    if version ~= 0 then badrcs(ci, fullcmd, "unknown version " .. version) return end
    if not ci.extra.rcs or installed then engine.writelog("rcs: hello" .. (installed and " installed" or "") .. " " .. server.colorname(ci, nil)) end
    if not ci.extra.rcs then
      ci.extra.rcs = { version = 0, queued = {},  nextack = 0 }
      local hooks = spaghetti.hooks.rcshello
      if hooks then hooks({ ci = ci, installed = installed }) end
    end
  end,
  rcsuninstall = function(ci, args, fullcmd)
    if not ci.extra.rcs then badrcs(ci, fullcmd, "no rcshello received") return end
    ci.extra.rcs = nil
    engine.writelog("rcs: uninstall  " .. server.colorname(ci, nil))
    local hooks = spaghetti.hooks.rcsuninstall
    return hooks and hooks({ ci = ci })
  end,
}

spaghetti.addhook(server.N_SERVCMD, function(info)
  if info.skip or not info.text:match("^rcs") then return end
  info.skip = true
  local cmd, args = info.text:match("^(rcs[^ ]*) ?(.*)")
  local handler = handlers[cmd]
  if not handler then badrcs(info.ci, cmd) return end
  handler(info.ci, args, info.text)
end, true)

return module
