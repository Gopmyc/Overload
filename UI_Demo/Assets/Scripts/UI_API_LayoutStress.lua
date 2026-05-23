---@class UI_API_LayoutStress : Behaviour
local UI_API_LayoutStress =
{
	time = 0.0,
	stepTimer = 0.0,
	phase = 0,
	childCount = 6
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

function UI_API_LayoutStress:OnStart()
	local scene = Scenes.GetCurrentScene()
	self.root = self.owner

	local rootTransform = EnsureTransform2D(self.root)
	rootTransform:SetAnchorPreset(AnchorPreset.CENTER)
	rootTransform:SetPosition(Vector2.new(0.0, 0.0))
	rootTransform:SetSize(Vector2.new(900.0, 460.0))

	local rootImage = self.root:GetImage()
	if rootImage == nil then
		rootImage = self.root:AddImage()
	end
	rootImage:SetSize(Vector2.new(900.0, 460.0))
	rootImage:SetTint(Vector4.new(0.08, 0.08, 0.10, 0.85))

	self:EnsureLayoutMode(false)
	self.layout:SetSize(Vector2.new(900.0, 460.0))
	self.layout:SetPadding(Vector4.new(18.0, 18.0, 18.0, 18.0))
	self.layout:SetSpacing(10.0)
	self.layout:SetHorizontalAlignment(LayoutHorizontalAlignment.CENTER)
	self.layout:SetVerticalAlignment(LayoutVerticalAlignment.TOP)
	self.layout:SetControlChildrenWidth(true)
	self.layout:SetControlChildrenHeight(false)

	for i = 1, self.childCount do
		local child = EnsureActor(scene, "Lua_UI_LayoutStress_Item_" .. tostring(i), "UI", self.root)
		local childTransform = EnsureTransform2D(child)
		childTransform:SetAnchorPreset(AnchorPreset.TOP_LEFT)
		childTransform:SetPosition(Vector2.Zero())
		childTransform:SetSize(Vector2.new(240.0 + (i * 18.0), 36.0 + (i * 4.0)))

		local childImage = child:GetImage()
		if childImage == nil then
			childImage = child:AddImage()
		end
		childImage:SetSize(Vector2.new(240.0 + (i * 18.0), 36.0 + (i * 4.0)))
		childImage:SetTint(Vector4.new(0.10 + (i * 0.05), 0.14, 0.20, 0.95))

		local childText = child:GetText()
		if childText == nil then
			childText = child:AddText()
		end
		childText:SetText("Layout item " .. tostring(i))
		childText:SetFontSize(22.0)
		childText:SetExtents(Vector2.new(220.0 + (i * 16.0), 32.0 + (i * 3.0)))
		childText:SetHorizontalAlignment(TextHorizontalAlignment.CENTER)
		childText:SetVerticalAlignment(TextVerticalAlignment.CENTER)
	end

	Debug.LogInfo("[UI_API_LayoutStress] Layout stress hierarchy ready.")
end

function UI_API_LayoutStress:EnsureLayoutMode(horizontal)
	if horizontal then
		local horizontalLayout = self.root:GetHorizontalLayout()
		if horizontalLayout == nil then
			if self.root:GetVerticalLayout() ~= nil then
				self.root:RemoveVerticalLayout()
			end
			horizontalLayout = self.root:AddHorizontalLayout()
		end
		self.layout = horizontalLayout
	else
		local verticalLayout = self.root:GetVerticalLayout()
		if verticalLayout == nil then
			if self.root:GetHorizontalLayout() ~= nil then
				self.root:RemoveHorizontalLayout()
			end
			verticalLayout = self.root:AddVerticalLayout()
		end
		self.layout = verticalLayout
	end
end

function UI_API_LayoutStress:ApplyPhase()
	self.phase = self.phase + 1
	if self.phase > 4 then
		self.phase = 1
	end

	if self.phase == 1 then
		self:EnsureLayoutMode(false)
		self.layout:SetControlChildrenWidth(true)
		self.layout:SetControlChildrenHeight(false)
		self.layout:SetSpacing(8.0)
		self.layout:SetPadding(Vector4.new(14.0, 14.0, 14.0, 14.0))
		self.layout:SetHorizontalAlignment(LayoutHorizontalAlignment.LEFT)
	elseif self.phase == 2 then
		self:EnsureLayoutMode(true)
		self.layout:SetControlChildrenWidth(false)
		self.layout:SetControlChildrenHeight(true)
		self.layout:SetSpacing(20.0)
		self.layout:SetPadding(Vector4.new(26.0, 26.0, 18.0, 18.0))
		self.layout:SetHorizontalAlignment(LayoutHorizontalAlignment.CENTER)
	elseif self.phase == 3 then
		self:EnsureLayoutMode(false)
		self.layout:SetControlChildrenWidth(true)
		self.layout:SetControlChildrenHeight(true)
		self.layout:SetSpacing(30.0)
		self.layout:SetPadding(Vector4.new(34.0, 34.0, 22.0, 22.0))
		self.layout:SetHorizontalAlignment(LayoutHorizontalAlignment.RIGHT)
	else
		self:EnsureLayoutMode(true)
		self.layout:SetControlChildrenWidth(false)
		self.layout:SetControlChildrenHeight(false)
		self.layout:SetSpacing(14.0)
		self.layout:SetPadding(Vector4.new(18.0, 18.0, 18.0, 18.0))
		self.layout:SetHorizontalAlignment(LayoutHorizontalAlignment.CENTER)
	end
end

function UI_API_LayoutStress:OnUpdate(deltaTime)
	self.time = self.time + deltaTime
	self.stepTimer = self.stepTimer + deltaTime

	if self.stepTimer >= 1.5 then
		self.stepTimer = self.stepTimer - 1.5
		self:ApplyPhase()

		for i = 1, self.childCount do
			local child = self.root:FindChild("Lua_UI_LayoutStress_Item_" .. tostring(i), false)
			if child ~= nil then
				local transform2D = child:GetTransform2D()
				if transform2D ~= nil then
					transform2D:SetSize(Vector2.new(150.0 + (i * 30.0), 28.0 + (i * 6.0)))
				end

				local image = child:GetImage()
				if image ~= nil then
					image:SetSize(Vector2.new(160.0 + (i * 24.0), 26.0 + (i * 6.0)))
					local pulse = 0.5 + 0.5 * math.sin(self.time * 1.6 + i)
					image:SetTint(Vector4.new(0.08 + (0.45 * pulse), 0.13 + (i * 0.03), 0.23, 0.95))
				end

				local text = child:GetText()
				if text ~= nil then
					text:SetExtents(Vector2.new(140.0 + (i * 28.0), 22.0 + (i * 5.0)))
				end
			end
		end
	end
end

return UI_API_LayoutStress
