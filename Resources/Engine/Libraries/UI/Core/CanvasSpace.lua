--- Conversions between the viewport (pixels, top-left origin) and canvas space (canvas units,
--- top-left origin). Mirrors UIRenderingUtils::GetCanvasScale and GetCanvasSize, which aren't
--- exposed to Lua (see Docs/LuaExposureGaps.md).
---@class UICanvasSpace
local CanvasSpace = {}

local kMinimumScale = 0.0001

local function IsFinite(value)
	return value == value and value ~= math.huge and value ~= -math.huge
end

local function ClampCanvasSize(value)
	return IsFinite(value) and math.max(value, 1) or 1
end

local function ClampFinite(value, minimum)
	return IsFinite(value) and math.max(value, minimum) or minimum
end

--- Returns the number of viewport pixels covered by one canvas unit
---@param canvas Canvas
---@param viewportWidth number
---@param viewportHeight number
---@return number
function CanvasSpace.GetScale(canvas, viewportWidth, viewportHeight)
	local scaleFactor = ClampFinite(canvas:GetScaleFactor(), kMinimumScale)

	if canvas:GetScalerMode() == CanvasScalerMode.CONSTANT_PIXEL_SIZE then
		return scaleFactor
	end

	local reference = canvas:GetReferenceResolution()
	local widthScale = ClampCanvasSize(viewportWidth) / ClampCanvasSize(reference.x)
	local heightScale = ClampCanvasSize(viewportHeight) / ClampCanvasSize(reference.y)
	local matchMode = canvas:GetScreenMatchMode()
	local screenScale

	if matchMode == CanvasScreenMatchMode.EXPAND then
		screenScale = math.min(widthScale, heightScale)
	elseif matchMode == CanvasScreenMatchMode.SHRINK then
		screenScale = math.max(widthScale, heightScale)
	else
		local match = math.min(math.max(canvas:GetMatchWidthOrHeight(), 0), 1)
		local logWidth = math.log(math.max(widthScale, kMinimumScale), 2)
		local logHeight = math.log(math.max(heightScale, kMinimumScale), 2)
		screenScale = 2 ^ (logWidth + (logHeight - logWidth) * match)
	end

	return ClampFinite(screenScale * scaleFactor, kMinimumScale)
end

--- Returns the canvas size in canvas units
---@param canvas Canvas
---@param viewportWidth number
---@param viewportHeight number
---@return number, number
function CanvasSpace.GetSize(canvas, viewportWidth, viewportHeight)
	if canvas:GetScalerMode() == CanvasScalerMode.CONSTANT_PIXEL_SIZE then
		local reference = canvas:GetReferenceResolution()
		return ClampCanvasSize(reference.x), ClampCanvasSize(reference.y)
	end

	local scale = CanvasSpace.GetScale(canvas, viewportWidth, viewportHeight)
	return ClampCanvasSize(ClampCanvasSize(viewportWidth) / scale), ClampCanvasSize(ClampCanvasSize(viewportHeight) / scale)
end

--- Converts a viewport position into canvas space. The canvas is centered in the viewport.
---@param canvas Canvas
---@param viewportWidth number
---@param viewportHeight number
---@param x number
---@param y number
---@return number, number
function CanvasSpace.ViewportToCanvas(canvas, viewportWidth, viewportHeight, x, y)
	local scale = CanvasSpace.GetScale(canvas, viewportWidth, viewportHeight)
	local canvasWidth, canvasHeight = CanvasSpace.GetSize(canvas, viewportWidth, viewportHeight)

	return
		(x - ClampCanvasSize(viewportWidth) * 0.5) / scale + canvasWidth * 0.5,
		(y - ClampCanvasSize(viewportHeight) * 0.5) / scale + canvasHeight * 0.5
end

return CanvasSpace
