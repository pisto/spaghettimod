--[[

  Futures. Naive and inefficient implementation.

]]--

local module = {}

local lambda = require"utils.lambda"
local Lr = lambda.Lr

local futures, tickhooktoken = {}

function module.later(delay, lambda, rep)
  local token = { when = delay + engine.totalmillis, lambda = lambda }
  if rep then token.delay = delay end
  table.sort(table.insert(futures, token), Lr"_1.when < _2.when")
  if not tickhooktoken then
    tickhooktoken = spaghetti.addhook("tick", function()
      while futures[1] and futures[1].when <= engine.totalmillis do
        local f = table.remove(futures, 1)
        local ok, msg = xpcall(f.lambda, spaghetti.stackdumper)
        if not ok then engine.writelog("One future of resulted in an error" .. (r.delay and " and has been cancelled" or "") .. ": " .. msg)
        elseif f.delay then
          f.when = f.when + f.delay
          table.sort(table.insert(futures, token), Lr"_1.when < _2.when")
        end
      end
      if #futures == 0 then spaghetti.removehook("tick", tickhooktoken) end
    end)
  end
  return token
end

function module.cancel(token)
  for i, v in futures do if v == token then table.remove(futures, i) return end end
end

return module
