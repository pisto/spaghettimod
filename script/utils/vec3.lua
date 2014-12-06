--[[

  Operations on three dimensional vectors. Not a sauer vec struct.

]]--

local methods = {}

function methods.magnitude(this)
  return math.sqrt(this.x^2 + this.y^2 + this.z^2)
end

function methods.dot(this, o)
  return this.x * o.x + this.y * o.y + this.z * o.z;
end

function methods.mul(this, scalar)
  this.x, this.y, this.z = this.x * scalar, this.y * scalar, this.z * scalar
  return this
end

function methods.div(this, scalar)
  return this:mul(1 / scalar)
end

function methods.add(this, o)
  this.x, this.y, this.z = this.x + o.x, this.y + o.y, this.z + o.z
  return this
end

function methods.sub(this, o)
  this.x, this.y, this.z = this.x - o.x, this.y - o.y, this.z - o.z
  return this
end

function methods.neg(this)
  this.x, this.y, this.z = -this.x, -this.y, -this.z
  return this
end

function methods.dist(this, o)
  return math.sqrt((this.x - o.x)^2 + (this.y - o.y)^2 + (this.z - o.z)^2)
end

function methods.normalize(vec)
  return vec:mul(1 / vec:magnitude())
end

function methods.sauer(vec)
  local sauer = engine.vec()
  sauer.x, sauer.y, sauer.z = vec.x, vec.y, vec.z
  return sauer
end

local meta = { __index = methods, __tostring = function(vec) return '{' .. tostring(vec.x) .. ", " .. tostring(vec.y) .. ", " .. tostring(vec.z) .. '}' end }
return function(...)
  local this, nargs, o = setmetatable({}, meta), select('#', ...), ...
  if nargs == 3 then this.x, this.y, this.z = ...
  elseif nargs == 1 then this.x, this.y, this.z = o.x, o.y, o.z
  elseif nargs == 0 then this.x, this.y, this.z = 0, 0, 0
  else error("Bad constructor arguments") end
  return this
end
