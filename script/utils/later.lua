--[[

  Futures. Naive and inefficient implementation.

]]--

local module = {}

local lambda, fp = require"utils.lambda", require"utils.fp"
local Lr = lambda.Lr

local queues = { abs = {}, game = {} }

local function later(delay, lambda, rep, futures, timer)
  local token = { when = delay + timer(), lambda = lambda, delay = rep and delay, futures = futures }
  table.sort(table.insert(futures, token), Lr"_1.when < _2.when")
  if not futures.tickhooktoken then
    futures.tickhooktoken = spaghetti.addhook("tick", function()
      while futures[1] and futures[1].when <= timer() do
        local f = table.remove(futures, 1)
        local ok, msg = xpcall(f.lambda, spaghetti.stackdumper)
        if not ok then engine.writelog("One future of resulted in an error" .. (f.delay and " and has been cancelled" or "") .. ": " .. msg)
        elseif f.delay then
          f.when = f.when + f.delay
          table.sort(table.insert(futures, f), Lr"_1.when < _2.when")
        end
      end
      if #futures == 0 then spaghetti.removehook(futures.tickhooktoken) end
    end)
  end
  return token
end

function module.cancel(token)
  for i, v in ipairs(token.futures) do if v == token then table.remove(token.futures, i) return end end
end

function module.later(delay, lambda, rep)
  return later(delay, lambda, rep, queues.abs, Lr"engine.totalmillis")
end

function module.latergame(delay, lambda, rep)
  local token = later(delay, lambda, rep, queues.game, Lr"server.gamemillis")
  spaghetti.addhook("changemap", function() module.cancel(token) end)
end

return module
