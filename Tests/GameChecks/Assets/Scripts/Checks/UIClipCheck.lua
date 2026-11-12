-- UI elements that clip their children, from Lua: the property, a scroll panel of the UI library that clips its content,
-- and, drawn for a screenshot (--frames 4 --screenshot), a panel with children running past its edges, a clip inside a
-- clip, a turned panel and a world space canvas. Each overflowing child is red: none of it may show outside its grey
-- panel
local Check = require("Scripts/Checks/Check")
local UI = Resources.GetScript(":Libraries/UI/Init.lua")

local UIClipCheck = {}
local oCheck = Check.New("UIClip")
local nFrame = 0
local oPanel, oPlain, oScroll

local kFont = ":Fonts/Roboto-Regular.ttf"

local function AddElement(oParent, sName, vPosition, vSize, vTint)
	local oActor = Scenes.GetCurrentScene():CreateActor(sName, "")
	local oTransform = oActor:GetTransform()
	oTransform:EnableUIData()
	oTransform:SetUIAnchorPreset(AnchorPreset.TOP_LEFT)
	oTransform:SetUIPivot(Vector2.new(-1, -1))
	oTransform:SetUIPosition(vPosition)
	oTransform:SetUISize(vSize)
	oActor:SetParent(oParent)

	if vTint then
		oActor:AddImage():SetTint(vTint)
	end

	return oActor
end

local kGrey = Vector4.new(0.35, 0.35, 0.35, 1)
local kRed = Vector4.new(0.9, 0.15, 0.15, 1)
local kBlue = Vector4.new(0.2, 0.4, 0.9, 1)

function UIClipCheck:OnStart()
	local oScene = Scenes.GetCurrentScene()
	local oCamera = oScene:CreateActor("Camera", "")
	oCamera:AddCamera()

	local oCanvas = oScene:CreateActor("Canvas", "")
	oCanvas:AddCanvas()

	-- A clipping panel, with a red child running past its right and bottom edges, and a text past its right edge
	oPanel = AddElement(oCanvas, "Panel", Vector2.new(60, -60), Vector2.new(500, 300), kGrey)
	oPanel:GetTransform():SetUIClipChildren(true)
	AddElement(oPanel, "Overflow", Vector2.new(350, -200), Vector2.new(300, 200), kRed)

	local oLine = AddElement(oPanel, "Line", Vector2.new(20, -20), Vector2.new(0, 0))
	local oText = oLine:AddText()
	oText:SetFontPath(kFont)
	oText:SetFontSize(40)
	oText:SetText("This line runs past the panel's right edge")

	-- A clip inside the clip: blue, its red child cut by both
	local oInner = AddElement(oPanel, "Inner", Vector2.new(20, -120), Vector2.new(200, 120), kBlue)
	oInner:GetTransform():SetUIClipChildren(true)
	AddElement(oInner, "InnerOverflow", Vector2.new(120, -60), Vector2.new(200, 200), kRed)

	-- The same, not clipping: its red child shows past it
	oPlain = AddElement(oCanvas, "Plain", Vector2.new(60, -420), Vector2.new(300, 200), kGrey)
	AddElement(oPlain, "PlainOverflow", Vector2.new(200, -100), Vector2.new(250, 150), kRed)

	-- A turned clipping panel
	local oTurned = AddElement(oCanvas, "Turned", Vector2.new(700, -420), Vector2.new(300, 200), kGrey)
	oTurned:GetTransform():SetUIRotation(20)
	oTurned:GetTransform():SetUIClipChildren(true)
	AddElement(oTurned, "TurnedOverflow", Vector2.new(200, -100), Vector2.new(250, 150), kRed)

	-- A world space canvas in front of the camera, with a clipping panel
	local oPlate = oScene:CreateActor("Plate", "")
	local oWorld = oPlate:AddCanvas()
	oWorld:SetRenderMode(CanvasRenderMode.WORLD_SPACE)
	oWorld:SetReferenceResolution(Vector2.new(400, 300))
	oWorld:SetWorldScale(0.004)
	oPlate:GetTransform():SetWorldPosition(Vector3.new(-1.2, -0.5, 4))
	oPlate:GetTransform():SetWorldRotation(Quaternion.new(Vector3.new(0, 200, 0)))
	local oWorldPanel = AddElement(oPlate, "WorldPanel", Vector2.new(20, -20), Vector2.new(250, 150), kGrey)
	oWorldPanel:GetTransform():SetUIClipChildren(true)
	AddElement(oWorldPanel, "WorldOverflow", Vector2.new(150, -80), Vector2.new(200, 150), kRed)

	-- A scroll panel of the UI library clips its content
	local oUI = UI.CreateController()
	oScroll = oUI:Create("DScrollPanel")
	oScroll:SetPos(1300, 60)
	oScroll:SetSize(300, 300)

	-- An item half out of the view, at the bottom
	local oItem = oScroll:Add("DPanel")
	oItem:SetPos(20, 220)
	oItem:SetSize(240, 200)
	oItem:SetBackgroundColor(kRed)
end

function UIClipCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1

	if nFrame ~= 5 then return end

	oCheck:Expect(oPanel:GetTransform():GetUIClipChildren(), "the panel clips its children")
	oCheck:Expect(not oPlain:GetTransform():GetUIClipChildren(), "an element doesn't clip by default")
	oPlain:GetTransform():SetUIClipChildren(true)
	oCheck:Expect(oPlain:GetTransform():GetUIClipChildren(), "set from Lua")
	oPlain:GetTransform():SetUIClipChildren(false)

	oCheck:Expect(oScroll:GetCanvas():GetParent() == oScroll and oScroll.m_Transform:GetUIClipChildren(), "a scroll panel clips its content")

	local nMajor, nMinor = Application.GetApiVersion()
	oCheck:Expect(nMajor == 1 and nMinor >= 9, "the Lua API is 1.9 or a later 1.x: " .. nMajor .. "." .. nMinor)
	oCheck:Finish()
end

return UIClipCheck
