local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")

Resources.GetScript(":Libraries/UI/Panels/Panel.lua")

--- Carries a 3D model on a world space canvas: plates, keys, slots or items standing on the canvas.
--- Once the layout is final, every frame, the model is centred on the panel, turned like the canvas
--- (and like the panel on it) then by its own rotation, pushed along the canvas normal by its depth,
--- and shown only while the panel is. The panel draws nothing itself and doesn't take the mouse; it
--- owns its model, which is destroyed with the panel. On a screen space canvas the model stays hidden.
---@class DModelPanel : Panel
local DModelPanel = {}

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
	self:SetMouseInputEnabled(false)
end

--- Gives the panel the model to carry, destroying the previous one. The panel owns the actor from
--- then on: it moves it, shows and hides it, and destroys it when removed.
---@param actor Actor|nil
function DModelPanel:SetModel(actor)
	if self.m_Model == actor then
		return
	end

	if self.m_Model and self.m_Model:IsAlive() then
		self.m_Model:Destroy()
	end

	self.m_Model = actor
	self.m_ModelActive = nil
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

--- Places the model on the panel, called by the controller once the layout is final
---@package
function DModelPanel:PlaceInWorld()
	local model = self.m_Model

	if not model then
		return
	end

	if not model:IsAlive() then
		self.m_Model = nil
		return
	end

	local rect = IsShown(self) and self:GetWorldRect() or nil

	if not rect then
		SetModelActive(self, false)
		return
	end

	local frame = self.m_Controller:GetCanvasFrame()
	local transform = model:GetTransform()
	local scale = self.m_ModelScale
	local spin = Quaternion.new(Vector3.new(0, 0, GetCanvasRotation(self)))

	if self.m_FillRect then
		scale = Vector3.new(scale.x * rect.width, scale.y * rect.height, scale.z)
	end

	transform:SetWorldPosition(rect.position + rect.normal * self.m_Depth)
	transform:SetWorldRotation(frame.rotation * spin * self.m_ModelRotation)
	transform:SetLocalScale(scale)
	SetModelActive(self, true)
end

function DModelPanel:OnRemove()
	self:SetModel(nil)
end

return Registry.Register("DModelPanel", DModelPanel, "Panel")
