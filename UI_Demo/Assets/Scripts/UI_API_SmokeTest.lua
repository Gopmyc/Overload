---@class UI_API_SmokeTest : Behaviour
local UI_API_SmokeTest =
{
	time = 0.0,
	anchorStepTimer = 0.0,
	layoutStepTimer = 0.0,
	anchorIndex = 1,
	layoutPhase = 0
}

local anchors =
{
	AnchorPreset.TOP_LEFT,
	AnchorPreset.TOP_CENTER,
	AnchorPreset.TOP_RIGHT,
	AnchorPreset.MIDDLE_LEFT,
	AnchorPreset.CENTER,
	AnchorPreset.MIDDLE_RIGHT,
	AnchorPreset.BOTTOM_LEFT,
	AnchorPreset.BOTTOM_CENTER,
	AnchorPreset.BOTTOM_RIGHT,
	AnchorPreset.HORIZONTAL_STRETCH_TOP,
	AnchorPreset.HORIZONTAL_STRETCH_MIDDLE,
	AnchorPreset.HORIZONTAL_STRETCH_BOTTOM,
	AnchorPreset.VERTICAL_STRETCH_LEFT,
	AnchorPreset.VERTICAL_STRETCH_CENTER,
	AnchorPreset.VERTICAL_STRETCH_RIGHT,
	AnchorPreset.STRETCH_BOTH
}

local function EnsureTransform2D(actor)
	local transform2D = actor:GetTransform2D()
	if transform2D == nil then
		transform2D = actor:AddTransform2D()
	end
	return transform2D
end

local function EnsureActor(scene, name, tag, parent)
	local actor = scene:FindActorByName(name)
	if actor == nil then
		actor = scene:CreateActor(name, tag)
	end

	if parent ~= nil then
		local currentParent = actor:GetParent()
		if currentParent == nil or currentParent:GetID() ~= parent:GetID() then
			actor:SetParent(parent)
		end
	end

	return actor
end

function UI_API_SmokeTest:OnStart()
	local scene = Scenes.GetCurrentScene()

	self.canvasActor = EnsureActor(scene, "Lua_UI_Runtime_Canvas", "UI", nil)
	self.canvas = self.canvasActor:GetCanvas()
	if self.canvas == nil then
		self.canvas = self.canvasActor:AddCanvas()
	end

	local canvasTransform = EnsureTransform2D(self.canvasActor)
	canvasTransform:SetAnchorPreset(AnchorPreset.CENTER)
	canvasTransform:SetPosition(Vector2.Zero())
	canvasTransform:SetSize(Vector2.new(1920.0, 1080.0))

	self.canvas:SetScalerMode(CanvasScalerMode.SCALE_WITH_SCREEN_SIZE)
	self.canvas:SetReferenceResolution(Vector2.new(1920.0, 1080.0))
	self.canvas:SetScaleFactor(1.0)
	self.canvas:SetPixelsPerUnit(100.0)

	self.panelActor = EnsureActor(scene, "Lua_UI_Runtime_Panel", "UI", self.canvasActor)
	local panelTransform = EnsureTransform2D(self.panelActor)
	panelTransform:SetAnchorPreset(AnchorPreset.CENTER)
	panelTransform:SetPosition(Vector2.new(0.0, 0.0))
	panelTransform:SetSize(Vector2.new(980.0, 600.0))

	self.panelImage = self.panelActor:GetImage()
	if self.panelImage == nil then
		self.panelImage = self.panelActor:AddImage()
	end
	self.panelImage:SetSize(Vector2.new(980.0, 600.0))
	self.panelImage:SetTint(Vector4.new(0.09, 0.10, 0.13, 0.95))

	self.rootLayout = self.panelActor:GetVerticalLayout()
	if self.rootLayout == nil then
		self.rootLayout = self.panelActor:AddVerticalLayout()
	end
	self.rootLayout:SetSpacing(22.0)
	self.rootLayout:SetPadding(Vector4.new(30.0, 30.0, 30.0, 30.0))
	self.rootLayout:SetSize(Vector2.new(980.0, 600.0))
	self.rootLayout:SetHorizontalAlignment(LayoutHorizontalAlignment.CENTER)
	self.rootLayout:SetVerticalAlignment(LayoutVerticalAlignment.TOP)
	self.rootLayout:SetControlChildrenWidth(true)
	self.rootLayout:SetControlChildrenHeight(false)

	self.headerActor = EnsureActor(scene, "Lua_UI_Runtime_Header", "UI", self.panelActor)
	local headerTransform = EnsureTransform2D(self.headerActor)
	headerTransform:SetAnchorPreset(AnchorPreset.TOP_CENTER)
	headerTransform:SetSize(Vector2.new(860.0, 86.0))
	headerTransform:SetPosition(Vector2.new(0.0, 0.0))

	self.headerText = self.headerActor:GetText()
	if self.headerText == nil then
		self.headerText = self.headerActor:AddText()
	end
	self.headerText:SetText("UI Lua Smoke Test")
	self.headerText:SetFontPath(":Fonts\\Roboto-Regular.ttf")
	self.headerText:SetFontSize(46.0)
	self.headerText:SetExtents(Vector2.new(860.0, 86.0))
	self.headerText:SetHorizontalAlignment(TextHorizontalAlignment.CENTER)
	self.headerText:SetVerticalAlignment(TextVerticalAlignment.CENTER)

	self.anchorProbeActor = EnsureActor(scene, "Lua_UI_Runtime_AnchorProbe", "UI", self.panelActor)
	self.anchorProbeTransform = EnsureTransform2D(self.anchorProbeActor)
	self.anchorProbeTransform:SetSize(Vector2.new(340.0, 110.0))
	self.anchorProbeTransform:SetAnchorPreset(AnchorPreset.TOP_LEFT)
	self.anchorProbeTransform:SetPosition(Vector2.new(0.0, -120.0))

	self.anchorProbeImage = self.anchorProbeActor:GetImage()
	if self.anchorProbeImage == nil then
		self.anchorProbeImage = self.anchorProbeActor:AddImage()
	end
	self.anchorProbeImage:SetSize(Vector2.new(340.0, 110.0))
	self.anchorProbeImage:SetTint(Vector4.new(0.16, 0.20, 0.26, 0.90))

	self.anchorProbeText = self.anchorProbeActor:GetText()
	if self.anchorProbeText == nil then
		self.anchorProbeText = self.anchorProbeActor:AddText()
	end
	self.anchorProbeText:SetText("Anchor preset test")
	self.anchorProbeText:SetExtents(Vector2.new(320.0, 80.0))
	self.anchorProbeText:SetHorizontalAlignment(TextHorizontalAlignment.CENTER)
	self.anchorProbeText:SetVerticalAlignment(TextVerticalAlignment.CENTER)
	self.anchorProbeText:SetColor(Vector4.new(0.95, 0.95, 1.0, 1.0))

	self.statusActor = EnsureActor(scene, "Lua_UI_Runtime_Status", "UI", self.panelActor)
	local statusTransform = EnsureTransform2D(self.statusActor)
	statusTransform:SetAnchorPreset(AnchorPreset.BOTTOM_CENTER)
	statusTransform:SetSize(Vector2.new(900.0, 70.0))
	statusTransform:SetPosition(Vector2.new(0.0, 0.0))

	self.statusText = self.statusActor:GetText()
	if self.statusText == nil then
		self.statusText = self.statusActor:AddText()
	end
	self.statusText:SetText("Running...")
	self.statusText:SetFontSize(28.0)
	self.statusText:SetExtents(Vector2.new(900.0, 70.0))
	self.statusText:SetHorizontalAlignment(TextHorizontalAlignment.CENTER)
	self.statusText:SetVerticalAlignment(TextVerticalAlignment.CENTER)

	Debug.LogInfo("[UI_API_SmokeTest] Scene bootstrapped.")
