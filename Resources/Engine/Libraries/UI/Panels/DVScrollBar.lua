local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")
local Skin = Resources.GetScript(":Libraries/UI/Core/Skin.lua")

local DButton = Resources.GetScript(":Libraries/UI/Panels/DButton.lua")

Resources.GetScript(":Libraries/UI/Panels/DPanel.lua")

--- Vertical scroll bar made of a track and a draggable DButton grip
---@class DVScrollBar : DPanel
local DVScrollBar = {}

local kMinimumGripHeight = 16
local kWheelStep = 40

local function GetGripHeight(self)
	if self.m_ContentSize <= self.m_ViewSize or self.m_ContentSize <= 0 then
		return self.m_Height
	end

	return math.min(self.m_Height, math.max(kMinimumGripHeight, self.m_Height * self.m_ViewSize / self.m_ContentSize))
end

local function GripPositionToScroll(self, gripY)
	local track = self.m_Height - GetGripHeight(self)

	if track <= 0 then
		return 0
	end

	return gripY / track * self:GetScrollRange()
end

function DVScrollBar:Init()
	self.m_ViewSize = 0
	self.m_ContentSize = 0
	self.m_Scroll = 0
	self.m_DragOffset = nil

	local bar = self
	local grip = self:Add("DButton")

	grip.OnMousePressed = function(panel, button)
		DButton.OnMousePressed(panel, button)

		if button == MouseButton.BUTTON_LEFT then
			local _, y = panel:LocalCursorPos()
			bar.m_DragOffset = y
		end
	end

	grip.OnCursorMoved = function(panel, x, y)
		if bar.m_DragOffset and panel:IsDown() then
			local _, barY = bar:LocalCursorPos()
			bar:SetScroll(GripPositionToScroll(bar, barY - bar.m_DragOffset))
		end
	end

	self.m_Grip = grip
end

--- Defines the visible size and the full content size, in canvas units
---@param viewSize number
---@param contentSize number
function DVScrollBar:SetUp(viewSize, contentSize)
	self.m_ViewSize = math.max(viewSize, 0)
	self.m_ContentSize = math.max(contentSize, 0)
	self:SetScroll(self.m_Scroll)
	self:InvalidateLayout()
end

--- Returns the maximum scroll value
---@return number
function DVScrollBar:GetScrollRange()
	return math.max(self.m_ContentSize - self.m_ViewSize, 0)
end

---@param scroll number
function DVScrollBar:SetScroll(scroll)
	scroll = math.max(0, math.min(scroll, self:GetScrollRange()))

	if self.m_Scroll == scroll then
		return
	end

	self.m_Scroll = scroll
	self:InvalidateLayout()
	self:OnScrollChanged(scroll)
end

---@return number
function DVScrollBar:GetScroll()
	return self.m_Scroll
end

---@param delta number
function DVScrollBar:AddScroll(delta)
	self:SetScroll(self.m_Scroll + delta)
end

--- Called when the scroll value changes
---@param scroll number
function DVScrollBar:OnScrollChanged(scroll) end

--- Clicking the track scrolls by one page towards the cursor
---@param button MouseButton
function DVScrollBar:OnMousePressed(button)
	if button ~= MouseButton.BUTTON_LEFT then
		return
	end

	local _, y = self:LocalCursorPos()
	local _, gripY = self.m_Grip:GetPos()

	self:AddScroll(y < gripY and -self.m_ViewSize or self.m_ViewSize)
end

---@param delta number
---@return boolean
function DVScrollBar:OnMouseWheeled(delta)
	self:AddScroll(-delta * kWheelStep)
	return true
end

---@param width number
---@param height number
function DVScrollBar:PerformLayout(width, height)
	local range = self:GetScrollRange()
	local gripHeight = GetGripHeight(self)
	local gripY = range > 0 and (height - gripHeight) * self.m_Scroll / range or 0

	self.m_Grip:SetVisible(range > 0)
	self.m_Grip:SetPos(0, gripY)
	self.m_Grip:SetSize(width, gripHeight)
end

---@return Vector4
function DVScrollBar:GetSkinBackgroundColor()
	return Skin.ScrollBarBackground
end

return Registry.Register("DVScrollBar", DVScrollBar, "DPanel")
