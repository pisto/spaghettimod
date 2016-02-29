local L = require"utils.lambda"

local function noop() end

local function I(...) return ... end

local U = table.unpack or unpack

--[[

	Variadic helpers

]]--

--Only way to implement perfect forwarding, as # is UB when ... contains embedded nils
local function varP(...)
  local ret = {...}
  ret.n = select('#', ...)
  return ret
end

local function varU(packed, s, e)
  s = s ~= nil and s or 1
  e = e ~= nil and e or packed.n
  return U(packed, s, e)
end

local function itervariadic_f(s)
  if s.i < s.n then s.i = s.i + 1 return s[s.i] end
end
local function itervariadic(...)
  local s = varP(...)
  s.i = 0
  return itervariadic_f, s, nil
end

--[[

  Pseudo pointers

]]--

local function mkptr(obj) return {obj} end
local function getptr(obj) return obj[1] end
local function setptr(obj, val) obj[1] = val return val end

--[[

	Functional generator

]]--

--break replacement usable in the user lambda
local breakktag = {}
local function breakk(...) error(setmetatable(varP(...), breakktag)) end

local adderfuncs = {
  l = L"_1[_2] = _3",
  L = L"_1[_2] = {select(3, ...)}",
  g = L"table.insert(_1, _3)",
  G = L"table.insert(_1, {select(3, ...)})",
  s = L"if _3 ~= nil then _1[_3] = true end",
  m = L"if _3 ~= nil then _1[_3] = _4 end",
  n = noop
}

local iterfuncs = {
  f = I,
  p = pairs,
  i = ipairs,
  v = itervariadic,
}

local function getflag(flavor, regex, default)
  local flag, subst = flavor:match(regex) or default:match(regex)
  flavor, subst = flavor:gsub(regex, "")
  if subst > 1 then error("Bad flavor specification") end
  return flag, flavor
end

local function getstacktrace(err)
  if getmetatable(err) == breakktag or type(err) ~= "string" then return err end
  return "Inner handler: {\n\t" .. (debug and debug.traceback(err, 2) or err):gsub("\n", "\n\t") .. "\n}"
end

