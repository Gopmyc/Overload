---@class UI_API_TextImagePulse : Behaviour
local UI_API_TextImagePulse =
{
	time = 0.0,
	alignTimer = 0.0,
	alignIndex = 1
}

local horizontalAlignments =
{
	TextHorizontalAlignment.LEFT,
	TextHorizontalAlignment.CENTER,
	TextHorizontalAlignment.RIGHT
}

local verticalAlignments =
{
	TextVerticalAlignment.TOP,
	TextVerticalAlignment.CENTER,
	TextVerticalAlignment.BOTTOM
}

local function EnsureTransform2D(actor)
	local transform2D = actor:GetTransform2D()
	if transform2D == nil then
		transform2D = actor:AddTransform2D()
	end
	return transform2D
end

function UI_API_TextImagePulse:OnStart()
	self.transform2D = EnsureTransform2D(self.owner)
	self.transform2D:SetAnchorPreset(AnchorPreset.CENTER)
	self.transform2D:SetPosition(Vector2.new(0.0, 0.0))
	self.transform2D:SetSize(Vector2.new(640.0, 260.0))

	self.image = self.owner:GetImage()
	if self.image == nil then
		self.image = self.owner:AddImage()
	end
	self.image:SetSize(Vector2.new(640.0, 260.0))
	self.image:SetTint(Vector4.new(0.10, 0.10, 0.10, 0.92))

	local texture = Resources.GetTexture(":Textures\\Overload.png")
	if texture ~= nil then
		self.image:SetTexture(texture)
	end

	self.text = self.owner:GetText()
	if self.text == nil then
		self.text = self.owner:AddText()
	end

	self.text:SetText("Text/Image API pulse")
	self.text:SetFontPath(":Fonts\\Roboto-Regular.ttf")
	self.text:SetFontSize(34.0)
	self.text:SetExtents(Vector2.new(600.0, 220.0))
	self.text:SetHorizontalAlignment(TextHorizontalAlignment.CENTER)
	self.text:SetVerticalAlignment(TextVerticalAlignment.CENTER)
	self.text:SetColor(Vector4.new(1.0, 1.0, 1.0, 1.0))

	Debug.LogInfo("[UI_API_TextImagePulse] Ready.")
end

function UI_API_TextImagePulse:OnUpdate(deltaTime)
	self.time = self.time + deltaTime
	self.alignTimer = self.alignTimer + deltaTime

	local pulseA = 0.5 + 0.5 * math.sin(self.time * 2.0)
	local pulseB = 0.5 + 0.5 * math.sin(self.time * 1.3 + 1.7)
	local pulseC = 0.5 + 0.5 * math.sin(self.time * 1.9 + 3.1)

	self.image:SetTint(Vector4.new(0.12 + (0.55 * pulseA), 0.12 + (0.30 * pulseB), 0.14 + (0.55 * pulseC), 0.90))
	self.image:SetSize(Vector2.new(560.0 + (120.0 * pulseB), 220.0 + (80.0 * pulseC)))

	self.text:SetFontSize(22.0 + (20.0 * pulseA))
	self.text:SetExtents(Vector2.new(520.0 + (120.0 * pulseC), 180.0 + (70.0 * pulseB)))
	self.text:SetColor(Vector4.new(0.75 + (0.25 * pulseC), 0.75 + (0.25 * pulseA), 0.70 + (0.30 * pulseB), 1.0))

	if self.alignTimer >= 1.2 then
		self.alignTimer = self.alignTimer - 1.2
		self.alignIndex = self.alignIndex + 1
		if self.alignIndex > 3 then
			self.alignIndex = 1
		end

		self.text:SetHorizontalAlignment(horizontalAlignments[self.alignIndex])
		self.text:SetVerticalAlignment(verticalAlignments[self.alignIndex])
		self.text:SetText("Text/Image API pulse | align step " .. tostring(self.alignIndex))
	end
end

return UI_API_TextImagePulse

