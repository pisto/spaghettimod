--[[

  Open a tcp pipe to receive lua commands and return the results/errors.
  With some socat trickery this can be turned into a console, like the standalone lua program.

]]--

local module = {}

local fp, lambda = require"utils.fp", require"utils.lambda"
local map, noop, varP, L = fp.map, fp.noop, fp.varP, lambda.L

local function service(token)

  while true do
    local newclient = token.acceptor:accept()
    if not newclient then break end
    newclient:settimeout(0)
    token.newsocklambda(newclient)
    token.clients[newclient] = { recv = "", send = token.multiline and "> " or "", chunk = "" }
  end

  map.np(function(client, buffers)

    while #buffers.send > 0 do
      local i, _, ii = client:send(buffers.send)
      if not i then i = ii end
      if i == 0 then break end
      buffers.send = buffers.send:sub(i + 1)
    end

    while true do
      local data, err, dataa = client:receive(8192)
      if not data then data = dataa end
      if #data == 0 then
        if err == "closed" then client:close() token.clients[client] = nil end
        break
      end
      buffers.recv = buffers.recv .. data
    end

    if buffers.recv == "" then return end
    map.nf(function(cmd)
      if not token.multiline and buffers.chunk == "" and cmd:match("^%s*$") then return end
      local f, err = loadstring(buffers.chunk .. cmd)
      if not f and token.multiline and err:match("%<eof%>$") then buffers.chunk = buffers.chunk .. cmd return end
      buffers.chunk = ""
      if not f then buffers.send = buffers.send .. err .. '\n' return end
      local var = varP(select(2, xpcall(f, spaghetti.stackdumper)))
      if var.n == 0 then return end
      for i = 1, var.n do buffers.send = buffers.send .. (i == 1 and "" or '\t') .. tostring(var[i]) end
      buffers.send = buffers.send .. '\n'
    end, buffers.recv:gmatch("[^\n]*\n"))
    buffers.chunk = buffers.chunk .. buffers.recv:match(".*\n(.*)")
    buffers.recv = ""
    if not token.multiline then return end
    buffers.send = buffers.send .. (buffers.chunk == "" and "> " or ">> ")

  end, token.clients)
end

local function selfservice(token)
  if token.selfservicing then return end
  token.selfservicing = spaghetti.addhook("tick", function() token:service() end)
end

local function close(token)
  if token.selfservicing then spaghetti.removehook(token.selfservicing) end
  map.np(L"_:close()", token.clients)
  token.acceptor:close()
end

function module.create(acceptor, multiline, newsocklambda)
  acceptor:settimeout(0)
  return { acceptor = acceptor, multiline = multiline, service = service, clients = {}, newsocklambda = newsocklambda or noop, selfservice = selfservice, close = close }
end

return module
