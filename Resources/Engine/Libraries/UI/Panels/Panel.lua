local Array = Resources.GetScript(":Libraries/UI/Core/Array.lua")
local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")

--- Base class of every panel. A panel owns an actor under the controller canvas; its geometry is
--- expressed in canvas units, relative to the top-left corner of its parent, with Y going down.
---@class Panel
---@field ClassName string
---@field BaseClass table|nil
local Panel = {}

-- The engine reserves a zero size axis for the intrinsic size of the element (CTransform::SetUISize)
local kMinimumAxisSize = 0.001

-- A removed panel keeps its fields, but its actor and the components cached by its classes are
-- destroyed: its methods raise an error instead of reaching them, except IsValid and Remove
local RemovedPanel = {}

RemovedPanel.__index = function(panel, key)
	local class = rawget(panel, "m_Class")
	local value = class[key]

	if type(value) ~= "function" or key == "IsValid" or key == "Remove" then
		return value
	end

	return function()
		error("UI: attempted to call " .. tostring(key) .. " on a removed " .. tostring(class.ClassName) .. " panel", 2)
	end
end

local function AssertAlive(panel)
	if panel.m_Removed then
		error("UI: attempted to use a removed " .. tostring(panel.ClassName) .. " panel", 3)
	end
end

-- The engine places the pivot of the element: it is the transform origin of the panel, around which
-- it rotates and scales. The engine Y axis goes up.
local function ApplyPosition(panel)
	panel.m_Transform:SetUIPosition(Vector2.new(
		panel.m_X + panel.m_OriginX * panel.m_Width,
		-(panel.m_Y + panel.m_OriginY * panel.m_Height)
	))
end

local function ApplySize(panel)
	panel.m_Transform:SetUISize(Vector2.new(
		math.max(panel.m_Width, kMinimumAxisSize),
		math.max(panel.m_Height, kMinimumAxisSize)
	))
end

local function ApplyActive(panel)
	panel.m_Actor:SetActive(panel.m_Visible and not panel.m_Culled and not panel.m_WorldHidden)
end

local function HasTransform(panel)
	return panel.m_Rotation ~= 0 or panel.m_ScaleX ~= 1 or panel.m_ScaleY ~= 1
end

local function Detach(panel)
	local parent = panel.m_Parent

	if parent then
		Array.RemoveValue(parent.m_Children, panel)
		parent:InvalidateLayout()
		panel.m_Parent = nil
	end
end

-- Re-attaching every child in order is the only way to reorder siblings, since actors are drawn in
-- the order of their parent children list
local function ReattachChildren(panel)
	Array.StableSortBy(panel.m_Children, function(child) return child.m_ZPos end)

	for i = 1, #panel.m_Children do
		panel.m_Children[i].m_Actor:SetParent(panel.m_Actor)
	end
end

