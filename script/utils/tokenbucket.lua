--[[

  A token bucket algorithm, to be used everywhere rate limiting with bursts is needed.

]]--

local function check(bucket, events)
  if events == nil then events = 1 end
  if bucket.tokens < bucket.maxtokens then
    bucket.tokens = math.min(bucket.tokens + bucket.rate * (engine.totalmillis - (bucket.lastupdate or -math.huge))/1000, bucket.maxtokens)
  end
  bucket.lastupdate = engine.totalmillis
  if bucket.tokens < events then
    local done
    if math.floor(events) == events then
      done, bucket.tokens = math.modf(bucket.tokens)
    else done, bucket.tokens = bucket.tokens, 0 end
    return false, done
  end
  bucket.tokens = bucket.tokens - events
  return true, events
end

local function newbucket(rate, maxtokens)
  return setmetatable({ rate = rate, maxtokens = maxtokens or rate, tokens = maxtokens or rate, check = check }, { __call = check })
end

return setmetatable({ newbucket = newbucket }, { __call = function(_, ...) return newbucket(...) end })
