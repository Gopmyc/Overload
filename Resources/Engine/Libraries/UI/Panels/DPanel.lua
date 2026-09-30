local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")
local Skin = Resources.GetScript(":Libraries/UI/Core/Skin.lua")

Resources.GetScript(":Libraries/UI/Panels/Panel.lua")

--- Panel with an optional background, drawn by an Image component on the panel actor
---@class DPanel : Panel
local DPanel = {}

function DPanel:Init()
	self.m_PaintBackground = true
	self.m_BackgroundColor = nil
	self.m_BackgroundTexture = nil
	self.m_Image = nil
end

---@param paintBackground boolean
function DPanel:SetPaintBackground(paintBackground)
	self.m_PaintBackground = paintBackground and true or false
	self:InvalidatePaint()
end

---@return boolean
function DPanel:GetPaintBackground()
	return self.m_PaintBackground
end

--- Overrides the skin background color, or restores it when nil
---@param color Vector4|nil
function DPanel:SetBackgroundColor(color)
	self.m_BackgroundColor = color
	self:InvalidatePaint()
end

---@return Vector4
function DPanel:GetBackgroundColor()
	return self.m_BackgroundColor or self:GetSkinBackgroundColor()
end

--- Defines the texture drawn as background, tinted by the background color
---@param texture Texture|nil
function DPanel:SetBackgroundImage(texture)
	self.m_BackgroundTexture = texture

	if self.m_Image then
		self.m_Image:SetTexture(texture)
	end
end

---@return Texture|nil
function DPanel:GetBackgroundImage()
	return self.m_BackgroundTexture
end

--- Returns the skin color matching the current state. Derived panels override it per state.
---@return Vector4
function DPanel:GetSkinBackgroundColor()
	return Skin.PanelBackground
end

--- Applies the background. Overrides must call DPanel.Paint(self, width, height) to keep it.
---@param width number
---@param height number
function DPanel:Paint(width, height)
	if not self.m_PaintBackground then
		if self.m_Image then
			self.m_Actor:RemoveImage()
			self.m_Image = nil
		end

		return
	end

	if not self.m_Image then
		self.m_Image = self.m_Actor:AddImage()
		self.m_Image:SetTexture(self.m_BackgroundTexture)
	end

	self.m_Image:SetTint(self:GetBackgroundColor())
end

return Registry.Register("DPanel", DPanel, "Panel")
