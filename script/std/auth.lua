--[[

  Take over auth control from C++. Support multiple concurrent auth requests, domain providers, auth before join (preauth).

  A provider is a function called when an auth request begins, and it returns the an auth_handler and a completion token:
    auth_handler, completion = provider(initiating_client, auth_domain, auth_account)
  auth_handler can be:
    false or nil                => ignore auth request
    "string"                    => interpret auth_handler as the pubkey, the module will carry out the auth sequence
    callable({ auth_state... }) => call it to obtain the challenge, the answer, or the confirmation that auth succeeded
  completion can be:
    false or nil or number      => interpret as a privilege code (no value is the same as privilege 'N' in adduser)
    callable(result, initiating_client, auth_domain, auth_account, error) => callable invoked when auth suceeds of fails

  If auth_handler is a callable, it is expected to read from and populate the auth_state table to communicate with this module. The fields of this table can be:
    ci            => the initiating client
    domain        => the auth domain
    user          => the auth account
    pubkey        => the public key
    if the callable cannot provide the pubkey, it must provide
      challenge   => the challenge
      answer      => the answer
      if the callable cannot provide the answer, the module sets it to the client answer, and the callable must provide
        result    => a true or false value
        error     => error value to be passed to the completion callable 
  The auth_handler shall set result = false at any time to force an interruption of the auth process. When the module handles the auth process details, it can return the following errors:
    "failauth"    => the client did not reply with a correct answer to the challenge
    "disconnect"  => the client disconnected before auth completed
    "error"       => the auth_handler callable threw an error
  If your auth_handler needs always deinitialization, you can return it as the completion callable, as it will be called in any case.

  For an example on how to build a domain handler, see the gauth handler at the bottom of this file.

]]--

local fp, L, playermsg, putf, parsepacket, uuid = require"utils.fp", require"utils.lambda", require"std.playermsg", require"std.putf", require"std.parsepacket", require"std.uuid"
local map = fp.map

local module = { domains = {} }


--take over local auth control
local localauths = {}
local privcodes = { a = server.PRIV_ADMIN, m = server.PRIV_AUTH, n = server.PRIV_NONE }
map.mtp(privcodes, L"_1:upper(), _2", privcodes)
rawset(cs, "adduser", function(user, domain, pubkey, privilege)
  localauths[domain] = localauths[domain] or {}
  localauths[domain][user] = { pubkey = pubkey, privilege = privcodes[privilege] or server.PRIV_AUTH }
end)
rawset(cs, "clearusers", function(domain)
  if not domain then localauths = {} else localauths[domain] = nil end
end)
rawset(cs, "listusers", function(domain)
  return pairs(localauths[domain] or {})
end)
local function lookuplocal(user, domain)
  user = (localauths[domain] or {})[user] or {}
  return user.pubkey, user.privilege
end



--utilities
local function sendchallenge(ci, authid, domain, challenge)
  engine.sendpacket(ci.clientnum, 1, putf({ 30, r = 1}, server.N_AUTHCHAL, domain, authid, challenge):finalize(), -1)
end

local urandom = io.open"/dev/urandom"
function module.genchallenge(pubkey)
  local seed = urandom and urandom:read(24)
  if not seed then for i = 1, 6 do seed = (seed or "") .. ('%.13A'):format(math.random()) end end
  return engine.genchallenge(pubkey, seed)
end

function module.intersectauths(auths1, auths2)
  local intersect = map.mp(function(domain, names)
    if type(domain) ~= "string" or not auths2[domain] then return end
    return domain, map.sp(function(name)
      local found = auths2[domain]
      if type(found) ~= "table" then return found end
      return found[name]
    end, names)
  end, auths1)
  return next(intersect) and intersect
end

spaghetti.addhook("clientconnect", L"_.ci.extra.allclaims = {}")




--main auth implementation
local authid = 0

local function failpreauth(ci)
  if ci.connectauth ~= 0 then engine.disconnect_client(ci.clientnum, ci.connectauth) end
end

local pumpproc
local waitproc = setmetatable({}, { __index = {}, __newindex = function(waitproc, authid, proc)
  local meta = getmetatable(waitproc)
  local real = meta.__index
  real[authid] = proc
  if next(real) then
    meta.procrunner = meta.procrunner or spaghetti.later(50, function()
      for authid in pairs(real) do pumpproc(authid) end
    end, true)
    if proc then pumpproc(authid) end
  elseif meta.procrunner then
    spaghetti.cancel(meta.procrunner)
    meta.procrunner = nil
  end
end })

