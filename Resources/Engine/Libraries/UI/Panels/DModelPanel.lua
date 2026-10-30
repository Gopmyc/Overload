local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")

Resources.GetScript(":Libraries/UI/Panels/Panel.lua")

--- Carries a 3D model on a world space canvas: plates, keys, slots or items standing on the canvas.
--- The model is a child of the canvas actor: it follows the canvas without being placed again. Once the
--- layout is final, it is centred on the panel, turned like the panel on the canvas then by its own
--- rotation, pushed along the canvas normal by its depth (or along the line of sight, see
--- SetSightAligned), and shown only while the panel is. It is placed again only when something changed:
--- the panel or one of its parents, the canvas size, its own depth, rotation or scale, the camera for a
--- model on the line of sight (by more than a rounding), or the panel announcing an animation
--- (Panel:IsAnimating).
--- The panel draws nothing itself and doesn't take the mouse; it owns its model, which is destroyed with
--- the panel. On a screen space canvas the model stays hidden.
---@class DModelPanel : Panel
local DModelPanel = {}

-- Below this, the line of sight runs too flat to the canvas to follow it
local kMinSightFacing = 0.2

-- Metres: a camera nearer than this to where a model on the line of sight was placed from has not moved. A canvas the
-- camera carries keeps its place before it but for the rounding of the world positions, which this absorbs.
local kCameraTolerance = 1e-4

local function IsShown(panel)
	while panel do
		if panel.m_Removed or not panel.m_Visible or panel.m_Culled or panel.m_WorldHidden then
			return false
		end

		panel = panel.m_Parent
	end

	return true
end

-- Rotation of the panel on its canvas, counter-clockwise as seen from the front
local function GetCanvasRotation(panel)
	local angle = 0

	while panel do
		angle = angle + panel.m_Rotation
		panel = panel.m_Parent
	end

	return angle
end

-- On every renderer of the model, its children's included
local function ApplyVisibilityFlags(actor, flags)
	local renderer = actor:GetMaterialRenderer()

	if renderer then
		renderer:SetVisibilityFlags(flags)
	end

	for _, child in ipairs(actor:GetChildren()) do
		ApplyVisibilityFlags(child, flags)
	end
end

local function SetModelActive(self, active)
	if self.m_ModelActive ~= active then
		self.m_ModelActive = active
		self.m_Model:SetActive(active)
	end
end

function DModelPanel:Init()
	self.m_Model = nil
	self.m_ModelActive = nil
	self.m_Depth = 0
	self.m_ModelRotation = Quaternion.new()
	self.m_ModelScale = Vector3.new(1, 1, 1)
	self.m_FillRect = false
	self.m_SightAligned = false
	self.m_VisibilityFlags = nil
	self.m_Placed = nil
	self.m_Parented = false
	self:SetMouseInputEnabled(false)
end

--- Gives the panel the model to carry, destroying the previous one. The panel owns the actor from
--- then on: it moves it, shows and hides it, and destroys it when removed.
---@param actor Actor|nil
function DModelPanel:SetModel(actor)
	if self.m_Model == actor then
		return
	end

	-- Hidden first: a built game never collects destroyed actors, which would otherwise stay drawn
	if self.m_Model and self.m_Model:IsAlive() then
		self.m_Model:SetActive(false)
		self.m_Model:Destroy()
	end

	self.m_Model = actor
	self.m_ModelActive = nil
	self.m_Placed = nil
	self.m_Parented = false

	if actor and self.m_VisibilityFlags then
		ApplyVisibilityFlags(actor, self.m_VisibilityFlags)
	end

	-- Shown by the scene until placed, even under a hidden panel
	self:InvalidatePlacement()
end

--- Instantiates the prefab at the given path and carries it, or carries nothing when the path is nil
--- or the prefab can't be instantiated. Returns the model.
---@param path string|nil
---@return Actor|nil
function DModelPanel:SetModelPrefab(path)
	local actor = nil

	if path then
		local prefab = Prefab()
		prefab.path = path
		actor = Scenes.GetCurrentScene():InstantiatePrefab(prefab)

		if not actor then
			Debug.LogWarning("UI: DModelPanel could not instantiate the prefab " .. path)
		end
	end

	self:SetModel(actor)
	return actor