local function generateflavor(functional, flavor)

  local origflavor = flavor

  --parse flags
  local mtt = getmetatable(functional)
  assert(not flavor:match(mtt.unsupported), "Unsupported flags in flavor specification")
  local adder, iter, rettype
  adder, flavor = getflag(flavor, "[gGlLsmnz]", mtt.default)
  iter, flavor = getflag(flavor, "[fpiv]", mtt.default)
  if adder ~= 'n' and adder ~= 'z' then rettype, flavor = getflag(flavor, "[tau]", mtt.default) end
  assert(#flavor == 0, "Incoherent flavor specification")
  mtt.extracheck(adder, iter, rettype)

  --generate and cache callable flavor that applies the flags and calls the implementation lambda
  if adder ~= 'z' then

    local function body(f, s, var, adder, returntable, iteration, impl_lambda)
      while true do
        local itervalues = varP(f(s, var))
        var = itervalues[1]
        if var == nil then break end
        adder(returntable, getptr(iteration) + 1, impl_lambda(varU(itervalues)))
        setptr(iteration, getptr(iteration) + 1)
      end
    end

    functional[origflavor] = function(...)

      --get the implementation and extract variables from user args
      local adder, returntable, impl_lambda, f, s, var = adderfuncs[adder], mtt.implementation(adder, iter, rettype, ...)
      if rettype ~= 't' then returntable = rettype ~= nil and {} or nil end

      --loop over iterator returns
      local iteration = mkptr(0)
      local ok, err = xpcall(body, getstacktrace, f, s, var, adder, returntable, iteration, impl_lambda)
      --check for errors or breakk()
      iteration = getptr(iteration)
      if not ok then
        if getmetatable(err) ~= breakktag then error(err) end
        iteration = iteration + 1
        adder(returntable, iteration, varU(err))
      end

      --return results
      if rettype == 'u' then return U(returntable, 1, iteration)
      elseif rettype ~= nil then return returntable end

    end

  else

    local function body(f, s, var, impl_lambda)  --handle breakk()
      while true do --pump the iterator till we have something to return
        local itervalues = varP(f(s, getptr(var)))
        setptr(var, itervalues[1])
        if getptr(var) == nil then breakk() end
        local result = varP(impl_lambda(varU(itervalues)))
        if result.n > 0 then return result end
    end
    end

    local function retf(s)
      if s.f == nil then return end
      local ok, result = xpcall(body, getstacktrace, s.f, s.s, s.var, s.impl_lambda)
      --check for errors or breakk()
      if not ok then
        s.f = nil --prevent further calls
        if getmetatable(result) ~= breakktag then error(result) end
      end
      if result.n > 0 then return varU(result) end
    end

    functional[origflavor] = function(...)

      --get the implementation and extract variables from user args
      local _, impl_lambda, f, s, var = mtt.implementation(adder, iter, rettype, ...)
      return retf, {f = f, s = s, var = mkptr(var), impl_lambda = impl_lambda}, nil

    end

  end

  return functional[origflavor]

end

--[[

	Generate a functional.
	The implementation is repeated over the returns of an iterator, and can have an internal state.
	The return type, how the user arguments are transformed into an iterator, and the table to use for output are controlled by these flags:

	input iterator, initializes values... each round ("iter"):
		f => iterator (f,  s, var)
		p => input is table, use pairs()
		i => input is table, use ipairs()
		v => input is variadic, use itervariadic()
	value... = functional_implementation(return_table, current, iteration_index, {values...}, userlambda)
	generated result type ("adderr"):
		l => list (table.insert(result, iteration, value1))
		L => fat list (table.insert(result, iteration, {value...}))
    g => gather (table.insert(result, value1))
    G => fat gather (table.insert(result, {value...}))
		s => set (result[value1] = true)
		m => map (result[value1] = value2)
		z => return an iterator (laZy evaluation) which returns indx, values... Note: the user lambda is called until it returns some values (nil included)
		n => do not compute
	output flags ("rettype"), not applicable if adderr == n, z:
		t => the user will provide the table
		a => allocate a new table
		u => unpack the result

	The implementation can define the supported and default set of flags.
	implementation(adder, iter, rettype, userargs...) must return the table to be used to collect the results, the implementation lambda and the iterator

]]--

local function generatefunctional(implementation, supported, default, extracheck)
  return setmetatable({}, {
    __index = generateflavor,
    implementation = implementation,
    unsupported = "[^" .. supported .. "]",
    default = default,
    __call = function(functional, ...) return functional[default](...) end,
    extracheck = extracheck ~= nil and extracheck or noop
  })
end

--[[

	Map: call repeatedly the lambda over the input values and collect the result.
	Supports all flags.

]]--

local function mapflavors(adder, iter, rettype, ...)
  local offset, userarg, user_lambda = (rettype == 't' and 1 or 0), varP(...)
  local function impl_lambda(...) return user_lambda(...) end
  user_lambda = userarg[offset + 1]
  return userarg[offset], impl_lambda, iterfuncs[iter](varU(userarg, offset + 2))
end

local map = generatefunctional(mapflavors, "vampfilLsnutzgG", "ila")

--[[

	Numeric range.
	Supports all rettype flags, list and lazy iterator.

]]--

local function rangeiterator(s, var)
  if s.mult*(var+s.i) <= s.mult*s.e then return var+s.i end
end

local function genrangeiterator(b, e, i)
  i = i~=nil and i or 1
  local error_check = b+e*1+i*1
  return rangeiterator, {e = e, mult = i > 0 and 1 or -1, i = i}, b - i
end

local function rangeflavors(adder, iter, rettype, ...)
  local offset, userarg = (rettype == 't' and 1 or 0), varP(...)
  return userarg[offset], I, genrangeiterator(varU(userarg, offset + 1))
end

local range = generatefunctional(rangeflavors, "zalutg", "la")
range.z  = genrangeiterator

--[[

	Fold (nest): call the lambda over itself and optional additional arguments passed by the iterator.
	If there are no additional parameters, this is simply f(f(f(x0)))..., and an additional argument specifying the recursion
	must be passed after the initial value specification.
	Supports all rettype. ipairs, explicit iterator and variadic as input, use the explicit iterator parallelipairs() to pass
	more than one additional argument. Builds a set, a list, nothing or a lazy iterator.

]]--

local function foldflavors(adder, iter, rettype, ...)
  local offset, userarg, user_table, user_lambda, f0 = (rettype == 't' and 1 or 0), varP(...), {}
  local dummyiterator = false
  local function impl_lambda(...)
    if dummyiterator then f0 = user_lambda(f0)
    else f0 = user_lambda(f0, ...) end
    return f0
  end
  if offset == 1 then user_table = userarg[1] end
  user_lambda, f0 = userarg[offset + 1], userarg[offset + 2]
  --no additional arguments case (nest)
  if iter == 'v' or (userarg.n ~= offset + 2 and (type(userarg[offset + 3]) ~= "number" or userarg.n > offset + 3)) then
    return user_table, impl_lambda, iterfuncs[iter](varU(userarg, offset + 3))
  end
  dummyiterator = true
  return user_table, impl_lambda, range.z(1, userarg.n ~= offset + 2 and userarg[offset + 3] or 1/0)
end

local fold = generatefunctional(foldflavors, "fatzinsluvg", "ila")

--[[

	Pick: select iterator values over which the lambda evaluates to true.
	Supports all rettypes, iterators, set, map and lazy iterator.

]]--

local function pickflavors(adder, iter, rettype, ...)
  local offset, userarg, user_table, user_lambda = (rettype == 't' and 1 or 0), varP(...), {}
  local function impl_lambda(...)
    local args = varP(...)
    local ok, result = xpcall(function() return user_lambda(varU(args)) end, getstacktrace)
    if not ok then
      if getmetatable(result) == breakktag then
        if result[1] then breakk(...) end
        breakk()
      end
      error(result)
    end
    if result then return ... end
  end
  if offset == 1 then user_table = userarg[1] end
  user_lambda = userarg[offset + 1]
  return user_table, impl_lambda, iterfuncs[iter](varU(userarg, offset + 2))
end

local pick = generatefunctional(pickflavors, "mstfapviz", "pma")

local function eq (uv, indx) return function(...) return ({...})[indx or 2] == uv end end
local function neq(uv, indx) return function(...) return ({...})[indx or 2] ~= uv end end
local function lt (uv, indx) return function(...) return ({...})[indx or 2] <  uv end end
local function leq(uv, indx) return function(...) return ({...})[indx or 2] <= uv end end
local function gt (uv, indx) return function(...) return ({...})[indx or 2] >  uv end end
local function geq(uv, indx) return function(...) return ({...})[indx or 2] >= uv end end

local function upto(max, test)
  local found = 0
  return function(...)
    local ret = test(...)
    if ret then
      found = found + 1
      if found >= max then breakk(true) end
    end
    return ret
  end
end
local function once(test) return upto(1, test) end

--[[

	Extended table utilities

]]--

local originsert, origsort = table.insert, table.sort

function table.insert(t, ...)
  originsert(t, ...)
  return t
end

function table.sort(t, ...)
  origsort(t, ...)
  return t
end

--[[

	Additional iterators.

]]--

local function pi(s, var)
  var = var + 1
  local ret = map(function(_, table)
    local v = table[var]
    return v ~= nil and v or breakk()
  end, s)
  if #ret < #s then return end
  return var, U(ret)
end
--returns t1[i], t2[i], t3[i], ... up to first nil in any table
local function parallelipairs(...) return pi, {...}, 0 end

local function si(s, var)
  local ret = s[s.currenttable][var + 1 - s.offset]
  if ret ~= nil then return var + 1, ret end
  if s[s.currenttable + 1] == nil then return end
  s.currenttable = s.currenttable + 1
  s.offset = var
  return si(s, var)
end
--concatenate ipairs() for the passed tables
local function serialipairs(...) return si, { currenttable = 1, offset = 0, ... }, 0 end

local function sp(s, var)
  local var, v = next(s[s.currenttable], var)
  if var ~= nil then return var, v end
  if s[s.currenttable + 1] == nil then return end
  s.currenttable = s.currenttable + 1
  return sp(s)
end
--concatenate pairs() for the passed tables
local function serialpairs(...) return sp, { currenttable = 1, ... } end


local function first(...)
  local arg, t = varP(...), ...
  if arg.n == 1 then return t[1] end
  return arg[1](arg[2], arg[3])
end

local function last(...)
  local arg, t = varP(...), ...
  if arg.n == 1 then return t[#t] end
  local last, f, s, var = varP(), ...
  while true do
    local new = varP(f(s, var))
    var = new[1]
    if var == nil then return varU(last) end
    last = new
  end
end

return {
  noop = noop,
  I = I,
  U = U,
  varP = varP,
  varU = varU,
  mkptr = mkptr,
  getptr = getptr,
  setptr = setptr,
  breakk = breakk,
  map = map,
  range = range,
  pick = pick,
  fold = fold,
  eq = eq,
  neq = neq,
  lt = lt,
  leq = leq,
  gt = gt,
  geq = geq,
  upto = upto,
  once = once,
  parallelipairs = parallelipairs,
  serialipairs = serialipairs,
  first = first,
  last = last
}