end

function UI_API_SmokeTest:ApplyLayoutPhase()
	self.layoutPhase = self.layoutPhase + 1
	if self.layoutPhase > 4 then
		self.layoutPhase = 1
	end

	if self.layoutPhase == 1 then
		self.rootLayout:SetControlChildrenWidth(true)
		self.rootLayout:SetControlChildrenHeight(false)
		self.rootLayout:SetSpacing(14.0)
		self.rootLayout:SetPadding(Vector4.new(22.0, 22.0, 20.0, 20.0))
	elseif self.layoutPhase == 2 then
		self.rootLayout:SetControlChildrenWidth(false)
		self.rootLayout:SetControlChildrenHeight(true)
		self.rootLayout:SetSpacing(26.0)
		self.rootLayout:SetPadding(Vector4.new(34.0, 34.0, 28.0, 28.0))
	elseif self.layoutPhase == 3 then
		self.rootLayout:SetControlChildrenWidth(true)
		self.rootLayout:SetControlChildrenHeight(true)
		self.rootLayout:SetSpacing(34.0)
		self.rootLayout:SetPadding(Vector4.new(40.0, 40.0, 34.0, 34.0))
	else
		self.rootLayout:SetControlChildrenWidth(false)
		self.rootLayout:SetControlChildrenHeight(false)
		self.rootLayout:SetSpacing(18.0)
		self.rootLayout:SetPadding(Vector4.new(30.0, 30.0, 30.0, 30.0))
	end
end

function UI_API_SmokeTest:OnUpdate(deltaTime)
	self.time = self.time + deltaTime
	self.anchorStepTimer = self.anchorStepTimer + deltaTime
	self.layoutStepTimer = self.layoutStepTimer + deltaTime

	local pulse = 0.5 + 0.5 * math.sin(self.time * 2.2)
	self.headerText:SetColor(Vector4.new(1.0, 0.75 + (0.25 * pulse), 0.40 + (0.55 * pulse), 1.0))

	if self.anchorStepTimer >= 1.0 then
		self.anchorStepTimer = self.anchorStepTimer - 1.0
		self.anchorIndex = self.anchorIndex + 1
		if self.anchorIndex > #anchors then
			self.anchorIndex = 1
		end

		local preset = anchors[self.anchorIndex]
		self.anchorProbeTransform:SetAnchorPreset(preset)

		local offsetX = math.sin(self.time * 0.9) * 210.0
		local offsetY = math.cos(self.time * 0.7) * 120.0
		self.anchorProbeTransform:SetPosition(Vector2.new(offsetX, offsetY))
	end

	if self.layoutStepTimer >= 2.0 then
		self.layoutStepTimer = self.layoutStepTimer - 2.0
		self:ApplyLayoutPhase()

		self.statusText:SetText(
			"layout phase=" .. tostring(self.layoutPhase) ..
			" | anchor index=" .. tostring(self.anchorIndex) ..
			" | controlW=" .. tostring(self.rootLayout:GetControlChildrenWidth()) ..
			" | controlH=" .. tostring(self.rootLayout:GetControlChildrenHeight())
		)
	end
end

return UI_API_SmokeTest