end

---@return Actor|nil
function DModelPanel:GetModel()
	return self.m_Model
end

--- Defines how far the model stands in front of the canvas, in world units along its normal.
--- Negative values sink it behind the canvas.
---@param depth number
function DModelPanel:SetDepth(depth)
	self.m_Depth = depth
end

---@return number
function DModelPanel:GetDepth()
	return self.m_Depth
end

--- Defines the rotation of the model in the frame of the panel: X to the right, Y up, Z towards the
--- viewer
---@param rotation Quaternion
function DModelPanel:SetModelRotation(rotation)
	self.m_ModelRotation = rotation
end

---@return Quaternion
function DModelPanel:GetModelRotation()
	return self.m_ModelRotation
end

--- Defines the scale of the model, uniform when a number is given
---@param scale number|Vector3
function DModelPanel:SetModelScale(scale)
	self.m_ModelScale = type(scale) == "number" and Vector3.new(scale, scale, scale) or scale
end

---@return Vector3
function DModelPanel:GetModelScale()
	return self.m_ModelScale
end

--- When true, the scale on X and Y is also multiplied by the world width and height of the panel, so
--- a model one unit wide and tall covers it
---@param fillRect boolean
function DModelPanel:SetFillRect(fillRect)
	self.m_FillRect = fillRect and true or false
end

---@return boolean
function DModelPanel:GetFillRect()
	return self.m_FillRect
end

--- When true, the model stands on the line of sight from the camera through the centre of the panel, as far
--- along it as needed to stand its depth out of the canvas: seen from the camera, it stays centred on its
--- panel however the canvas is turned. Needs a camera (controller:SetCamera).
---@param aligned boolean
function DModelPanel:SetSightAligned(aligned)
	self.m_SightAligned = aligned and true or false
end

---@return boolean
function DModelPanel:IsSightAligned()
	return self.m_SightAligned
end

--- Defines the passes the model is drawn in, on every renderer under it, for this model and the next ones: a model
--- standing over the screen, as a HUD's, is drawn with VisibilityFlags.GEOMETRY alone, so that it casts no shadow into
--- the scene and stays out of the reflection probes. Nil leaves each model its own flags.
---@param flags VisibilityFlags|integer|nil
function DModelPanel:SetVisibilityFlags(flags)
	self.m_VisibilityFlags = flags

	if self.m_Model and self.m_Model:IsAlive() and flags then
		ApplyVisibilityFlags(self.m_Model, flags)
	end
end

---@return VisibilityFlags|integer|nil
function DModelPanel:GetVisibilityFlags()
	return self.m_VisibilityFlags
end

