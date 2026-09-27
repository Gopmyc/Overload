--- Projection of 3D world positions onto the viewport, mirroring the camera matrices of the engine
--- (Camera::CalculateViewMatrix, FMatrix4::CreatePerspective and CreateOrthographic), which aren't
--- exposed to Lua (see Docs/LuaExposureGaps.md). Also holds the anchor shared by the world panels.
---@class UIWorldSpace
local WorldSpace = {}

---@class UIWorldView
---@field width number viewport width in pixels
---@field height number viewport height in pixels

--- Captures the camera state for the current frame, or returns nil when the actor is destroyed or
--- has no Camera
---@param cameraActor Actor
---@param viewportWidth number
---@param viewportHeight number
---@return UIWorldView|nil
function WorldSpace.CreateView(cameraActor, viewportWidth, viewportHeight)
	if not cameraActor:IsAlive() then
		return nil
	end

	local camera = cameraActor:GetCamera()

	if not camera then
		return nil
	end

	local transform = cameraActor:GetTransform()
	local position, forward, up = transform:GetWorldPosition(), transform:GetWorldForward(), transform:GetWorldUp()

	-- FMatrix4::CreateView uses cross(forward, up) as the screen right, the opposite of the transform right
	local rightX = forward.y * up.z - forward.z * up.y
	local rightY = forward.z * up.x - forward.x * up.z
	local rightZ = forward.x * up.y - forward.y * up.x
	local rightLength = math.sqrt(rightX * rightX + rightY * rightY + rightZ * rightZ)

	if rightLength == 0 then
		return nil
	end

	return {
		x = position.x, y = position.y, z = position.z,
		forwardX = forward.x, forwardY = forward.y, forwardZ = forward.z,
		upX = up.x, upY = up.y, upZ = up.z,
		rightX = rightX / rightLength, rightY = rightY / rightLength, rightZ = rightZ / rightLength,
		near = camera:GetNear(),
		perspective = camera:GetProjectionMode() == ProjectionMode.PERSPECTIVE,
		tangent = math.tan(math.rad(camera:GetFov()) / 2),
		size = camera:GetSize(),
		width = viewportWidth,
		height = viewportHeight,
		aspect = viewportWidth / viewportHeight
	}
end

--- Converts a world position into camera space: x to the right, y up, z the depth in front of the camera
---@param view UIWorldView
---@return number, number, number
function WorldSpace.ToCamera(view, x, y, z)
	local dx, dy, dz = x - view.x, y - view.y, z - view.z

	return
		dx * view.rightX + dy * view.rightY + dz * view.rightZ,
		dx * view.upX + dy * view.upY + dz * view.upZ,
		dx * view.forwardX + dy * view.forwardY + dz * view.forwardZ
end

--- Converts a camera space position in front of the camera into viewport pixels (top-left origin)
---@param view UIWorldView
---@return number, number
function WorldSpace.CameraToViewport(view, x, y, z)
	local halfHeight = view.perspective and z * view.tangent or view.size
	local halfWidth = halfHeight * view.aspect

	return (0.5 + 0.5 * x / halfWidth) * view.width, (0.5 - 0.5 * y / halfHeight) * view.height
end

--- Projects a world position into viewport pixels, and returns its depth, or nil when it isn't in
--- front of the near plane
---@param view UIWorldView
---@return number|nil, number|nil, number|nil
function WorldSpace.Project(view, x, y, z)
	local cameraX, cameraY, depth = WorldSpace.ToCamera(view, x, y, z)

	if depth <= view.near then
		return nil
	end

	local viewportX, viewportY = WorldSpace.CameraToViewport(view, cameraX, cameraY, depth)
	return viewportX, viewportY, depth
end

--- Returns the part [t0, t1] of a segment whose depth goes from depthA to depthB that lies between
--- minDepth and maxDepth (no upper bound when nil), or nil when none of it does
---@return number|nil, number|nil
function WorldSpace.ClipDepth(depthA, depthB, minDepth, maxDepth)
	local t0, t1 = 0, 1

	if depthA < minDepth and depthB < minDepth then
		return nil
	elseif depthA < minDepth then
		t0 = (minDepth - depthA) / (depthB - depthA)
	elseif depthB < minDepth then
		t1 = (minDepth - depthA) / (depthB - depthA)
	end

	if maxDepth then
		if depthA > maxDepth and depthB > maxDepth then
			return nil
		elseif depthA > maxDepth then
			t0 = math.max(t0, (maxDepth - depthA) / (depthB - depthA))
		elseif depthB > maxDepth then
			t1 = math.min(t1, (maxDepth - depthA) / (depthB - depthA))
		end
	end

	if t0 > t1 then
		return nil
	end

	return t0, t1
end

--- Returns the number of viewport pixels covered by one world unit at the given depth
---@param view UIWorldView
---@param depth number
---@return number
function WorldSpace.PixelsPerWorldUnit(view, depth)
	local halfHeight = view.perspective and math.max(depth, view.near) * view.tangent or view.size
	return view.height / (2 * halfHeight)
end

