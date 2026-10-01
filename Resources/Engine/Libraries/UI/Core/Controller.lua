local Array = Resources.GetScript(":Libraries/UI/Core/Array.lua")
local CanvasSpace = Resources.GetScript(":Libraries/UI/Core/CanvasSpace.lua")
local Keyboard = Resources.GetScript(":Libraries/UI/Core/Keyboard.lua")
local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")
local WorldSpace = Resources.GetScript(":Libraries/UI/Core/WorldSpace.lua")

-- Registers the base class, which every controller root relies on
Resources.GetScript(":Libraries/UI/Panels/Panel.lua")

--- Owns a canvas and the panels created on it. Dispatches inputs and runs the think, layout and
--- paint passes. Must be updated every frame from a Behaviour, e.g. in OnUpdate.
---@class UIController
local Controller = {}
Controller.__index = Controller

local kRepeatDelay = 0.4
local kRepeatInterval = 0.05
local kMaxLayoutPasses = 4

local function IsInSubtree(panel, root)
	while panel do
		if panel == root then
			return true
		end

		panel = panel.m_Parent
	end

	return false
end

local function IsInteractive(panel)
	while panel do
		if panel.m_Removed or not panel.m_Visible or panel.m_Culled or panel.m_WorldHidden or not panel.m_Enabled then
			return false
		end

		panel = panel.m_Parent
	end

	return true
end

-- Returns the top-most panel accepting the cursor, false when a disabled panel blocks it, or nil.
-- The position is local to the panel.
local function HitTest(panel, x, y)
	if not panel.m_Visible or panel.m_Culled or panel.m_WorldHidden then
		return nil
	end

	if x < 0 or y < 0 or x >= panel.m_Width or y >= panel.m_Height then
		return nil
	end

	if not panel.m_Enabled then
		if panel.m_MouseInputEnabled then
			return false
		end

		return nil
	end

	local children = panel.m_Children

	for i = #children, 1, -1 do
		local child = children[i]

		-- A panel scaled to zero covers no area
		if child.m_ScaleX ~= 0 and child.m_ScaleY ~= 0 then
			local hit = HitTest(child, child:ParentToLocal(x, y))

			if hit ~= nil then
				return hit
			end
		end
	end

	if panel.m_MouseInputEnabled then
		return panel
	end

	return nil
end

local function Think(panel, deltaTime)
	if panel.m_Removed or not panel.m_Visible then
		return
	end

	panel:Think(deltaTime)

	local children = Array.Copy(panel.m_Children)

	for i = 1, #children do
		Think(children[i], deltaTime)
	end
end

local function Layout(panel)
	if panel.m_Removed or not panel.m_Visible then
		return false
	end

	local laidOut = false

	if panel.m_LayoutDirty then
		panel:PerformLayout(panel.m_Width, panel.m_Height)
		panel.m_LayoutDirty = false
		laidOut = true
	end

	local children = Array.Copy(panel.m_Children)

	for i = 1, #children do
		laidOut = Layout(children[i]) or laidOut
	end

	return laidOut
end

local function Paint(panel)
	if panel.m_Removed or not panel.m_Visible then
		return
	end

	if panel.m_PaintDirty then
		panel:Paint(panel.m_Width, panel.m_Height)
		panel.m_PaintDirty = false
	end

	local children = Array.Copy(panel.m_Children)

	for i = 1, #children do
		Paint(children[i])
	end
end

-- The size is 0 while the rendered area is hidden or minimized: the last known size is kept
local function UpdateViewportSize(self)
	local size = Inputs.GetViewportSize()

	self.m_ViewportVisible = size.x > 0 and size.y > 0

	if self.m_ViewportVisible then
		self.m_ViewportWidth, self.m_ViewportHeight = size.x, size.y
	end
end