-- The latest change of geometry of the panel or of one of its parents, which place it on the canvas (the
-- root panel's size being the canvas size)
local function GetMovedAt(panel)
	local movedAt = 0

	while panel do
		if panel.m_MovedAt > movedAt then
			movedAt = panel.m_MovedAt
		end

		panel = panel.m_Parent
	end

	return movedAt
end

-- What the last placement depended on, outside of the model's own depth, rotation and scale
local function IsPlacementCurrent(self, placed, frame)
	local controller = self.m_Controller

	if GetMovedAt(self) > placed.version
		or placed.frameScale ~= frame.scale
		or placed.actorScaleX ~= frame.actorScaleX
		or placed.actorScaleY ~= frame.actorScaleY
		or placed.actorScaleZ ~= frame.actorScaleZ
	then
		return false
	end

	if self.m_SightAligned then
		local x, y, z = controller:GetCameraOnCanvas()

		if not x or not placed.cameraX then
			return x == placed.cameraX
		end

		return math.abs(x - placed.cameraX) <= kCameraTolerance
			and math.abs(y - placed.cameraY) <= kCameraTolerance
			and math.abs(z - placed.cameraZ) <= kCameraTolerance
	end

	return true
end

-- The model's own placement values, compared one number at a time
local function IsPoseCurrent(self, placed)
	local rotation, scale = self.m_ModelRotation, self.m_ModelScale

	return placed.depth == self.m_Depth
		and placed.fillRect == self.m_FillRect
		and placed.sightAligned == self.m_SightAligned
		and placed.rotationX == rotation.x and placed.rotationY == rotation.y
		and placed.rotationZ == rotation.z and placed.rotationW == rotation.w
		and placed.scaleX == scale.x and placed.scaleY == scale.y and placed.scaleZ == scale.z
end

--- Returns true when the model would be placed again at the next placement pass: the panel or the canvas
--- changed since, or the panel is animating. A derived class can check it before preparing its placement.
---@return boolean
function DModelPanel:IsPlacementStale()
	local placed, frame = self.m_Placed, self.m_Controller:GetCanvasFrame()

	return not placed or not frame or self:IsAnimating() or not IsPlacementCurrent(self, placed, frame)
		or not IsPoseCurrent(self, placed)
end

--- Places the model on the panel, called by the controller once the layout is final, when it is stale
--- (see IsPlacementStale). Returns the canvas rect of the panel (see GetCanvasRect) when it placed its
--- model, so a derived class doesn't compute it again, nil when it had nothing to do.
---@package
---@return table|nil
function DModelPanel:PlaceInWorld()
	local model = self.m_Model

	if not model then
		return nil
	end

	if not model:IsAlive() then
		self.m_Model = nil
		self.m_Placed = nil
		return nil
	end

	local frame = IsShown(self) and self.m_Controller:GetCanvasFrame() or nil

	if not frame then
		SetModelActive(self, false)
		return nil
	end

	if not self:IsPlacementStale() then
		SetModelActive(self, true)
		return nil
	end

	local controller = self.m_Controller
	local rect = self:GetCanvasRect()
	local scale = self.m_ModelScale
	local angle = GetCanvasRotation(self)
	local rotation = self.m_ModelRotation
	local x, y, z = rect.x, rect.y, self.m_Depth

	-- Along the line of sight from the camera through the centre of the panel
	local cameraX, cameraY, cameraZ

	if self.m_SightAligned then
		cameraX, cameraY, cameraZ = controller:GetCameraOnCanvas()
	end

	if cameraX then
		local sightX, sightY, sightZ = cameraX - x, cameraY - y, cameraZ
		local length = math.sqrt(sightX * sightX + sightY * sightY + sightZ * sightZ)

		if length > 0 and sightZ / length >= kMinSightFacing then
			local along = self.m_Depth / (sightZ / length) / length
			x, y, z = x + sightX * along, y + sightY * along, sightZ * along
		end
	end

	if angle ~= 0 then
		rotation = Quaternion.new(Vector3.new(0, 0, angle)) * rotation
	end

	if self.m_FillRect then
		scale = Vector3.new(scale.x * rect.width, scale.y * rect.height, scale.z)
	end

	-- In the space of the canvas actor, which scales its children (but not the canvas)
	local scaleX, scaleY, scaleZ = frame.actorScaleX, frame.actorScaleY, frame.actorScaleZ
	local transform = model:GetTransform()

	if not self.m_Parented then
		model:SetParent(controller:GetCanvasActor())
		self.m_Parented = true
	end

	transform:SetLocalPosition(Vector3.new(x / scaleX, y / scaleY, z / scaleZ))
	transform:SetLocalRotation(rotation)
	transform:SetLocalScale(Vector3.new(scale.x / scaleX, scale.y / scaleY, scale.z / scaleZ))
	SetModelActive(self, true)

	local modelRotation, modelScale = self.m_ModelRotation, self.m_ModelScale

	self.m_Placed = {
		version = controller.m_GeometryVersion,
		frameScale = frame.scale,
		actorScaleX = scaleX, actorScaleY = scaleY, actorScaleZ = scaleZ,
		cameraX = cameraX, cameraY = cameraY, cameraZ = cameraZ,
		depth = self.m_Depth, fillRect = self.m_FillRect, sightAligned = self.m_SightAligned,
		rotationX = modelRotation.x, rotationY = modelRotation.y, rotationZ = modelRotation.z, rotationW = modelRotation.w,
		scaleX = modelScale.x, scaleY = modelScale.y, scaleZ = modelScale.z
	}

	return rect
end

function DModelPanel:OnRemove()
	self:SetModel(nil)
end

return Registry.Register("DModelPanel", DModelPanel, "Panel")
