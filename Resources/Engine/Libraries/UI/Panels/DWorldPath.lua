local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")
local WorldSpace = Resources.GetScript(":Libraries/UI/Core/WorldSpace.lua")

Resources.GetScript(":Libraries/UI/Panels/Panel.lua")
Resources.GetScript(":Libraries/UI/Panels/DPolyline.lua")

--- Line through points of the 3D world, relative to an anchor: rings, trails, curved bars. It is
--- projected every frame, cut at the near plane, and drawn over the scene with DPolyline (see
--- Docs/WorldSpace.md). It doesn't receive the mouse.
---@class DWorldPath : Panel
local DWorldPath = {}

for name, method in pairs(WorldSpace.AnchorMethods) do
	DWorldPath[name] = method
end

local kDefaultColor = Vector4.new(1, 1, 1, 1)

local function Distance(a, b)
	local dx, dy, dz = b.x - a.x, b.y - a.y, b.z - a.z
	return math.sqrt(dx * dx + dy * dy + dz * dz)
end

-- Returns the beginning of an open path covering the given fraction of its length
local function CutPath(points, fraction)
	local length = 0

	for i = 2, #points do
		length = length + Distance(points[i - 1], points[i])
	end

	local remaining = length * fraction
	local result = { points[1] }

	for i = 2, #points do
		local a, b = points[i - 1], points[i]
		local segmentLength = Distance(a, b)

		if segmentLength >= remaining then
			local t = segmentLength > 0 and remaining / segmentLength or 0
			result[#result + 1] = { x = a.x + (b.x - a.x) * t, y = a.y + (b.y - a.y) * t, z = a.z + (b.z - a.z) * t }
			return result
		end

		result[#result + 1] = b
		remaining = remaining - segmentLength

		-- The cut fell on this point, up to rounding errors
		if remaining <= 1e-9 then
			return result
		end
	end

	return result
end

-- Affine mapping from viewport pixels to coordinates local to the panel, built once per frame since
-- the conversions of the canvas and of the panel parents are affine
local function CreateViewportMapping(self)
	local controller = self.m_Controller
	local originX, originY = self:ScreenToLocal(controller:ViewportToCanvas(0, 0))
	local rightX, rightY = self:ScreenToLocal(controller:ViewportToCanvas(1, 0))
	local downX, downY = self:ScreenToLocal(controller:ViewportToCanvas(0, 1))

	return {
		x = originX, y = originY,
		xx = rightX - originX, xy = rightY - originY,
		yx = downX - originX, yy = downY - originY,
		unitsPerPixel = math.sqrt((rightX - originX) ^ 2 + (rightY - originY) ^ 2)
	}
end

-- Projects a camera space point into a DPolyline point local to the panel
local function ToLocal(self, view, mapping, x, y, depth)
	local viewportX, viewportY = WorldSpace.CameraToViewport(view, x, y, depth)
	local width = self.m_Thickness

	if self.m_WorldThickness then
		width = self.m_Thickness * WorldSpace.PixelsPerWorldUnit(view, depth) * mapping.unitsPerPixel
	end

	return {
		x = mapping.x + viewportX * mapping.xx + viewportY * mapping.yx,
		y = mapping.y + viewportX * mapping.xy + viewportY * mapping.yy,
		w = width
	}
end

-- Splits the projected path into continuous runs, cut where it goes behind the near plane or, when
-- maxDepth is set, farther than maxDepth
local function BuildRuns(self, view, mapping, points, maxDepth)
	local runs = {}
	local current = nil

	for i = 1, #points - 1 do
		local a, b = points[i], points[i + 1]
		local ax, ay, az = WorldSpace.ToCamera(view, a.x, a.y, a.z)
		local bx, by, bz = WorldSpace.ToCamera(view, b.x, b.y, b.z)
		local t0, t1 = WorldSpace.ClipDepth(az, bz, view.near, maxDepth)

		if t0 then
			if not current or t0 > 0 then
				current = { ToLocal(self, view, mapping, ax + (bx - ax) * t0, ay + (by - ay) * t0, az + (bz - az) * t0) }
				runs[#runs + 1] = current
			end

			current[#current + 1] = ToLocal(self, view, mapping, ax + (bx - ax) * t1, ay + (by - ay) * t1, az + (bz - az) * t1)

			if t1 < 1 then
				current = nil
			end
		else
			current = nil
		end
	end

	return runs
end

local function ApplyRuns(self, layer, lines, runs, color)
	for i = 1, #runs do
		local line = lines[i]

		if not line then
			line = layer:Add("DPolyline")
			line:SetColor(color)
			line:SetTexture(self.m_Texture)
			lines[i] = line
		end

		line:SetVisible(true)
		line:SetPoints(runs[i])
	end

	for i = #runs + 1, #lines do
		lines[i]:SetVisible(false)
	end
end

local function SetLinesColor(lines, color)
	for i = 1, #lines do
		lines[i]:SetColor(color)
	end
end

function DWorldPath:Init()
	WorldSpace.InitAnchor(self)
	self:SetMouseInputEnabled(false)

	self.m_Points = {}
	self.m_Closed = false
	self.m_Thickness = 4
	self.m_WorldThickness = false
	self.m_Color = kDefaultColor
	self.m_BackgroundColor = nil
	self.m_Texture = nil
	self.m_Fraction = 1
	self.m_FrontOnly = false
	self.m_Depth = nil

	-- The background is drawn below the filled part
	self.m_BackgroundLayer = self:Add("Panel")
	self.m_BackgroundLayer:SetMouseInputEnabled(false)
	self.m_FillLayer = self:Add("Panel")
	self.m_FillLayer:SetMouseInputEnabled(false)
	self.m_BackgroundLines = {}
	self.m_FillLines = {}

	-- Hidden until placed by the first update
	self:SetWorldHidden(true)
end

--- Defines the points of the path, relative to the anchor, in world axes or in the axes of the
--- target (see SetSpace)
---@param points Vector3[]
function DWorldPath:SetPoints(points)
	assert(type(points) == "table", "UI: SetPoints expects a list of points")
	self.m_Points = points
end

---@return Vector3[]
function DWorldPath:GetPoints()
	return self.m_Points
end

--- Connects the last point to the first one
---@param closed boolean
function DWorldPath:SetClosed(closed)
	self.m_Closed = closed and true or false
end

--- Defines the thickness in units of the panel (canvas units unless a parent is scaled), or in world
--- units when inWorldUnits is true, which makes the line thinner with the distance
---@param thickness number
---@param inWorldUnits boolean|nil
function DWorldPath:SetThickness(thickness, inWorldUnits)
	self.m_Thickness = thickness
	self.m_WorldThickness = inWorldUnits and true or false
end

---@return number, boolean
function DWorldPath:GetThickness()
	return self.m_Thickness, self.m_WorldThickness
end

--- Defines the color of the drawn part
---@param color Vector4
function DWorldPath:SetColor(color)
	self.m_Color = color
	SetLinesColor(self.m_FillLines, color)
end

---@return Vector4
function DWorldPath:GetColor()
	return self.m_Color
end

--- Draws the whole path below the drawn part with the given color, or nothing when nil
---@param color Vector4|nil
function DWorldPath:SetBackgroundColor(color)
	self.m_BackgroundColor = color

	if color then
		SetLinesColor(self.m_BackgroundLines, color)
	end
end

---@return Vector4|nil
function DWorldPath:GetBackgroundColor()
	return self.m_BackgroundColor
end

--- Defines the texture stretched along each segment
---@param texture Texture|nil
function DWorldPath:SetTexture(texture)
	self.m_Texture = texture

	for _, lines in ipairs({ self.m_BackgroundLines, self.m_FillLines }) do
		for i = 1, #lines do
			lines[i]:SetTexture(texture)
		end
	end
end

--- Draws only the given fraction of the path length, from its first point, measured in the world
---@param fraction number between 0 and 1
function DWorldPath:SetFraction(fraction)
	self.m_Fraction = math.min(math.max(fraction, 0), 1)
end

---@return number
function DWorldPath:GetFraction()
	return self.m_Fraction
end

--- Hides the parts of the path farther from the camera than the anchor. UI is drawn over the scene,
--- so it can't be hidden by the body of a character: this keeps only the half of a ring in front.
---@param frontOnly boolean
function DWorldPath:SetFrontOnly(frontOnly)
	self.m_FrontOnly = frontOnly and true or false
end

--- Returns the world positions of the path for this frame. Derived classes generating their points,
--- like DWorldArc, override it.
---@param view UIWorldView
---@param anchor table
---@return { x: number, y: number, z: number }[]
function DWorldPath:BuildWorldPoints(view, anchor)
	local points = {}

	for i = 1, #self.m_Points do
		local point = self.m_Points[i]
		local x, y, z = WorldSpace.AnchorToWorld(anchor, point.x, point.y, point.z)
		points[i] = { x = x, y = y, z = z }
	end

	return points
end

--- Projects the path on the canvas, called by the controller before the mouse is processed
---@package
---@param view UIWorldView|nil
---@param deltaTime number
function DWorldPath:UpdateWorld(view, deltaTime)
	local anchor = WorldSpace.ResolveAnchor(self)

	-- OnTargetLost may have removed the panel
	if not self:IsValid() then
		return
	end

	if not anchor or not view then
		self.m_Depth = nil
		self:SetWorldHidden(true)
		return
	end

	local _, _, anchorDepth = WorldSpace.ToCamera(view, anchor.x, anchor.y, anchor.z)
	self.m_Depth = anchorDepth > view.near and anchorDepth or nil

	local points = self:BuildWorldPoints(view, anchor)

	if self.m_Closed and #points > 2 then
		points[#points + 1] = points[1]
	end

	local mapping = CreateViewportMapping(self)
	local maxDepth = self.m_FrontOnly and anchorDepth or nil
	local backgroundRuns = self.m_BackgroundColor and BuildRuns(self, view, mapping, points, maxDepth) or {}
	local fillPoints = self.m_Fraction < 1 and CutPath(points, self.m_Fraction) or points
	local fillRuns = self.m_Fraction > 0 and BuildRuns(self, view, mapping, fillPoints, maxDepth) or {}

	ApplyRuns(self, self.m_BackgroundLayer, self.m_BackgroundLines, backgroundRuns, self.m_BackgroundColor)
	ApplyRuns(self, self.m_FillLayer, self.m_FillLines, fillRuns, self.m_Color)
	self:SetWorldHidden(false)
end

return Registry.Register("DWorldPath", DWorldPath, "Panel")