local function finishprocess(ci, authid)
  local authprocs = ci.extra.authprocs
  local process = authprocs[authid]
  local arg = process.arg
  authprocs[authid], waitproc[authid] = nil
  if not next(authprocs) then ci.extra.authprocs = nil end
  if arg.result then
    local domain = ci.extra.allclaims[process.domain] or {}
    domain[process.user] = true
    ci.extra.allclaims[process.domain] = domain
  end
  local ok, err = xpcall(process.completion, spaghetti.stackdumper, arg.result, ci, arg.domain, arg.user, arg.error)
  return not ok and engine.writelog("an auth completion resulted in an error: " .. err)
end

pumpproc = function(authid)
  local process = waitproc[authid]
  local arg = process.arg
  local ok, err = xpcall(process.handler, spaghetti.stackdumper, arg)
  if not ok then
    if not arg.error then
      engine.writelog("an auth handler resulted in an error: " .. err)
      arg.result, arg.error = false, "error"
    end
    finishprocess(process.ci, authid)
    return
  end
  if arg.result ~= nil then finishprocess(process.ci, authid)
  elseif process.challenged then return end
  if arg.pubkey then arg.challenge, arg.answer = module.genchallenge(arg.pubkey)
  elseif not arg.challenge then return end
  sendchallenge(process.ci, authid, process.domain, arg.challenge)
  waitproc[authid] = nil
  process.challenged = true
end

module.privcompletions = {}

for priv = 0, 4 do module.privcompletions[priv] = function(result, ci, domain, user, error)
  local kci, reason = engine.getclientinfo(ci.authkickvictim), ci.authkickreason
  ci:cleanauthkick()
  if result then
    if ci.connectauth ~= 0 then server.connected(ci) end
    if kci then
      if server.setmaster(ci, true, "", user, domain, priv, false, true) then
        server.trykick(ci, kci.clientnum, reason, user, domain, priv)
      end
    else
      local oldpriv = ci.privilege
      server.setmaster(ci, true, "", user, domain, priv)
      if oldpriv == ci.privilege then
        playermsg("You authenticated as '\f5" .. user .. "\f7' " .. (domain == "" and "" or  "[\f0" .. domain .. "\f7]"), ci)
      end
    end
    return
  end
  if error == "disconnect" then return end
  local reason = ({ failauth = " because your authkey is invalid.", error = " because of an internal error." })[error] or ": " .. error
  playermsg(domain == "" and
    ("Cannot complete your gauth request as '\f5%s\f7'%s"):format(user, reason) or
    ("Cannot complete your auth request as '\f5%s\f7' [\f0%s\f7]%s"):format(user, domain, reason), ci)
  engine.writelog(("auth: fail %s (%d) as %s [%s]: "):format(ci.name, ci.clientnum, user, domain, tostring(error)))
end end

spaghetti.addhook("tryauth", function(info)
  if info.skip then return end
  info.skip, info.result = true
  local ci, domain, user = info.ci, info.desc, engine.filtertext(info.user):sub(1, 100)
  local process = { ci = ci, domain = domain, user = user, arg = { ci = ci, domain = domain, user = user } }
  local pubkey, privilege = lookuplocal(user, domain)
  if pubkey then process.completion, process.arg.challenge, process.arg.answer = module.privcompletions[privilege or server.PRIV_NONE], module.genchallenge(pubkey)
  else
    local provider = module.domains[domain]
    if not provider then
      playermsg(("Cannot find auth '\fs\f5%s\fr' [\fs\f0%s\fr]"):format(user, domain), ci)
      return failpreauth(ci)
    end
    local handler, completion = provider(ci, domain, user)
    if not handler then failpreauth(ci) return end
    process.completion = (not completion or type(completion) == "number") and module.privcompletions[completion or server.PRIV_NONE] or completion
    if type(handler) == "string" then process.arg.challenge, process.arg.answer = module.genchallenge(handler)
    else process.handler = handler end
  end
  info.result = true
  authid = authid + 1
  ci.extra.authprocs = ci.extra.authprocs or {}
  ci.extra.authprocs[authid] = process
  if process.arg.challenge then
    sendchallenge(ci, authid, domain, process.arg.challenge)
    process.challenged = true
  else waitproc[authid] = process end
end)

spaghetti.addhook("answerchallenge", function(info)
  if info.skip then return end
  info.skip, info.result = true
  local process = (info.ci.extra.authprocs or {})[info.id]
  if not process or not process.arg.challenge then
    info.result = info.ci.connectauth == 0
    return
  end
  local answer = info.val:match"^%x+"
  if process.arg.answer then
    info.result = process.arg.answer == answer
    process.arg.result, process.arg.error = info.result, not info.result and "failauth" or nil
    finishprocess(info.ci, info.id)
    return
  end
  info.result, process.arg.answer, waitproc[info.id] = true, answer, process
end)

