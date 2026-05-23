---@class UI_API_AnchorCycle : Behaviour
local UI_API_AnchorCycle =
{
	time = 0.0,
	stepTimer = 0.0,
	anchorIndex = 1
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

function UI_API_AnchorCycle:OnStart()
	self.transform2D = EnsureTransform2D(self.owner)
	self.transform2D:SetSize(Vector2.new(380.0, 120.0))
	self.transform2D:SetAnchorPreset(anchors[self.anchorIndex])

	self.image = self.owner:GetImage()
	if self.image == nil then
		self.image = self.owner:AddImage()
	end
	self.image:SetSize(Vector2.new(380.0, 120.0))
	self.image:SetTint(Vector4.new(0.14, 0.18, 0.24, 0.90))

	self.text = self.owner:GetText()
	if self.text == nil then
		self.text = self.owner:AddText()
	end
	self.text:SetFontSize(26.0)
	self.text:SetExtents(Vector2.new(360.0, 90.0))
	self.text:SetHorizontalAlignment(TextHorizontalAlignment.CENTER)
	self.text:SetVerticalAlignment(TextVerticalAlignment.CENTER)
	self.text:SetText("Anchor cycle started")

	Debug.LogInfo("[UI_API_AnchorCycle] Ready.")
end

function UI_API_AnchorCycle:OnUpdate(deltaTime)
	self.time = self.time + deltaTime
	self.stepTimer = self.stepTimer + deltaTime

	local x = math.sin(self.time * 1.0) * 260.0
	local y = math.cos(self.time * 1.4) * 160.0
	self.transform2D:SetPosition(Vector2.new(x, y))

	local pulse = 0.5 + 0.5 * math.sin(self.time * 2.1)
	self.image:SetTint(Vector4.new(0.10 + (0.30 * pulse), 0.18, 0.24 + (0.30 * pulse), 0.92))

	if self.stepTimer >= 1.0 then
		self.stepTimer = self.stepTimer - 1.0
		self.anchorIndex = self.anchorIndex + 1
		if self.anchorIndex > #anchors then
			self.anchorIndex = 1
		end

		local preset = anchors[self.anchorIndex]
		self.transform2D:SetAnchorPreset(preset)
		self.text:SetText("Anchor index: " .. tostring(self.anchorIndex))
	end
end

return UI_API_AnchorCycle

