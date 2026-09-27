local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")

Resources.GetScript(":Libraries/UI/Panels/Panel.lua")

--- Line through points local to the panel, drawn as rotated segments. The engine has no drawing API
--- (see Docs/EngineLimitations.md): each segment is a child panel with an Image, so joints are only
--- approximated and a translucent color shows where segments overlap.
---@class DPolyline : Panel
local DPolyline = {}

local kDefaultColor = Vector4.new(1, 1, 1, 1)

local function Distance(a, b)
	local dx, dy = b.x - a.x, b.y - a.y
	return math.sqrt(dx * dx + dy * dy)
end

local function AcquireSegment(self, index)
	local segment = self.m_Segments[index]

	if not segment then
		segment = self:Add("Panel")
		segment:SetMouseInputEnabled(false)
		segment:SetTransformOrigin(0, 0.5)
		segment.m_Image = segment:GetActor():AddImage()
		segment.m_Image:SetTexture(self.m_Texture)
		segment.m_Image:SetTint(self.m_Color)
		self.m_Segments[index] = segment
	end

	segment:SetVisible(true)
	return segment
end

-- Draws the segment from a to b, starting extend units before a to cover the joint with the previous one
local function PlaceSegment(self, index, a, b, thickness, extend)
	local length = Distance(a, b)
	local segment = AcquireSegment(self, index)

	if length == 0 then
		segment:SetVisible(false)
		return
	end

	local directionX, directionY = (b.x - a.x) / length, (b.y - a.y) / length
	local startX, startY = a.x - directionX * extend, a.y - directionY * extend

	-- The transform origin, at the middle of the left edge, is placed on the start point
	segment:SetDrivenPos(startX, startY - thickness / 2)
	segment:SetSize(length + extend, thickness)
	segment:SetRotation(math.deg(math.atan(-directionY, directionX)))
end

function DPolyline:Init()
	self:SetMouseInputEnabled(false)

	self.m_Points = {}
	self.m_Thickness = 4
	self.m_Color = kDefaultColor
	self.m_Texture = nil
	self.m_Closed = false
	self.m_Fraction = 1
	self.m_Segments = {}
end

--- Defines the points of the line, in coordinates local to the panel. A point can define its own
--- thickness with w, the thickness of a segment being the average of its ends.
---@param points { x: number, y: number, w: number|nil }[]
function DPolyline:SetPoints(points)
	assert(type(points) == "table", "UI: SetPoints expects a list of points")

	self.m_Points = points
	self:InvalidatePaint()
end

---@return table
function DPolyline:GetPoints()
	return self.m_Points
end

--- Replaces the points with an arc of a circle, angles being in degrees, counter-clockwise on screen
--- from the right. A negative sweep goes clockwise.
---@param centerX number
---@param centerY number
---@param radius number
---@param startAngle number
---@param sweep number
---@param segments number|nil 32 by default
function DPolyline:SetArc(centerX, centerY, radius, startAngle, sweep, segments)
	segments = math.max(1, math.floor(segments or 32))

	local points = {}

	for i = 0, segments do
		local angle = math.rad(startAngle + sweep * i / segments)
		points[#points + 1] = { x = centerX + radius * math.cos(angle), y = centerY - radius * math.sin(angle) }
	end

	self:SetPoints(points)
end

---@param thickness number
function DPolyline:SetThickness(thickness)
	self.m_Thickness = thickness
	self:InvalidatePaint()
end

---@return number
function DPolyline:GetThickness()
	return self.m_Thickness
end

---@param color Vector4
function DPolyline:SetColor(color)
	self.m_Color = color

	for i = 1, #self.m_Segments do
		self.m_Segments[i].m_Image:SetTint(color)
	end
end

---@return Vector4
function DPolyline:GetColor()
	return self.m_Color
end

--- Defines the texture of every segment, stretched along it
---@param texture Texture|nil
function DPolyline:SetTexture(texture)
	self.m_Texture = texture

	for i = 1, #self.m_Segments do
		self.m_Segments[i].m_Image:SetTexture(texture)
	end
end

--- Connects the last point to the first one
---@param closed boolean
function DPolyline:SetClosed(closed)
	self.m_Closed = closed and true or false
	self:InvalidatePaint()
end

--- Draws only the given fraction of the line length, from its first point
---@param fraction number between 0 and 1
function DPolyline:SetFraction(fraction)
	self.m_Fraction = math.min(math.max(fraction, 0), 1)
	self:InvalidatePaint()
end

---@return number
function DPolyline:GetFraction()
	return self.m_Fraction
end

--- Returns the length of the line, including the closing segment
---@return number
function DPolyline:GetLength()
	local points, length = self.m_Points, 0

	for i = 2, #points do
		length = length + Distance(points[i - 1], points[i])
	end

	if self.m_Closed and #points > 2 then
		length = length + Distance(points[#points], points[1])
	end

	return length
end

--- Rebuilds the segments from the points
function DPolyline:Paint(width, height)
	local points = self.m_Points
	local count = #points
	local segmentCount = self.m_Closed and count > 2 and count or count - 1
	local remaining = self:GetLength() * self.m_Fraction
	local used = 0

	for i = 1, math.max(segmentCount, 0) do
		if remaining <= 0 then
			break
		end

		local a, b = points[i], points[i % count + 1]
		local length = Distance(a, b)
		local thickness = ((a.w or self.m_Thickness) + (b.w or self.m_Thickness)) / 2

		if length > remaining then
			local t = remaining / length
			b = { x = a.x + (b.x - a.x) * t, y = a.y + (b.y - a.y) * t }
		end

		-- Every segment overlaps the previous joint, except the first one unless a full closed line joins it
		local extend = (i > 1 or (self.m_Closed and self.m_Fraction >= 1)) and thickness / 2 or 0

		used = used + 1
		PlaceSegment(self, used, a, b, thickness, extend)
		remaining = remaining - length
	end

	for i = used + 1, #self.m_Segments do
		self.m_Segments[i]:SetVisible(false)
	end
end

return Registry.Register("DPolyline", DPolyline, "Panel")