spaghetti.addhook("clientdisconnect", function(info)
  for authid, process in pairs(info.ci.extra.authprocs or {}) do
    process.arg.result, process.arg.error = false, "disconnect"
    finishprocess(info.ci, authid)
  end
end)



--preauth
require"std.limbo"
local fence = require"std.fence"

module.preauths = {}

spaghetti.addhook("enterlimbo", function(info)
  local reqauths
  for _, desc in ipairs(module.preauths) do reqauths = putf(reqauths or { 100, r = 1}, server.N_REQAUTH, desc) end
  if not reqauths then return end
  engine.sendpacket(info.ci.clientnum, 1, reqauths:finalize(), -1)
  info.ci.extra.limbo.locks.preauth = 1/0
  local ciuuid = info.ci.extra.uuid
  spaghetti.later(100, function()   --need to compensate that a fence is answered immediately, while tryauth() goes through normal packet coalescing
    local ci = uuid.find(ciuuid)
    if not ci or not ci.extra.limbo or ci.extra.limbo.locks.preauth ~= 1/0 then return end
    ci.extra.reqauthfence = fence(ci)
  end)
end)

spaghetti.addhook("fence", function(info)
  local extra = info.ci.extra
  if extra.reqauthfence ~= info.fence then return end
  extra.reqauthfence = nil
  info.ci.extra.limbo.locks.preauth = 0
end)

spaghetti.addhook("martian", function(info)
  if info.skip or info.ci.connected or info.ratelimited then return end
  if info.type == server.N_AUTHTRY and not parsepacket(info) then
    info.ci.extra.reqauthfence = nil
    info.ci.extra.limbo.locks.preauth = 1000
    local desc, name = map.uv(L"_:sub(1, server.MAXSTRLEN)", info.desc, info.name)
    local hooks, authinfo = spaghetti.hooks[server.N_AUTHTRY], setmetatable({ skip = false, desc = desc, name = name }, { __index = info, __newindex = info })
    if hooks then hooks(authinfo) end
    if not authinfo.skip then server.tryauth(info.ci, authinfo.name, authinfo.desc) end
    info.skip = true
  elseif info.type == server.N_AUTHANS and not parsepacket(info) then
    local p = info.p
    local authinfo = setmetatable({ skip = false, desc = info.desc:sub(1, server.MAXSTRLEN), id = info.id % 2^32, ans = info.ans:sub(1, server.MAXSTRLEN) }, { __index = info, __newindex = info })
    local hooks = spaghetti.hooks[server.N_AUTHANS]
    if hooks then hooks(info) end
    if not authinfo.skip then server.answerchallenge(info.ci, authinfo.id, authinfo.ans, authinfo.desc) end
    info.skip = true
  end
end, true)



--gauth
local waitgauth = {}
local mastercmds = map.sv(L"_", "failauth", "chalauth", "succauth")
spaghetti.addhook("masterin", function(info)
  if info.skip then return end
  local cmd, arg = info.input:match("^(%a+) (%d+)")
  arg = waitgauth[tonumber(arg)]
  if not mastercmds[cmd] or not arg or arg.cmd then return end
  info.skip, arg.cmd = true, info.input
end)
spaghetti.addhook("masterdisconnected", function()
  for _, arg in pairs(waitgauth) do arg.result, arg.error = false, "not connected to authentication server" end
  waitgauth = {}
end)

local function askmaster(arg, request)
  if engine.requestmaster(request .. '\n') then return true end
  arg.result, arg.error = false, "not connected to authentication server"
  error(arg.error)
end
local function waitfield(t, f)
  repeat coroutine.yield() until t[f]
  return t[f]
end
local function waitmaster(arg)
  local cmd = waitfield(arg, "cmd")
  arg.cmd = nil
  return cmd
end

local gauthid = 0
module.domains[""] = function()
  gauthid = gauthid + 1
  local gauthid = gauthid
  return coroutine.wrap(function(arg)
    waitgauth[gauthid] = arg
    askmaster(arg, ("reqauth %d %s"):format(gauthid, arg.user))
    arg.challenge = waitmaster(arg):match"chalauth %d+ ([%+%-]%x+)"
    askmaster(arg, ("confauth %d %s"):format(gauthid, waitfield(arg, "answer")))
    arg.result = waitmaster(arg):match"succauth" or false
    arg.error = not arg.result and not arg.error and "failauth" or nil
  end), function(...)
    waitgauth[gauthid] = nil
    module.privcompletions[server.PRIV_AUTH](...)
  end
end


return module