local function AttachTo(panel, parent)
	Detach(panel)

	local children = parent.m_Children
	local last = children[#children]

	panel.m_Parent = parent
	Array.Append(children, panel)
	panel.m_Actor:SetParent(parent.m_Actor)

	if last and last.m_ZPos > panel.m_ZPos then
		ReattachChildren(parent)
	end

	parent:InvalidateLayout()
end

-- Marking the whole subtree first makes Remove calls made from the callbacks return immediately
local function MarkRemoving(panel)
	panel.m_Removing = true

	for i = 1, #panel.m_Children do
		MarkRemoving(panel.m_Children[i])
	end
end

local function NotifyRemove(panel)
	local children = Array.Copy(panel.m_Children)

	for i = 1, #children do
		NotifyRemove(children[i])
	end

	panel:OnRemove()
end

local function Invalidate(panel)
	for i = 1, #panel.m_Children do
		Invalidate(panel.m_Children[i])
	end

	panel.m_Removed = true
	panel.m_Actor = nil
	panel.m_Transform = nil
	panel.m_Class = getmetatable(panel)
	setmetatable(panel, RemovedPanel)
end

--- Creates the actor of the panel and attaches it. Called by the registry before Init.
---@package
---@param controller UIController
---@param parent Panel|nil
function Panel:Setup(controller, parent)
	self.m_Controller = controller
	self.m_Parent = nil
	self.m_Children = {}
	self.m_X, self.m_Y, self.m_Width, self.m_Height = 0, 0, 0, 0
	self.m_ZPos = 0
	self.m_Rotation = 0
	self.m_ScaleX, self.m_ScaleY = 1, 1
	self.m_OriginX, self.m_OriginY = 0, 0
	self.m_Visible = true
	self.m_Culled = false
	self.m_WorldHidden = false
	self.m_Enabled = true
	self.m_MouseInputEnabled = true
	self.m_KeyboardInputEnabled = false
	self.m_LayoutDirty = true
	self.m_PaintDirty = true
	self.m_Removing = false
	self.m_Removed = false

	parent = parent or controller:GetRoot()

	-- Checked before creating the actor, so a failure doesn't leave an orphan actor in the scene
	if parent then
		AssertAlive(parent)
	end

	local actor = Scenes.GetCurrentScene():CreateActor(self.ClassName, "")
	local transform = actor:GetTransform()

	transform:EnableUIData()
	transform:SetUIAnchorPreset(AnchorPreset.TOP_LEFT)
	transform:SetUIPivot(Vector2.new(-1, -1))

	self.m_Actor = actor
	self.m_Transform = transform

	ApplyPosition(self)
	ApplySize(self)

	if parent then
		AttachTo(self, parent)
	else
		actor:SetParent(controller:GetCanvasActor())
	end
end

--- Hides the panel without changing its visibility, used to cull content outside a scroll view
---@package
---@param culled boolean
function Panel:SetCulled(culled)
	culled = culled and true or false

	if self.m_Culled ~= culled then
		self.m_Culled = culled
		ApplyActive(self)
	end
end

--- Hides the panel without changing its visibility, used by panels placed in the 3D world when
--- their anchor isn't visible
---@package
---@param hidden boolean
function Panel:SetWorldHidden(hidden)
	hidden = hidden and true or false

	if self.m_WorldHidden ~= hidden then
		self.m_WorldHidden = hidden
		ApplyActive(self)

		if hidden then
			self.m_Controller:ReleasePanel(self)
		end
	end
end

--- Moves the panel without invalidating the layout of its parent, for positions driven every frame
---@package
---@param x number
---@param y number
function Panel:SetDrivenPos(x, y)
	if self.m_X ~= x or self.m_Y ~= y then
		self.m_X, self.m_Y = x, y
		ApplyPosition(self)
	end
end

--- Applies the Z positions of the children, set beforehand, in a single pass
---@package
function Panel:ApplyChildrenZPos()
	ReattachChildren(self)
end

--- Creates a panel of the given class as a child of this panel
---@param className string
---@return Panel
function Panel:Add(className)
	return Registry.Create(self.m_Controller, className, self)
end

---@param x number
---@param y number
function Panel:SetPos(x, y)
	assert(type(x) == "number" and type(y) == "number", "UI: SetPos expects two numbers")

	if self.m_X == x and self.m_Y == y then
		return
	end

	self.m_X, self.m_Y = x, y
	ApplyPosition(self)

	if self.m_Parent then
		self.m_Parent:InvalidateLayout()
	end
end

---@return number, number
function Panel:GetPos()
	return self.m_X, self.m_Y
end

---@param width number
---@param height number
function Panel:SetSize(width, height)
	assert(type(width) == "number" and type(height) == "number", "UI: SetSize expects two numbers")

	width, height = math.max(width, 0), math.max(height, 0)

	if self.m_Width == width and self.m_Height == height then
		return
	end

	self.m_Width, self.m_Height = width, height
	ApplySize(self)

	if self.m_OriginX ~= 0 or self.m_OriginY ~= 0 then
		ApplyPosition(self)
	end

	self:InvalidateLayout()
	self:InvalidatePaint()

	if self.m_Parent then
		self.m_Parent:InvalidateLayout()
	end
end

---@return number, number
function Panel:GetSize()
	return self.m_Width, self.m_Height
end

---@param width number
function Panel:SetWide(width)
	self:SetSize(width, self.m_Height)
end

---@param height number
function Panel:SetTall(height)
	self:SetSize(self.m_Width, height)
end

---@return number
function Panel:GetWide()
	return self.m_Width
end

---@return number
function Panel:GetTall()
	return self.m_Height
end

--- Moves the panel under another panel, or under the controller root panel when nil
---@param parent Panel|nil
function Panel:SetParent(parent)
	parent = parent or self.m_Controller:GetRoot()
	AssertAlive(parent)

	if parent == self.m_Parent then
		return
	end

	assert(parent.m_Controller == self.m_Controller, "UI: a panel can't move to another controller")

	local ancestor = parent

	while ancestor do
		assert(ancestor ~= self, "UI: a panel can't be parented to itself or to one of its children")
		ancestor = ancestor.m_Parent
	end

	AttachTo(self, parent)
end

---@return Panel|nil
function Panel:GetParent()
	return self.m_Parent
end

--- Returns a copy of the children list, ordered from the bottom-most to the top-most panel
---@return Panel[]
function Panel:GetChildren()
	return Array.Copy(self.m_Children)
end

--- Defines the drawing order among siblings, higher values being drawn on top
---@param zPos number
function Panel:SetZPos(zPos)
	assert(type(zPos) == "number", "UI: SetZPos expects a number")

	if self.m_ZPos == zPos then
		return
	end

	self.m_ZPos = zPos

	if self.m_Parent then
		ReattachChildren(self.m_Parent)
	end
end

---@return number
function Panel:GetZPos()
	return self.m_ZPos
end

--- Rotates the panel and its children around the transform origin, counter-clockwise on screen.
--- Hit-testing and coordinate conversions follow the rotation.
---@param degrees number
function Panel:SetRotation(degrees)
	assert(type(degrees) == "number", "UI: SetRotation expects a number")

	if self.m_Rotation ~= degrees then
		self.m_Rotation = degrees
		self.m_Transform:SetUIRotation(degrees)
	end
end

---@return number
function Panel:GetRotation()
	return self.m_Rotation
end

--- Scales the panel and its children around the transform origin, without changing their size or
--- position. Hit-testing and coordinate conversions follow the scale.
---@param x number
---@param y number|nil defaults to x
function Panel:SetScale(x, y)
	y = y or x
	assert(type(x) == "number" and type(y) == "number", "UI: SetScale expects numbers")

	if self.m_ScaleX ~= x or self.m_ScaleY ~= y then
		self.m_ScaleX, self.m_ScaleY = x, y
		self.m_Transform:SetUIScale(Vector2.new(x, y))
	end
end

---@return number, number
function Panel:GetScale()
	return self.m_ScaleX, self.m_ScaleY
end

--- Defines the point the panel rotates and scales around, as fractions of its size: (0, 0) is the
--- top-left corner (default), (0.5, 0.5) the center
---@param x number
---@param y number
function Panel:SetTransformOrigin(x, y)
	assert(type(x) == "number" and type(y) == "number", "UI: SetTransformOrigin expects two numbers")

	if self.m_OriginX ~= x or self.m_OriginY ~= y then
		self.m_OriginX, self.m_OriginY = x, y
		self.m_Transform:SetUIPivot(Vector2.new(2 * x - 1, 2 * y - 1))
		ApplyPosition(self)
	end
end

---@return number, number
function Panel:GetTransformOrigin()
	return self.m_OriginX, self.m_OriginY
end

---@param visible boolean
function Panel:SetVisible(visible)
	visible = visible and true or false

	if self.m_Visible == visible then
		return
	end

	self.m_Visible = visible
	ApplyActive(self)

	if self.m_Parent then
		self.m_Parent:InvalidateLayout()
	end

	if not visible then
		self.m_Controller:ReleasePanel(self)
	end
end

---@return boolean
function Panel:IsVisible()
	return self.m_Visible
end

--- A disabled panel, and its children, don't receive mouse and keyboard inputs
---@param enabled boolean
function Panel:SetEnabled(enabled)
	enabled = enabled and true or false

	if self.m_Enabled == enabled then
		return
	end

	self.m_Enabled = enabled
	self:InvalidatePaint()

	if not enabled then
		self.m_Controller:ReleasePanel(self)
	end
end

---@return boolean
function Panel:IsEnabled()
	return self.m_Enabled
end

--- A panel without mouse input is transparent to the cursor, its children still receive it
---@param enabled boolean
function Panel:SetMouseInputEnabled(enabled)
	self.m_MouseInputEnabled = enabled and true or false
end

---@return boolean
function Panel:IsMouseInputEnabled()
	return self.m_MouseInputEnabled
end

--- Keyboard input is required to get the focus and receive key callbacks
---@param enabled boolean
function Panel:SetKeyboardInputEnabled(enabled)
	self.m_KeyboardInputEnabled = enabled and true or false

	if not self.m_KeyboardInputEnabled then
		self:KillFocus()
	end
end

---@return boolean
function Panel:IsKeyboardInputEnabled()
	return self.m_KeyboardInputEnabled
end

--- Defines the cursor shape shown while the panel is hovered, or the default cursor when nil
---@param shape CursorShape|nil
function Panel:SetCursor(shape)
	self.m_Cursor = shape
end

---@return CursorShape|nil
function Panel:GetCursor()
	return self.m_Cursor
end

---@return boolean
function Panel:IsHovered()
	return self.m_Controller:GetHoveredPanel() == self
end

---@return boolean
function Panel:HasFocus()
	return self.m_Controller:GetFocusedPanel() == self
end

function Panel:RequestFocus()
	self.m_Controller:SetFocusedPanel(self)
end

function Panel:KillFocus()
	if self:HasFocus() then
		self.m_Controller:SetFocusedPanel(nil)
	end
end

--- Schedules PerformLayout, or runs it immediately when layoutNow is true
---@param layoutNow boolean|nil
function Panel:InvalidateLayout(layoutNow)
	if layoutNow then
		self:PerformLayout(self.m_Width, self.m_Height)
		self.m_LayoutDirty = false
	else
		self.m_LayoutDirty = true
	end
end

--- Schedules Paint, which applies the visual state of the panel to its components
function Panel:InvalidatePaint()
	self.m_PaintDirty = true
end

--- Converts a position local to the panel into canvas space
---@param x number
---@param y number
---@return number, number
function Panel:LocalToScreen(x, y)
	local panel = self

	while panel do
		x, y = panel:LocalToParent(x, y)
		panel = panel.m_Parent
	end

	return x, y
end

--- Converts a position in canvas space into a position local to the panel
---@param x number
---@param y number
---@return number, number
function Panel:ScreenToLocal(x, y)
	local chain, panel = {}, self

	while panel do
		chain[#chain + 1] = panel
		panel = panel.m_Parent
	end

	for i = #chain, 1, -1 do
		x, y = chain[i]:ParentToLocal(x, y)
	end

	return x, y
end

--- Converts a position local to the panel into the space of its parent
---@param x number
---@param y number
---@return number, number
function Panel:LocalToParent(x, y)
	if not HasTransform(self) then
		return x + self.m_X, y + self.m_Y
	end

	local originX, originY = self.m_OriginX * self.m_Width, self.m_OriginY * self.m_Height
	local dx, dy = (x - originX) * self.m_ScaleX, (y - originY) * self.m_ScaleY
	local angle = math.rad(self.m_Rotation)
	local c, s = math.cos(angle), math.sin(angle)

	-- Counter-clockwise on screen, where Y goes down
	return self.m_X + originX + c * dx + s * dy, self.m_Y + originY - s * dx + c * dy
end

--- Converts a position in the space of the parent into a position local to the panel. A panel scaled
--- to zero maps every position to its transform origin.
---@param x number
---@param y number
---@return number, number
function Panel:ParentToLocal(x, y)
	if not HasTransform(self) then
		return x - self.m_X, y - self.m_Y
	end

	local originX, originY = self.m_OriginX * self.m_Width, self.m_OriginY * self.m_Height

	if self.m_ScaleX == 0 or self.m_ScaleY == 0 then
		return originX, originY
	end

	local dx, dy = x - self.m_X - originX, y - self.m_Y - originY
	local angle = math.rad(self.m_Rotation)
	local c, s = math.cos(angle), math.sin(angle)

	return originX + (c * dx - s * dy) / self.m_ScaleX, originY + (s * dx + c * dy) / self.m_ScaleY
end

--- Returns the cursor position local to the panel
---@return number, number
function Panel:LocalCursorPos()
	return self:ScreenToLocal(self.m_Controller:GetCursorPos())
end

---@return UIController
function Panel:GetController()
	return self.m_Controller
end

--- Returns the actor backing the panel. Its Transform is driven by the panel and must not be edited,
--- and the actor must not be destroyed or re-parented other than through the panel.
---@return Actor|nil
function Panel:GetActor()
	return self.m_Actor
end

---@return boolean
function Panel:IsValid()
	return not self.m_Removed
end

--- Removes the panel and its children, calling OnRemove once on each of them. Once removed, calling
--- any method other than IsValid and Remove raises an error.
function Panel:Remove()
	if self.m_Removing then
		return
	end

	MarkRemoving(self)
	self.m_Controller:ReleasePanel(self)
	NotifyRemove(self)
	Detach(self)
	self.m_Actor:Destroy()
	Invalidate(self)
end

--- Marks the panel and its children as removed without reaching their actors, which are already
--- deleted, and without calling OnRemove
---@package
function Panel:Discard()
	MarkRemoving(self)
	Invalidate(self)
end

--- Called once the panel is created, from the base class to the most derived one
function Panel:Init() end

--- Called every frame while the panel is visible, from controller:Update
---@param deltaTime number
function Panel:Think(deltaTime) end

--- Called when the visual state changed (size, hover, pressed, enabled...), to update the components
---@param width number
---@param height number
function Panel:Paint(width, height) end

--- Called after InvalidateLayout, before Paint, to position the children
---@param width number
---@param height number
function Panel:PerformLayout(width, height) end

---@param button MouseButton
function Panel:OnMousePressed(button) end

---@param button MouseButton
function Panel:OnMouseReleased(button) end

--- Returns true to stop the event, otherwise it is sent to the parent
---@param delta number
---@return boolean|nil
function Panel:OnMouseWheeled(delta) end

--- Called while the panel is hovered or captures the mouse
---@param x number
---@param y number
function Panel:OnCursorMoved(x, y) end

---@param key Key
function Panel:OnKeyCodePressed(key) end

---@param key Key
function Panel:OnKeyCodeReleased(key) end

function Panel:OnCursorEntered() end

function Panel:OnCursorExited() end

---@param gained boolean
function Panel:OnFocusChanged(gained) end

function Panel:OnRemove() end

return Registry.Register("Panel", Panel)