local function UpdateRootSize(self)
	local width, height

	if self.m_ViewportWidth then
		width, height = CanvasSpace.GetSize(self.m_Canvas, self.m_ViewportWidth, self.m_ViewportHeight)
	else
		local reference = self.m_Canvas:GetReferenceResolution()
		width, height = reference.x, reference.y
	end

	self.m_Root:SetSize(width, height)
end

-- The viewport size, or the reference resolution until the rendered area has been visible
local function GetViewport(self)
	if self.m_ViewportWidth then
		return self.m_ViewportWidth, self.m_ViewportHeight
	end

	local reference = self.m_Canvas:GetReferenceResolution()
	return reference.x, reference.y
end

local function CreateWorldView(self)
	if self.m_Camera and self.m_ViewportWidth then
		return WorldSpace.CreateView(self.m_Camera, self.m_ViewportWidth, self.m_ViewportHeight)
	end

	return nil
end

-- Places the panels anchored in the 3D world, parents first, before the mouse is hit-tested
local function UpdateWorld(self, panel, view, deltaTime)
	if panel.m_Removed or not panel.m_Visible then
		return
	end

	if panel.UpdateWorld then
		if not view and not self.m_Camera and not self.m_CameraWarningLogged then
			Debug.LogWarning("UI: panels placed in the world stay hidden until controller:SetCamera is called")
			self.m_CameraWarningLogged = true
		end

		panel:UpdateWorld(view, deltaTime)

		-- OnTargetLost may remove the panel
		if panel.m_Removed then
			return
		end
	end

	local children = Array.Copy(panel.m_Children)

	for i = 1, #children do
		UpdateWorld(self, children[i], view, deltaTime)
	end
end

-- The plane of a world space canvas this frame. The engine lays it on the actor's local XY plane,
-- centred on the actor, unscaled by the actor, facing the actor's +Z.
local function CreateCanvasFrame(self)
	local transform = self.m_CanvasActor:GetTransform()
	local position, rotation = transform:GetWorldPosition(), transform:GetWorldRotation()
	local right, up, normal = transform:GetWorldRight(), transform:GetWorldUp(), transform:GetWorldForward()
	local width, height = self.m_Root:GetSize()

	return {
		x = position.x, y = position.y, z = position.z,
		rightX = right.x, rightY = right.y, rightZ = right.z,
		upX = up.x, upY = up.y, upZ = up.z,
		normalX = normal.x, normalY = normal.y, normalZ = normal.z,
		rotation = rotation,
		scale = self.m_Canvas:GetWorldScale(),
		width = width,
		height = height
	}
end

-- Where the ray through a viewport position meets the plane of a world space canvas, in canvas space.
-- Also tells whether it meets the front of the canvas, in front of the camera.
local function RayToCanvas(self, view, x, y)
	local frame = self:GetCanvasFrame()
	local originX, originY, originZ, directionX, directionY, directionZ = WorldSpace.ViewportToRay(view, x, y)
	local distance = WorldSpace.IntersectPlane(
		originX, originY, originZ, directionX, directionY, directionZ,
		frame.x, frame.y, frame.z, frame.normalX, frame.normalY, frame.normalZ
	)

	if not distance then
		return nil
	end

	local offsetX = originX + directionX * distance - frame.x
	local offsetY = originY + directionY * distance - frame.y
	local offsetZ = originZ + directionZ * distance - frame.z
	local alongRight = offsetX * frame.rightX + offsetY * frame.rightY + offsetZ * frame.rightZ
	local alongUp = offsetX * frame.upX + offsetY * frame.upY + offsetZ * frame.upZ
	local facing = directionX * frame.normalX + directionY * frame.normalY + directionZ * frame.normalZ

	return
		alongRight / frame.scale + frame.width * 0.5,
		frame.height * 0.5 - alongUp / frame.scale,
		distance > 0 and facing < 0
end

