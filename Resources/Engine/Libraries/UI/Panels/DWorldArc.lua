local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")

Resources.GetScript(":Libraries/UI/Panels/DWorldPath.lua")

--- Arc of a circle around the anchor, such as a curved health bar around a character or a selection
--- ring on the ground. Combine SetFraction and SetBackgroundColor for a bar.
---@class DWorldArc : DWorldPath
local DWorldArc = {}

local function Normalize(x, y, z)
	local length = math.sqrt(x * x + y * y + z * z)

	if length < 1e-6 then
		return nil
	end

	return x / length, y / length, z / length
end

-- Removes the component of the vector along the unit axis, then normalizes it
local function ProjectOnPlane(x, y, z, axisX, axisY, axisZ)
	local dot = x * axisX + y * axisY + z * axisZ
	return Normalize(x - axisX * dot, y - axisY * dot, z - axisZ * dot)
end

function DWorldArc:Init()
	self.m_Radius = 1
	self.m_Axis = { x = 0, y = 1, z = 0 }
	self.m_CenterAngle = 0
	self.m_Sweep = 360
	self.m_SegmentCount = 32
	self.m_FaceCamera = false
end

---@param radius number world units
function DWorldArc:SetRadius(radius)
	self.m_Radius = radius
end

---@return number
function DWorldArc:GetRadius()
	return self.m_Radius
end

--- Defines the axis the arc turns around, in the space of the anchor (up by default)
---@param axis Vector3
function DWorldArc:SetAxis(axis)
	self.m_Axis = { x = axis.x, y = axis.y, z = axis.z }
end

--- Defines the arc, in degrees, as its center angle and its length. Angles start from the reference
--- direction (see SetFaceCamera) and turn towards cross(axis, reference). The fraction fills the arc
--- from centerAngle - sweep / 2, so a negative sweep fills it the other way.
---@param centerAngle number
---@param sweep number 360 for a full circle
function DWorldArc:SetArc(centerAngle, sweep)
	self.m_CenterAngle, self.m_Sweep = centerAngle, sweep
end

---@return number, number
function DWorldArc:GetArc()
	return self.m_CenterAngle, self.m_Sweep
end

---@param count number
function DWorldArc:SetSegments(count)
	self.m_SegmentCount = math.max(1, math.floor(count))
end

--- The reference direction is towards the camera when true, so the arc keeps facing the viewer
--- around the axis. Otherwise it is the forward axis of the anchor space.
---@param faceCamera boolean
function DWorldArc:SetFaceCamera(faceCamera)
	self.m_FaceCamera = faceCamera and true or false
end

---@param view UIWorldView
---@param anchor table
function DWorldArc:BuildWorldPoints(view, anchor)
	local axes = anchor.axes
	local axis = self.m_Axis
	local axisX, axisY, axisZ = Normalize(
		axis.x * axes[1] + axis.y * axes[4] + axis.z * axes[7],
		axis.x * axes[2] + axis.y * axes[5] + axis.z * axes[8],
		axis.x * axes[3] + axis.y * axes[6] + axis.z * axes[9]
	)

	if not axisX then
		return {}
	end

	local referenceX, referenceY, referenceZ

	if self.m_FaceCamera then
		referenceX, referenceY, referenceZ = ProjectOnPlane(view.x - anchor.x, view.y - anchor.y, view.z - anchor.z, axisX, axisY, axisZ)
	end

	-- Falls back on the forward, then the right axis of the anchor space when parallel to the axis
	if not referenceX then
		referenceX, referenceY, referenceZ = ProjectOnPlane(axes[7], axes[8], axes[9], axisX, axisY, axisZ)
	end

	if not referenceX then
		referenceX, referenceY, referenceZ = ProjectOnPlane(axes[1], axes[2], axes[3], axisX, axisY, axisZ)
	end

	local sideX = axisY * referenceZ - axisZ * referenceY
	local sideY = axisZ * referenceX - axisX * referenceZ
	local sideZ = axisX * referenceY - axisY * referenceX

	local points = {}
	local count = self.m_SegmentCount
	local startAngle = self.m_CenterAngle - self.m_Sweep / 2

	for i = 0, count do
		local angle = math.rad(startAngle + self.m_Sweep * i / count)
		local c, s = math.cos(angle) * self.m_Radius, math.sin(angle) * self.m_Radius

		points[#points + 1] = {
			x = anchor.x + referenceX * c + sideX * s,
			y = anchor.y + referenceY * c + sideY * s,
			z = anchor.z + referenceZ * c + sideZ * s
		}
	end

	return points
end

return Registry.Register("DWorldArc", DWorldArc, "DWorldPath")
