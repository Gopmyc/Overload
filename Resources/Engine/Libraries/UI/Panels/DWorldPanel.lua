local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")
local WorldSpace = Resources.GetScript(":Libraries/UI/Core/WorldSpace.lua")

Resources.GetScript(":Libraries/UI/Panels/Panel.lua")

--- Container following a position of the 3D world or an actor: name tags, markers, flat health bars.
--- It is placed on the canvas every frame and drawn over the scene (see Docs/WorldSpace.md). Its
--- children receive the mouse as usual, the container itself doesn't.
---@class DWorldPanel : Panel
local DWorldPanel = {}

for name, method in pairs(WorldSpace.AnchorMethods) do
	DWorldPanel[name] = method
end

-- Stops the occlusion ray just before the anchor, so the surface the anchor lies on doesn't hide it
local kOcclusionReach = 0.99

local function IsOccluded(self, view, anchor)
	local x, y, z = anchor.x - view.x, anchor.y - view.y, anchor.z - view.z
	local distance = math.sqrt(x * x + y * y + z * z)

	if distance == 0 then
		return false
	end

	local hit = Physics.Raycast(
		Vector3.new(view.x, view.y, view.z),
		Vector3.new(x / distance, y / distance, z / distance),
		distance * kOcclusionReach
	)

	if not hit then
		return false
	end

	local owner = hit.FirstResultObject:GetOwner()
	local target = self.m_Target

	return not (target and (owner == target or owner:IsDescendantOf(target)))
end

function DWorldPanel:Init()
	WorldSpace.InitAnchor(self)
	self:SetMouseInputEnabled(false)
	self:SetAlignment(0.5, 1)

	self.m_DistanceScale = nil
	self.m_MaxDistance = nil
	self.m_ClampMargin = nil
	self.m_Clamped = false
	self.m_EdgeAngle = 0
	self.m_OcclusionCheck = false
	self.m_OcclusionInterval = 0.1
	self.m_OcclusionTimer = 0
	self.m_Occluded = false
	self.m_Depth = nil

	-- Hidden until placed by the first update
	self:SetWorldHidden(true)
end

--- Defines the point of the panel placed on the anchor, as fractions of its size: (0.5, 1), the
--- default, is the middle of the bottom edge. The panel scales around that point.
---@param x number
---@param y number
function DWorldPanel:SetAlignment(x, y)
	self.m_AlignX, self.m_AlignY = x, y
	self:SetTransformOrigin(x, y)
end

---@return number, number
function DWorldPanel:GetAlignment()
	return self.m_AlignX, self.m_AlignY
end

--- Scales the panel by referenceDistance / depth, between minimum and maximum, so it looks attached
--- to the world. Disabled when nil, which restores a scale of 1. Perspective cameras only.
---@param referenceDistance number|nil distance at which the scale is 1
---@param minimum number|nil
---@param maximum number|nil
function DWorldPanel:SetDistanceScale(referenceDistance, minimum, maximum)
	if referenceDistance then
		self.m_DistanceScale = { reference = referenceDistance, minimum = minimum or 0, maximum = maximum or math.huge }
	else
		self.m_DistanceScale = nil
		self:SetScale(1)
	end
end

--- Hides the panel when the anchor is farther from the camera than the given distance, or never when nil
---@param distance number|nil
function DWorldPanel:SetMaxDistance(distance)
	self.m_MaxDistance = distance
end

--- Keeps the whole panel on the canvas, margin canvas units away from the edges, when its anchor is
--- off-screen or behind the camera, like a waypoint indicator. Disabled when nil.
---@param margin number|nil
function DWorldPanel:SetClampToScreen(margin)
	self.m_ClampMargin = margin
end

--- Returns true when the panel is held on a canvas edge by SetClampToScreen
---@return boolean
function DWorldPanel:IsClamped()
	return self.m_Clamped
end

--- Returns the direction of the anchor from the canvas center, in degrees counter-clockwise from the
--- right, to rotate an arrow with SetRotation. Updated while SetClampToScreen is enabled.
---@return number
function DWorldPanel:GetEdgeAngle()
	return self.m_EdgeAngle
end

--- Hides the panel when a collider lies between the camera and the anchor, other than the target and
--- its children. A ray is cast every interval seconds (0.1 by default).
---@param enabled boolean
---@param interval number|nil
function DWorldPanel:SetOcclusionCheck(enabled, interval)
	self.m_OcclusionCheck = enabled and true or false
	self.m_OcclusionInterval = interval or 0.1
	self.m_OcclusionTimer = 0
	self.m_Occluded = false
end

---@return boolean
function DWorldPanel:IsOccluded()
	return self.m_Occluded
end

--- Places the panel on the canvas, called by the controller before the mouse is processed
---@package
---@param view UIWorldView|nil
---@param deltaTime number
function DWorldPanel:UpdateWorld(view, deltaTime)
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

	local x, y, depth = WorldSpace.ToCamera(view, anchor.x, anchor.y, anchor.z)
	local inFront = depth > view.near
	self.m_Depth = inFront and depth or nil

	if self.m_MaxDistance and x * x + y * y + depth * depth > self.m_MaxDistance * self.m_MaxDistance then
		self:SetWorldHidden(true)
		return
	end

	local viewportX, viewportY

	if self.m_ClampMargin then
		-- Margins around the alignment point, so the scaled panel stays entirely inside
		local pixels = self.m_Controller:GetCanvasScale()
		local width, height = self.m_Width * self.m_ScaleX * pixels, self.m_Height * self.m_ScaleY * pixels
		local margin = self.m_ClampMargin * pixels

		viewportX, viewportY, self.m_EdgeAngle, self.m_Clamped = WorldSpace.ClampToEdges(
			view, x, y, depth,
			margin + self.m_AlignX * width, margin + self.m_AlignY * height,
			margin + (1 - self.m_AlignX) * width, margin + (1 - self.m_AlignY) * height
		)
	elseif inFront then
		viewportX, viewportY = WorldSpace.CameraToViewport(view, x, y, depth)
		self.m_Clamped = false
	else
		self:SetWorldHidden(true)
		return
	end

	-- A panel held on the edges shows where its anchor is, even behind a wall
	if self.m_OcclusionCheck and not self.m_Clamped then
		self.m_OcclusionTimer = self.m_OcclusionTimer - deltaTime

		if self.m_OcclusionTimer <= 0 then
			self.m_OcclusionTimer = self.m_OcclusionInterval
			self.m_Occluded = IsOccluded(self, view, anchor)
		end

		if self.m_Occluded then
			self:SetWorldHidden(true)
			return
		end
	end

	local distanceScale = self.m_DistanceScale

	if distanceScale and view.perspective and inFront then
		self:SetScale(math.min(math.max(distanceScale.reference / depth, distanceScale.minimum), distanceScale.maximum))
	end

	local canvasX, canvasY = self.m_Controller:ViewportToCanvas(viewportX, viewportY)
	local localX, localY = self.m_Parent:ScreenToLocal(canvasX, canvasY)

	self:SetDrivenPos(localX - self.m_AlignX * self.m_Width, localY - self.m_AlignY * self.m_Height)
	self:SetWorldHidden(false)
end

return Registry.Register("DWorldPanel", DWorldPanel, "Panel")