-- Callbacks run last: they may remove the panel they are called on
local function SetHoveredPanel(self, panel)
	local previous = self.m_Hovered

	if previous == panel then
		return
	end

	self.m_Hovered = panel

	if previous and previous:IsValid() then
		previous:InvalidatePaint()
		previous:OnCursorExited()
	end

	if panel and panel:IsValid() then
		panel:InvalidatePaint()
		panel:OnCursorEntered()
	end
end

local function PressMouseButton(self, button)
	local target = self.m_Hovered

	if target and target.m_KeyboardInputEnabled then
		self:SetFocusedPanel(target)
	else
		self:SetFocusedPanel(nil)
	end

	if target and target:IsValid() then
		self.m_Pressed[button] = target
		target:InvalidatePaint()
		target:OnMousePressed(button)
	end
end

local function ReleaseMouseButton(self, button)
	local target = self.m_Pressed[button]
	self.m_Pressed[button] = nil

	if target and target:IsValid() then
		target:InvalidatePaint()
		target:OnMouseReleased(button)
	end
end

local function UpdateMouse(self)
	if not self.m_ViewportVisible then
		SetHoveredPanel(self, nil)
		return
	end

	local mouse = Inputs.GetMousePos()
	local x, y, inside

	-- Parts of a canvas larger than the rendered area can't be hovered from outside of it
	local inViewport = mouse.x >= 0 and mouse.y >= 0 and mouse.x < self.m_ViewportWidth and mouse.y < self.m_ViewportHeight

	if CanvasSpace.IsWorldSpace(self.m_Canvas) then
		local view = self.m_WorldView
		local front

		if view then
			x, y, front = RayToCanvas(self, view, mouse.x, mouse.y)
		elseif not self.m_CameraWarningLogged then
			Debug.LogWarning("UI: a world space canvas can't be hovered until controller:SetCamera is called")
			self.m_CameraWarningLogged = true
		end

		-- A ray missing the plane keeps the cursor where it was, so a drag doesn't jump
		if not x then
			x, y, front = self.m_CursorX, self.m_CursorY, false
		end

		-- The root covers the canvas: the hit-test leaves out what falls beyond it
		inside = inViewport and front
	else
		x, y = CanvasSpace.ViewportToCanvas(self.m_Canvas, self.m_ViewportWidth, self.m_ViewportHeight, mouse.x, mouse.y)
		inside = inViewport
	end

	local moved = x ~= self.m_CursorX or y ~= self.m_CursorY
	local root = self.m_Root

	self.m_CursorX, self.m_CursorY = x, y
	SetHoveredPanel(self, inside and HitTest(root, root:ParentToLocal(x, y)) or nil)

	if moved then
		local notified = {}

		for i = 1, #self.m_MouseButtons do
			local captured = self.m_Pressed[self.m_MouseButtons[i]]

			if captured and not notified[captured] and captured:IsValid() then
				notified[captured] = true
				captured:OnCursorMoved(captured:ScreenToLocal(x, y))
			end
		end

		local hovered = self.m_Hovered

		if hovered and not notified[hovered] and hovered:IsValid() then
			hovered:OnCursorMoved(hovered:ScreenToLocal(x, y))
		end
	end

	for i = 1, #self.m_MouseButtons do
		local button = self.m_MouseButtons[i]
		local pressed, released = Inputs.GetMouseButtonDown(button), Inputs.GetMouseButtonUp(button)

		-- Pressed and released within the frame: still held means the release came first
		if pressed and released and Inputs.GetMouseButton(button) then
			ReleaseMouseButton(self, button)
			PressMouseButton(self, button)
		else
			if pressed then
				PressMouseButton(self, button)
			end

			if released then
				ReleaseMouseButton(self, button)
			end
		end
	end

	local scroll = Inputs.GetMouseScroll()

	if scroll.y ~= 0 then
		local panel = self.m_Hovered

		while panel and not panel:OnMouseWheeled(scroll.y) do
			panel = panel.m_Parent
		end
	end
end

