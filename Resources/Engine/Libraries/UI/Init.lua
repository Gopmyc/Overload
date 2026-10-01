--- Entry point of the UI library, a panel system modeled after Garry's Mod Derma.
---
--- local UI = Resources.GetScript(":Libraries/UI/Init.lua")
---
--- function Menu:OnStart()
---     self.ui = UI.CreateController()
---
---     local button = self.ui:Create("DButton")
---     button:SetPos(20, 20)
---     button:SetSize(160, 40)
---     button:SetText("Play")
---     button.DoClick = function() Debug.Log("Play") end
--- end
---
--- function Menu:OnUpdate(deltaTime)
---     self.ui:Update(deltaTime)
--- end
---
--- function Menu:OnDestroy() if self.ui then self.ui:Destroy() end end
---
--- Panels can also follow the 3D world (see Docs/WorldSpace.md):
---
--- self.ui:SetCamera(Scenes.GetCurrentScene():FindActorByName("Main Camera"))
--- local layer = self.ui:Create("DWorldLayer")
--- local bar = layer:Add("DWorldArc")
--- bar:SetTarget(self.owner, Vector3.new(0, 1.2, 0))
--- bar:SetRadius(0.6)
--- bar:SetArc(0, 120)
--- bar:SetFaceCamera(true)
--- bar:SetThickness(0.05, true)
--- bar:SetBackgroundColor(Vector4.new(0, 0, 0, 0.5))
--- bar:SetFraction(0.75)

local Controller = Resources.GetScript(":Libraries/UI/Core/Controller.lua")
local Fonts = Resources.GetScript(":Libraries/UI/Core/Fonts.lua")
local Registry = Resources.GetScript(":Libraries/UI/Core/Registry.lua")
local Skin = Resources.GetScript(":Libraries/UI/Core/Skin.lua")

-- Loading the built-in panels registers them
Resources.GetScript(":Libraries/UI/Panels/Panel.lua")
Resources.GetScript(":Libraries/UI/Panels/DPanel.lua")
Resources.GetScript(":Libraries/UI/Panels/DLabel.lua")
Resources.GetScript(":Libraries/UI/Panels/DButton.lua")
Resources.GetScript(":Libraries/UI/Panels/DTextEntry.lua")
Resources.GetScript(":Libraries/UI/Panels/DVScrollBar.lua")
Resources.GetScript(":Libraries/UI/Panels/DScrollPanel.lua")
Resources.GetScript(":Libraries/UI/Panels/DPolyline.lua")
Resources.GetScript(":Libraries/UI/Panels/DWorldPanel.lua")
Resources.GetScript(":Libraries/UI/Panels/DWorldPath.lua")
Resources.GetScript(":Libraries/UI/Panels/DWorldArc.lua")
Resources.GetScript(":Libraries/UI/Panels/DWorldLayer.lua")
Resources.GetScript(":Libraries/UI/Panels/DModelPanel.lua")

---@class UI
local UI = {}

--- Creates a controller on the Canvas of the given actor, or on a new canvas actor when nil
---@param canvasActor Actor|nil
---@return UIController
function UI.CreateController(canvasActor)
	return Controller.New(canvasActor)
end

--- Creates a panel of the given class as a child of an existing panel, like vgui.Create.
--- Top-level panels are created with controller:Create.
---@param className string
---@param parent Panel
---@return Panel
function UI.Create(className, parent)
	assert(type(parent) == "table" and parent.IsValid, "UI: UI.Create needs a parent panel, use controller:Create for top-level panels")
	assert(parent:IsValid(), "UI: UI.Create was given a removed parent panel")

	return parent:Add(className)
end

--- Registers a panel class deriving from a registered class, like vgui.Register
---@param className string
---@param class table
---@param baseName string|nil
---@return table
function UI.Register(className, class, baseName)
	return Registry.Register(className, class, baseName)
end

--- Returns the class table of a registered panel, to call base implementations from overrides
---@param className string
---@return table|nil
function UI.GetControlTable(className)
	return Registry.Get(className)
end

--- Registers a named font usable with SetFont, like surface.CreateFont
---@param name string
---@param data { path: string|nil, size: number|nil }
function UI.CreateFont(name, data)
	Fonts.Create(name, data)
end

--- Default colors used by the built-in panels
UI.Skin = Skin

return UI