--- Keeps a camera space position inside the viewport shrunk by the given margins, in pixels. Returns
--- the viewport position, the angle from the viewport center towards the position (degrees,
--- counter-clockwise on screen, 0 pointing right) and whether the position had to be moved.
---@param view UIWorldView
---@return number, number, number, boolean
function WorldSpace.ClampToEdges(view, x, y, depth, left, top, right, bottom)
	local centerX, centerY = view.width / 2, view.height / 2
	local minimumX, maximumX = math.min(left, centerX), math.max(view.width - right, centerX)
	local minimumY, maximumY = math.min(top, centerY), math.max(view.height - bottom, centerY)
	local directionX, directionY

	if depth > view.near then
		local viewportX, viewportY = WorldSpace.CameraToViewport(view, x, y, depth)
		directionX, directionY = viewportX - centerX, viewportY - centerY

		if viewportX >= minimumX and viewportX <= maximumX and viewportY >= minimumY and viewportY <= maximumY then
			return viewportX, viewportY, math.deg(math.atan(-directionY, directionX)), false
		end
	else
		-- The projection flips behind the camera, but the camera space direction stays meaningful
		directionX, directionY = x, -y

		if directionX == 0 and directionY == 0 then
			directionY = 1
		end
	end

	-- Moves from the center along the direction until an edge of the area is reached
	local scale = math.huge

	if directionX ~= 0 then
		scale = math.min(scale, ((directionX > 0 and maximumX or minimumX) - centerX) / directionX)
	end

	if directionY ~= 0 then
		scale = math.min(scale, ((directionY > 0 and maximumY or minimumY) - centerY) / directionY)
	end

	return centerX + directionX * scale, centerY + directionY * scale, math.deg(math.atan(-directionY, directionX)), true
end

-- Anchor of the world panels: a fixed world position or a target actor, plus an offset. In "target"
-- space, offsets and points follow the axes of the target (right, up, forward), without its scale.

local kWorldAxes = { 1, 0, 0, 0, 1, 0, 0, 0, 1 }

---@param panel Panel
function WorldSpace.InitAnchor(panel)
	panel.m_Target = nil
	panel.m_TargetTransform = nil
	panel.m_WorldPosition = nil
	panel.m_Offset = { x = 0, y = 0, z = 0 }
	panel.m_Space = "world"
	panel.m_Anchor = { x = 0, y = 0, z = 0, axes = kWorldAxes }
end

--- Resolves the anchor for the current frame, or returns nil when there is none. Calls OnTargetLost
--- once when the target actor is destroyed.
---@param panel Panel
---@return table|nil
function WorldSpace.ResolveAnchor(panel)
	local anchor = panel.m_Anchor
	local x, y, z

	if panel.m_Target then
		if not panel.m_Target:IsAlive() then
			panel.m_Target, panel.m_TargetTransform = nil, nil
			panel:OnTargetLost()
			return nil
		end

		local transform = panel.m_TargetTransform
		local position = transform:GetWorldPosition()
		x, y, z = position.x, position.y, position.z

		if panel.m_Space == "target" then
			local right, up, forward = transform:GetWorldRight(), transform:GetWorldUp(), transform:GetWorldForward()
			local axes = anchor.targetAxes or {}
			axes[1], axes[2], axes[3] = right.x, right.y, right.z
			axes[4], axes[5], axes[6] = up.x, up.y, up.z
			axes[7], axes[8], axes[9] = forward.x, forward.y, forward.z
			anchor.targetAxes = axes
			anchor.axes = axes
		else
			anchor.axes = kWorldAxes
		end
	elseif panel.m_WorldPosition then
		x, y, z = panel.m_WorldPosition.x, panel.m_WorldPosition.y, panel.m_WorldPosition.z
		anchor.axes = kWorldAxes
	else
		return nil
	end

	local offset = panel.m_Offset
	anchor.x, anchor.y, anchor.z = x, y, z
	anchor.x, anchor.y, anchor.z = WorldSpace.AnchorToWorld(anchor, offset.x, offset.y, offset.z)

	return anchor
end

--- Converts a position expressed in the anchor space into a world position
---@param anchor table
---@return number, number, number
function WorldSpace.AnchorToWorld(anchor, x, y, z)
	local axes = anchor.axes

	return
		anchor.x + x * axes[1] + y * axes[4] + z * axes[7],
		anchor.y + x * axes[2] + y * axes[5] + z * axes[8],
		anchor.z + x * axes[3] + y * axes[6] + z * axes[9]
end

--- Methods shared by the world panels, copied into their classes
WorldSpace.AnchorMethods = {}

--- Attaches the panel to an actor. The offset is expressed in world axes, or in the axes of the
--- target in "target" space.
---@param actor Actor
---@param offset Vector3|nil
function WorldSpace.AnchorMethods:SetTarget(actor, offset)
	assert(actor and actor:IsAlive(), "UI: SetTarget expects a living actor")

	self.m_Target = actor
	self.m_TargetTransform = actor:GetTransform()
	self.m_WorldPosition = nil

	if offset then
		self:SetOffset(offset)
	end
end

---@return Actor|nil
function WorldSpace.AnchorMethods:GetTarget()
	return self.m_Target
end

--- Attaches the panel to a fixed world position
---@param position Vector3
function WorldSpace.AnchorMethods:SetWorldPosition(position)
	self.m_WorldPosition = { x = position.x, y = position.y, z = position.z }
	self.m_Target, self.m_TargetTransform = nil, nil
end

---@param offset Vector3
function WorldSpace.AnchorMethods:SetOffset(offset)
	self.m_Offset = { x = offset.x, y = offset.y, z = offset.z }
end

--- "world" (default): offsets and points use the world axes. "target": they follow the rotation of
--- the target actor.
---@param space string
function WorldSpace.AnchorMethods:SetSpace(space)
	assert(space == "world" or space == "target", "UI: the space must be \"world\" or \"target\"")
	self.m_Space = space
end

---@return string
function WorldSpace.AnchorMethods:GetSpace()
	return self.m_Space
end

--- Returns the depth of the anchor in front of the camera during the last update, or nil when it
--- wasn't in front of the camera
---@return number|nil
function WorldSpace.AnchorMethods:GetDepth()
	return self.m_Depth
end

--- Called once when the target actor is destroyed. Removes the panel by default.
function WorldSpace.AnchorMethods:OnTargetLost()
	self:Remove()
end

return WorldSpace