-- Returns false when the callback moved or removed the focus
local function ReleaseKey(self, focused, key)
	self.m_HeldKeys[key] = nil
	focused:OnKeyCodeReleased(key)

	return self.m_Focused == focused
end

local function UpdateKeyboard(self, deltaTime)
	local focused = self.m_Focused

	if not focused then
		return
	end

	local keys = Keyboard.GetTrackedKeys()

	for i = 1, #keys do
		local key = keys[i]
		local down, up = Inputs.GetKeyDown(key), Inputs.GetKeyUp(key)
		local pressed = false

		-- Pressed and released within the frame: still held means the release came first
		if down and up and Inputs.GetKey(key) then
			if not ReleaseKey(self, focused, key) then
				return
			end

			up = false
		end

		if down then
			self.m_HeldKeys[key] = kRepeatDelay
			pressed = true
		elseif self.m_HeldKeys[key] then
			if Inputs.GetKey(key) then
				local remaining = self.m_HeldKeys[key] - deltaTime
				pressed = remaining <= 0
				self.m_HeldKeys[key] = pressed and kRepeatInterval or remaining
			else
				self.m_HeldKeys[key] = nil
			end
		end

		-- A callback moving or removing the focus stops the dispatch for this frame
		if pressed then
			focused:OnKeyCodePressed(key)

			if self.m_Focused ~= focused then
				return
			end
		end

		if up and not ReleaseKey(self, focused, key) then
			return
		end
	end
end

local function ResetCursorShape(self)
	if self.m_CursorShapeSet then
		Inputs.SetCursorShape(CursorShape.ARROW)
		self.m_CursorShapeSet = false
	end
end

-- The window applies a cursor shape for one frame only, so it is set again every frame. Once set,
-- ImGui keeps it until its own cursor changes, so the arrow is restored explicitly.
local function UpdateCursorShape(self)
	local hovered = self.m_Hovered
	local shape = hovered and hovered.m_Cursor

	if shape then
		Inputs.SetCursorShape(shape)
		self.m_CursorShapeSet = true
	else
		ResetCursorShape(self)
	end
end

-- Forgets the panels and the canvas, once they are removed or deleted
local function Release(self)
	ResetCursorShape(self)

	self.m_Root = nil
	self.m_Hovered = nil
	self.m_Focused = nil
	self.m_Pressed = {}
	self.m_HeldKeys = {}
	self.m_CanvasActor = nil
	self.m_Canvas = nil
	self.m_Camera = nil
	self.m_WorldView = nil
	self.m_CanvasFrame = nil
end

--- Creates a controller on the given actor Canvas, or on a new canvas actor owned by the controller
---@param canvasActor Actor|nil
---@return UIController
function Controller.New(canvasActor)
	local self = setmetatable({}, Controller)

	if canvasActor then
		assert(canvasActor:GetCanvas(), "UI: the actor given to CreateController must hold a Canvas")
		self.m_OwnsCanvas = false
	else
		canvasActor = Scenes.GetCurrentScene():CreateActor("UI Canvas", "")
		canvasActor:AddCanvas()
		self.m_OwnsCanvas = true
	end

	self.m_CanvasActor = canvasActor
	self.m_Canvas = canvasActor:GetCanvas()
	self.m_MouseButtons = { MouseButton.BUTTON_LEFT, MouseButton.BUTTON_RIGHT, MouseButton.BUTTON_MIDDLE }
	self.m_CursorX, self.m_CursorY = 0, 0
	self.m_Pressed = {}
	self.m_HeldKeys = {}
	self.m_Root = Registry.Create(self, "Panel", nil)
	self.m_Root:SetMouseInputEnabled(false)
	UpdateViewportSize(self)
	UpdateRootSize(self)

	return self
end

--- Returns the size, in pixels, of the area the canvas is rendered into (the Game View in the
--- editor, the window in a built game), or nil while it has never been visible
---@return number|nil, number|nil
function Controller:GetViewportSize()
	return self.m_ViewportWidth, self.m_ViewportHeight
