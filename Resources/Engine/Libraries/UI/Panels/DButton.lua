local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")
local Skin = Resources.GetScript(":Libraries/UI/Core/Skin.lua")

Resources.GetScript(":Libraries/UI/Panels/DLabel.lua")

--- Clickable label with a background reflecting its state
---@class DButton : DLabel
local DButton = {}

function DButton:Init()
	self:SetPaintBackground(true)
	self:SetMouseInputEnabled(true)
	self:SetContentAlignment(5)
	self:SetCursor(CursorShape.HAND)
	self.m_Depressed = false
end

--- Returns true while the left mouse button, pressed on the button, is held
---@return boolean
function DButton:IsDown()
	return self.m_Depressed
end

---@param button MouseButton
function DButton:OnMousePressed(button)
	if button == MouseButton.BUTTON_LEFT then
		self.m_Depressed = true
		self:InvalidatePaint()
	end
end

---@param button MouseButton
function DButton:OnMouseReleased(button)
	local wasDepressed = self.m_Depressed

	if button == MouseButton.BUTTON_LEFT then
		self.m_Depressed = false
		self:InvalidatePaint()
	end

	if not self:IsHovered() then
		return
	end

	if button == MouseButton.BUTTON_LEFT and wasDepressed then
		self:DoClick()
	elseif button == MouseButton.BUTTON_RIGHT then
		self:DoRightClick()
	end
end

--- Releases the button when the mouse release was never received (panel hidden or disabled while
--- pressed). Overrides must call DButton.Think(self, deltaTime).
---@param deltaTime number
function DButton:Think(deltaTime)
	if self.m_Depressed and not Inputs.GetMouseButton(MouseButton.BUTTON_LEFT) then
		self.m_Depressed = false
		self:InvalidatePaint()
	end
end

--- Called when the button is left clicked
function DButton:DoClick() end

--- Called when the button is right clicked
function DButton:DoRightClick() end

---@return Vector4
function DButton:GetSkinBackgroundColor()
	if not self.m_Enabled then
		return Skin.ButtonDisabled
	end

	if self.m_Depressed then
		return Skin.ButtonDown
	end

	if self:IsHovered() then
		return Skin.ButtonHovered
	end

	return Skin.ButtonNormal
end

return Registry.Register("DButton", DButton, "DLabel")
