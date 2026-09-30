local Fonts = Resources.GetScript(":Libraries/UI/Core/Fonts.lua")
local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")
local Skin = Resources.GetScript(":Libraries/UI/Core/Skin.lua")

local DPanel = Resources.GetScript(":Libraries/UI/Panels/DPanel.lua")

--- Panel displaying a text, drawn by a Text component on the panel actor. No background by default.
---@class DLabel : DPanel
local DLabel = {}

-- SetContentAlignment follows the numeric keypad layout: 7 is top-left, 5 is centered, 3 is bottom-right
local kHorizontalAlignments = { "LEFT", "CENTER", "RIGHT", "LEFT", "CENTER", "RIGHT", "LEFT", "CENTER", "RIGHT" }
local kVerticalAlignments = { "BOTTOM", "BOTTOM", "BOTTOM", "CENTER", "CENTER", "CENTER", "TOP", "TOP", "TOP" }

function DLabel:Init()
	self:SetPaintBackground(false)
	self:SetMouseInputEnabled(false)

	self.m_Text = ""
	self.m_Font = "Default"
	self.m_TextColor = nil
	self.m_ContentAlignment = 4
	self.m_TextComponent = self.m_Actor:AddText()

	self:SetFont(self.m_Font)
	self:SetContentAlignment(self.m_ContentAlignment)

	-- Not RefreshText: derived classes override GetDisplayedText and aren't initialized yet
	self.m_TextComponent:SetText(self.m_Text)
end

---@param text string
function DLabel:SetText(text)
	text = tostring(text)

	if self.m_Text == text then
		return
	end

	self.m_Text = text
	self:RefreshText()
end

---@return string
function DLabel:GetText()
	return self.m_Text
end

--- Returns the text shown by the Text component. Derived panels override it to decorate the text.
---@return string
function DLabel:GetDisplayedText()
	return self.m_Text
end

--- Pushes the displayed text to the Text component
function DLabel:RefreshText()
	self.m_TextComponent:SetText(self:GetDisplayedText())
end

--- Uses a font created with UI.CreateFont
---@param name string
function DLabel:SetFont(name)
	assert(Fonts.Exists(name), "UI: unknown font '" .. tostring(name) .. "'")

	local font = Fonts.Get(name)

	self.m_Font = name
	self.m_TextComponent:SetFontPath(font.path)
	self.m_TextComponent:SetFontSize(font.size)
end

---@return string
function DLabel:GetFont()
	return self.m_Font
end

--- Overrides the skin text color, or restores it when nil
---@param color Vector4|nil
function DLabel:SetTextColor(color)
	self.m_TextColor = color
	self:InvalidatePaint()
end

---@return Vector4
function DLabel:GetTextColor()
	return self.m_TextColor or self:GetSkinTextColor()
end

--- Returns the skin text color matching the current state
---@return Vector4
function DLabel:GetSkinTextColor()
	return self.m_Enabled and Skin.Text or Skin.TextDisabled
end

--- Aligns the text inside the panel, following the numeric keypad layout (1 to 9)
---@param alignment integer
function DLabel:SetContentAlignment(alignment)
	assert(kHorizontalAlignments[alignment], "UI: the content alignment must be an integer from 1 to 9")

	self.m_ContentAlignment = alignment
	self.m_TextComponent:SetHorizontalAlignment(TextHorizontalAlignment[kHorizontalAlignments[alignment]])
	self.m_TextComponent:SetVerticalAlignment(TextVerticalAlignment[kVerticalAlignments[alignment]])
end

---@return integer
function DLabel:GetContentAlignment()
	return self.m_ContentAlignment
end

---@param width number
---@param height number
function DLabel:Paint(width, height)
	DPanel.Paint(self, width, height)
	self.m_TextComponent:SetColor(self:GetTextColor())
end

return Registry.Register("DLabel", DLabel, "DPanel")
