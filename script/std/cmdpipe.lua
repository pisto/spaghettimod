--[[

  Open a tcp pipe to receive lua commands and return the results/errors.
  With some socat trickery this can be turned into a console, like the standalone lua program.

]]--

local module = {}

local fp, L = require"utils.fp", require"utils.lambda"
local map, varP = fp.map, fp.varP

local function tryrun(chunk)
  local f, err = loadstring(chunk)
  if not f then return not err:match("%'?%<eof%>%'?$"), err .. '\n' end
  local var = varP(xpcall(f, spaghetti.stackdumper))
  if var.n == 1 then return true end
  local tosend = ""
  for i = 2, var.n do tosend = tosend .. (i == 2 and "" or '\t') .. tostring(var[i]) end
  return true, tosend .. '\n'
end

local function run(client)
  local recv, send, chunk = "", "> ", ""
  while true do
    coroutine.yield(true)
    while #send > 0 do
      local i, err, ii = client:send(send)
      if not i then
        if err == "closed" then return end
        i = ii
      end
      if i == 0 then break end
      send = send:sub(i + 1)
    end
    local readsome
    while true do
      local data, err, dataa = client:receive("*l")
      if not data then
        if err == "closed" then
          if dataa and dataa ~= "" then tryrun(chunk .. dataa) end
          return
        end
        if #dataa == 0 then break end
        chunk = chunk .. dataa
      else
        chunk = chunk .. data .. '\n'
        local complete, tosend = tryrun(chunk)
        if complete then chunk, send = "", send .. (tosend or "") end
      end
      readsome = true
    end
    if readsome then send = send .. (#chunk > 0 and ">> " or "> ") end
  end
end

local function service(token)
  while true do
    local newclient = token.acceptor:accept()
    if not newclient then break end
    newclient:settimeout(0)
    token.connectlambda(newclient, true)
    local c = coroutine.create(run)
    coroutine.resume(c, newclient)
    token.clients[newclient] = c
  end
  for client, c in pairs(token.clients) do
    local cret, ret = coroutine.resume(c)
    if not cret or not ret then
      if not cret then engine.writelog("Error in running cmdpipe: " .. ret) end
      token.connectlambda(client, false)
      client:close()
      token.clients[client] = nil
    end
  end
end

local function selfservice(token)
  if token.selfservicing then return end
  token.selfservicing = spaghetti.later(100, function() token:service() end, true)
end

local function close(token)
  if token.selfservicing then spaghetti.cancel(token.selfservicing) end
  map.np(L"_:close()", token.clients)
  token.acceptor:close()
end

function module.create(acceptor, connectlambda)
  acceptor:settimeout(0)
  return { acceptor = acceptor, service = service, clients = {}, connectlambda = connectlambda or L"", selfservice = selfservice, close = close }
end

return module