end

--- Processes inputs, then runs Think, PerformLayout and Paint on the panels that need it
---@param deltaTime number
function Controller:Update(deltaTime)
	local root = self.m_Root

	if not root then
		return
	end

	if not self.m_CanvasActor:IsAlive() then
		Debug.LogWarning("UI: the canvas actor was destroyed without calling controller:Destroy(), its panels are discarded")
		root:Discard()
		Release(self)
		return
	end

	UpdateViewportSize(self)
	UpdateRootSize(self)
	self.m_CanvasFrame = nil
	self.m_WorldView = CreateWorldView(self)
	UpdateWorld(self, root, self.m_WorldView, deltaTime)
	UpdateMouse(self)
	UpdateKeyboard(self, deltaTime)
	Think(root, deltaTime)

	for _ = 1, kMaxLayoutPasses do
		if not Layout(root) then
			break
		end
	end

	Paint(root)
	UpdateCursorShape(self)
end

--- Defines the camera used to place panels in the 3D world, usually the camera rendering the scene.
--- The engine picks the first active camera of the scene, which Lua can't query.
---@param cameraActor Actor|nil
function Controller:SetCamera(cameraActor)
	assert(cameraActor == nil or cameraActor:GetCamera(), "UI: the actor given to SetCamera must hold a Camera")

	self.m_Camera = cameraActor
	self.m_WorldView = nil
end

---@return Actor|nil
function Controller:GetCamera()
	return self.m_Camera
end

--- Returns the camera state used to place the world panels this frame, or nil without a camera
---@package
---@return UIWorldView|nil
function Controller:GetWorldView()
	if not self.m_WorldView then
		self.m_WorldView = CreateWorldView(self)
	end

	return self.m_WorldView
end

--- Returns true when the canvas is drawn in the world rather than over the screen
---@return boolean
function Controller:IsWorldSpace()
	return CanvasSpace.IsWorldSpace(self.m_Canvas)
end

--- Returns the plane of a world space canvas, or nil on a screen space canvas: its centre (x, y, z),
--- its right, up and normal axes (rightX..., upX..., normalX..., the normal facing the viewer), its
--- world rotation, the world size of one canvas unit (scale), and its size in canvas units. Read once
--- per update: a canvas actor moved since is seen from the next update on.
---@return table|nil
function Controller:GetCanvasFrame()
	if not self:IsWorldSpace() then
		return nil
	end

	if not self.m_CanvasFrame then
		self.m_CanvasFrame = CreateCanvasFrame(self)
	end

	return self.m_CanvasFrame
end

--- Converts a canvas position into a world position on a world space canvas, or returns nil on a
--- screen space canvas
---@param x number
---@param y number
---@return Vector3|nil
function Controller:CanvasToWorld(x, y)
	local frame = self:GetCanvasFrame()

	if not frame then
		return nil
	end

	local alongRight = (x - frame.width * 0.5) * frame.scale
	local alongUp = (frame.height * 0.5 - y) * frame.scale

	return Vector3.new(
		frame.x + frame.rightX * alongRight + frame.upX * alongUp,
		frame.y + frame.rightY * alongRight + frame.upY * alongUp,
		frame.z + frame.rightZ * alongRight + frame.upZ * alongUp
	)
end

--- Converts a position in pixels of the rendered area into canvas space. On a world space canvas,
--- casts a ray from the camera and returns nil when it misses the plane or no camera is set.
---@param x number
---@param y number
---@return number|nil, number|nil
function Controller:ViewportToCanvas(x, y)
	if self:IsWorldSpace() then
		local view = self:GetWorldView()

		if not view then
			return nil
		end

		local canvasX, canvasY = RayToCanvas(self, view, x, y)
		return canvasX, canvasY
	end

	local width, height = GetViewport(self)
	return CanvasSpace.ViewportToCanvas(self.m_Canvas, width, height, x, y)
