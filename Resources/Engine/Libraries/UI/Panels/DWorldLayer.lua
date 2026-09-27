local Array = Resources.GetScript(":Libraries/UI/Core/Array.lua")
local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")

Resources.GetScript(":Libraries/UI/Panels/Panel.lua")

--- Container covering its parent that draws its world panels from the farthest to the nearest. UI is
--- drawn over the scene without depth test, so this is the only depth ordering between them. It sets
--- the Z position of its children, and doesn't receive the mouse.
---@class DWorldLayer : Panel
local DWorldLayer = {}

-- Depth difference ignored when comparing children, so close depths don't swap every frame
local kDepthTolerance = 0.01

-- Children without depth (hidden, or not a world panel) are ordered as the nearest
local function GetDepthKey(panel)
	return panel.GetDepth and panel:GetDepth() or 0
end

function DWorldLayer:Init()
	self:SetMouseInputEnabled(false)
end

--- Covers the parent, so the hit-test reaches the world panels wherever they are placed
---@package
function DWorldLayer:UpdateWorld()
	local width, height = self.m_Parent:GetSize()

	self:SetDrivenPos(0, 0)
	self:SetSize(width, height)
end

function DWorldLayer:Think(deltaTime)
	local children = self.m_Children
	local sorted = true

	for i = 2, #children do
		if GetDepthKey(children[i]) > GetDepthKey(children[i - 1]) + kDepthTolerance then
			sorted = false
			break
		end
	end

	if sorted then
		return
	end

	local ordered = Array.Copy(children)
	Array.StableSortBy(ordered, function(child) return -GetDepthKey(child) end)

	for i = 1, #ordered do
		ordered[i].m_ZPos = i
	end

	self:ApplyChildrenZPos()
end

return Registry.Register("DWorldLayer", DWorldLayer, "Panel")
