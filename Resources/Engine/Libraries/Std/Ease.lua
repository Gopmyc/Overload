--- Easing curves from [0, 1] to [0, 1] (a Back curve goes past them on the way), their input clamped to [0, 1]; a
--- tween takes one as its `ease`. Approach follows a target at a speed, the same whatever the frame rate.
---@class StdEase
local Ease = {}

local function Clamp(t)
	return t < 0 and 0 or (t > 1 and 1 or t)
end

local kOvershoot = 1.70158

---@param t number
---@return number
function Ease.Linear(t)
	return Clamp(t)
end

---@param t number
---@return number
function Ease.InQuad(t)
	t = Clamp(t)
	return t * t
end

---@param t number
---@return number
function Ease.OutQuad(t)
	t = 1 - Clamp(t)
	return 1 - t * t
end

---@param t number
---@return number
function Ease.InOutQuad(t)
	t = Clamp(t)
	return t < 0.5 and 2 * t * t or 1 - 2 * (1 - t) * (1 - t)
end

---@param t number
---@return number
function Ease.InCubic(t)
	t = Clamp(t)
	return t * t * t
end

---@param t number
---@return number
function Ease.OutCubic(t)
	t = 1 - Clamp(t)
	return 1 - t * t * t
end

---@param t number
---@return number
function Ease.InOutCubic(t)
	t = Clamp(t)
	return t < 0.5 and 4 * t * t * t or 1 - 4 * (1 - t) * (1 - t) * (1 - t)
end

--- Draws back below 0 before it goes
---@param t number
---@param overshoot number|nil how far (1.70158 by default)
---@return number
function Ease.InBack(t, overshoot)
	t = Clamp(t)
	local s = overshoot or kOvershoot
	return (s + 1) * t * t * t - s * t * t
end

--- Goes past 1 before it settles
---@param t number
---@param overshoot number|nil how far (1.70158 by default)
---@return number
function Ease.OutBack(t, overshoot)
	t = Clamp(t) - 1
	local s = overshoot or kOvershoot
	return 1 + (s + 1) * t * t * t + s * t * t
end

--- Smooth at both ends: 3t² - 2t³
---@param t number
---@return number
function Ease.SmoothStep(t)
	t = Clamp(t)
	return t * t * (3 - 2 * t)
end

--- Smoother at both ends: 6t⁵ - 15t⁴ + 10t³
---@param t number
---@return number
function Ease.SmootherStep(t)
	t = Clamp(t)
	return t * t * t * (t * (t * 6 - 15) + 10)
end

--- The share of the way to a target that a follower at this speed covers in this time: 1 - e^(-speed × time)
---@param speed number per second; higher follows closer
---@param deltaTime number
---@return number
function Ease.ApproachFactor(speed, deltaTime)
	return 1 - math.exp(-math.max(speed, 0) * math.max(deltaTime, 0))
end

--- A value moved towards a target at a speed: the same path whatever the frame rate. Works on numbers and on vectors
---@generic T
---@param from T
---@param to T
---@param speed number
---@param deltaTime number
---@return T
function Ease.Approach(from, to, speed, deltaTime)
	return from + (to - from) * Ease.ApproachFactor(speed, deltaTime)
end

return Ease