end

--- Returns the number of pixels of the rendered area covered by one canvas unit
---@return number
function Controller:GetCanvasScale()
	local width, height = GetViewport(self)
	return CanvasSpace.GetScale(self.m_Canvas, width, height)
end

--- Projects a world position onto the canvas. Returns the canvas position and the depth in front of
--- the camera, or nil when the position is behind the near plane or no camera is set.
---@param position Vector3
---@return number|nil, number|nil, number|nil
function Controller:WorldToCanvas(position)
	local view = self:GetWorldView()

	if not view then
		return nil
	end

	local viewportX, viewportY, depth = WorldSpace.Project(view, position.x, position.y, position.z)

	if not viewportX then
		return nil
	end

	local x, y = self:ViewportToCanvas(viewportX, viewportY)
	return x, y, depth
end

--- Returns the number of canvas units covered by one world unit at the given depth, or nil without a
--- camera
---@param depth number
---@return number|nil
function Controller:GetCanvasUnitsPerWorldUnit(depth)
	local view = self:GetWorldView()

	if not view then
		return nil
	end

	return WorldSpace.PixelsPerWorldUnit(view, depth) / self:GetCanvasScale()
end

--- Creates a panel of the given class, under the root panel when no parent is given
---@param className string
---@param parent Panel|nil
---@return Panel
function Controller:Create(className, parent)
	assert(self.m_Root, "UI: the controller has been destroyed")
	return Registry.Create(self, className, parent)
end

--- Returns the panel covering the whole canvas, parent of the panels created without a parent
---@return Panel|nil
function Controller:GetRoot()
	return self.m_Root
end

---@return Actor|nil
function Controller:GetCanvasActor()
	return self.m_CanvasActor
end

---@return Panel|nil
function Controller:GetHoveredPanel()
	return self.m_Hovered
end

---@return Panel|nil
function Controller:GetFocusedPanel()
	return self.m_Focused
end

--- Returns the cursor position in canvas space
---@return number, number
function Controller:GetCursorPos()
	return self.m_CursorX, self.m_CursorY
end

--- Gives the keyboard focus to the panel, or removes it when nil
---@param panel Panel|nil
function Controller:SetFocusedPanel(panel)
	if panel and not (panel.m_KeyboardInputEnabled and IsInteractive(panel)) then
		return
	end

	local previous = self.m_Focused

	if previous == panel then
		return
	end

	self.m_Focused = panel
	self.m_HeldKeys = {}

	if previous and previous:IsValid() then
		previous:InvalidatePaint()
		previous:OnFocusChanged(false)
	end

	if panel and panel:IsValid() then
		panel:InvalidatePaint()
		panel:OnFocusChanged(true)
	end
end

--- Clears the hover, focus and mouse capture held by the panel or one of its children
---@package
---@param panel Panel
function Controller:ReleasePanel(panel)
	if self.m_Hovered and IsInSubtree(self.m_Hovered, panel) then
		SetHoveredPanel(self, nil)
	end

	for button, pressed in pairs(self.m_Pressed) do
		if IsInSubtree(pressed, panel) then
			self.m_Pressed[button] = nil
		end
	end

	if self.m_Focused and IsInSubtree(self.m_Focused, panel) then
		self:SetFocusedPanel(nil)
	end
end

--- Removes every panel, and the canvas actor when the controller created it. Safe to call from
--- OnDestroy, including while the scene unloads.
function Controller:Destroy()
	local root, canvasActor = self.m_Root, self.m_CanvasActor

	if not root then
		return
	end

	-- Cleared first, so a Destroy call made from OnRemove returns immediately
	self.m_Root = nil

	-- While a scene unloads, its actors are already being deleted
	if canvasActor:IsAlive() then
		root:Remove()

		if self.m_OwnsCanvas then
			canvasActor:Destroy()
		end
	else
		root:Discard()
	end

	Release(self)
end

return Controller
