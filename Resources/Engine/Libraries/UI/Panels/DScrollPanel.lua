local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")

local Panel = Resources.GetScript(":Libraries/UI/Panels/Panel.lua")

Resources.GetScript(":Libraries/UI/Panels/DPanel.lua")
Resources.GetScript(":Libraries/UI/Panels/DVScrollBar.lua")

--- Scrollable area: children are added to an inner canvas, moved by a vertical scroll bar.
--- Children entirely outside the view are hidden; partially visible ones aren't clipped, since the
--- engine has no UI clipping (see Docs/EngineLimitations.md).
---@class DScrollPanel : DPanel
local DScrollPanel = {}

local kScrollBarWidth = 12

function DScrollPanel:Init()
	local scrollPanel = self
	local canvas = Panel.Add(self, "Panel")
	local scrollBar = Panel.Add(self, "DVScrollBar")

	canvas:SetMouseInputEnabled(false)

	-- The content height depends on the canvas children, which invalidate the canvas when they change
	canvas.PerformLayout = function()
		scrollPanel:InvalidateLayout()
	end

	scrollBar.OnScrollChanged = function()
		scrollPanel:InvalidateLayout()
	end

	self.m_Canvas = canvas
	self.m_ScrollBar = scrollBar
end

--- Returns the panel holding the scrolled children
---@return Panel
function DScrollPanel:GetCanvas()
	return self.m_Canvas
end

---@return DVScrollBar
function DScrollPanel:GetVBar()
	return self.m_ScrollBar
end

--- Creates a panel of the given class inside the scrolled canvas
---@param className string
---@return Panel
function DScrollPanel:Add(className)
	return self.m_Canvas:Add(className)
end

--- Moves an existing panel inside the scrolled canvas
---@param panel Panel
function DScrollPanel:AddItem(panel)
	panel:SetParent(self.m_Canvas)
end

--- Scrolls only when there is something to scroll, otherwise lets the parents handle the wheel
---@param delta number
---@return boolean
function DScrollPanel:OnMouseWheeled(delta)
	if self.m_ScrollBar:GetScrollRange() <= 0 then
		return false
	end

	return self.m_ScrollBar:OnMouseWheeled(delta)
end

---@param width number
---@param height number
function DScrollPanel:PerformLayout(width, height)
	local canvas = self.m_Canvas
	local scrollBar = self.m_ScrollBar
	local children = canvas.m_Children
	local contentHeight = 0

	for i = 1, #children do
		local child = children[i]

		if child.m_Visible then
			contentHeight = math.max(contentHeight, child.m_Y + child.m_Height)
		end
	end

	scrollBar:SetUp(height, contentHeight)

	local showScrollBar = scrollBar:GetScrollRange() > 0
	local scroll = scrollBar:GetScroll()

	scrollBar:SetVisible(showScrollBar)
	scrollBar:SetPos(math.max(width - kScrollBarWidth, 0), 0)
	scrollBar:SetSize(math.min(kScrollBarWidth, width), height)

	canvas:SetPos(0, -scroll)
	canvas:SetSize(showScrollBar and math.max(width - kScrollBarWidth, 0) or width, math.max(contentHeight, height))

	for i = 1, #children do
		local child = children[i]
		local top = child.m_Y - scroll

		child:SetCulled(top >= height or top + child.m_Height <= 0)
	end
end

return Registry.Register("DScrollPanel", DScrollPanel, "DPanel")
