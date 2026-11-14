-- Text and images lit as the scene is under a world space canvas: the API of the components and of the UI library, and,
-- held for a screenshot (--frames 20 --screenshot), two signs side by side. The one on the right of the picture stands
-- in the sun; the one on the left in the shadow of a box above the view, with a weak lamp in front of it. Each sign has
-- a lit background, a lit text ("LIT", top) and an unlit one ("PLAIN", bottom): the lit text is bright in the sun and dim
-- by the lamp, the plain text the same on both. A small box stands in front of the sunlit "LIT", whose "I" it hides
local Check = require("Scripts/Checks/Check")
local UI = Resources.GetScript(":Libraries/UI/Init.lua")

local LitWorldUICheck = {}
local oCheck = Check.New("LitWorldUI")
local nFrame = 0
local t = {}

local kFont = ":Fonts/Roboto-Regular.ttf"
local kWhite = Vector4.new(1, 1, 1, 1)
local kGrey = Vector4.new(0.6, 0.6, 0.6, 1)

local function AddElement(oParent, sName, vPosition, vSize)
	local oActor = Scenes.GetCurrentScene():CreateActor(sName, "")
	local oTransform = oActor:GetTransform()
	oTransform:EnableUIData()
	oTransform:SetUIAnchorPreset(AnchorPreset.TOP_LEFT)
	oTransform:SetUIPivot(Vector2.new(-1, -1))
	oTransform:SetUIPosition(vPosition)
	oTransform:SetUISize(vSize)
	oActor:SetParent(oParent)
	return oActor
end

local function AddText(oParent, sName, vPosition, sText, bLit)
	local oText = AddElement(oParent, sName, vPosition, Vector2.new(360, 110)):AddText()
	oText:SetFontPath(kFont)
	oText:SetFontSize(96)
	oText:SetColor(kWhite)
	oText:SetText(sText)
	oText:SetLit(bLit)
	return oText
end

-- A sign of 1.6 by 1.2 metres facing the camera: a lit grey background, a lit text above an unlit one
local function AddSign(sName, vPosition)
	local oSign = Scenes.GetCurrentScene():CreateActor(sName, "")
	local oCanvas = oSign:AddCanvas()
	oCanvas:SetRenderMode(CanvasRenderMode.WORLD_SPACE)
	oCanvas:SetReferenceResolution(Vector2.new(400, 300))
	oCanvas:SetWorldScale(0.004)
	oSign:GetTransform():SetWorldPosition(vPosition)
	oSign:GetTransform():SetWorldRotation(Quaternion.new(Vector3.new(0, 180, 0)))

	local oBackgroundActor = AddElement(oSign, sName .. "Background", Vector2.new(0, 0), Vector2.new(400, 300))
	local oBackground = oBackgroundActor:AddImage()
	oBackground:SetTint(kGrey)
	oBackground:SetLit(true)

	return {
		oBackground = oBackground,
		oLit = AddText(oBackgroundActor, sName .. "Lit", Vector2.new(30, -20), "LIT", true),
		oPlain = AddText(oBackgroundActor, sName .. "Plain", Vector2.new(30, -160), "PLAIN", false)
	}
end

local function AddBox(sName, vPosition, vScale)
	local oBox = Scenes.GetCurrentScene():CreateActor(sName, "")
	oBox:GetTransform():SetWorldPosition(vPosition)
	oBox:GetTransform():SetLocalScale(vScale)
	oBox:AddModelRenderer():SetModel(Resources.GetModel(":Models/Cube.fbx"))
	oBox:AddMaterialRenderer():SetMaterial(0, Resources.GetMaterial(":Materials/Default.ovmat"))
	return oBox
end

function LitWorldUICheck:OnStart()
	local oScene = Scenes.GetCurrentScene()
	oScene:CreateActor("Camera", ""):AddCamera()

	-- The sun comes from above the camera, at 60 degrees, and casts shadows
	local oSun = oScene:CreateActor("Sun", "")
	oSun:GetTransform():SetLocalRotation(Quaternion.new(Vector3.new(60, 0, 0)))
	local oLight = oSun:AddDirectionalLight()
	oLight:SetIntensity(3)
	oLight:SetCastShadow(true)
	oLight:SetShadowAreaSize(20)

	local oAmbient = oScene:CreateActor("Ambient", ""):AddAmbientSphereLight()
	oAmbient:SetRadius(50)
	oAmbient:SetIntensity(0.15)

	t.tSun = AddSign("SunSign", Vector3.new(-0.95, 0, 4))
	t.tShade = AddSign("ShadeSign", Vector3.new(0.95, 0, 4))

	-- Above the view, between the sun and the right sign: its shadow covers the sign
	AddBox("Roof", Vector3.new(0.95, 2.6, 2.5), Vector3.new(1.7, 0.4, 1.6))

	-- A weak lamp in front of the shaded sign
	local oLamp = oScene:CreateActor("Lamp", "")
	oLamp:GetTransform():SetWorldPosition(Vector3.new(0.95, 0, 3))
	oLamp:AddPointLight():SetIntensity(0.5)

	-- In front of the sunlit sign's lit text (the camera sees +X on its left)
	AddBox("Post", Vector3.new(-0.44, 0.24, 3.2), Vector3.new(0.12, 0.12, 0.12))

	-- The UI library: a lit label, and a panel whose background is lit once painted
	t.oUI = UI.CreateController()
	t.oLabel = t.oUI:Create("DLabel")
	t.oPanel = t.oUI:Create("DPanel")
end

function LitWorldUICheck:OnUpdate()
	nFrame = nFrame + 1

	if nFrame == 3 then
		local nMajor, nMinor = Application.GetApiVersion()
		oCheck:Expect(nMajor == 1 and nMinor >= 20, "the Lua API is 1.20 or a later 1.x: " .. nMajor .. "." .. nMinor)

		-- The components
		oCheck:Expect(t.tSun.oLit:IsLit(), "a lit text")
		oCheck:Expect(not t.tSun.oPlain:IsLit(), "an unlit text")
		oCheck:Expect(t.tSun.oBackground:IsLit(), "a lit image")

		local oScene = Scenes.GetCurrentScene()
		local oFresh = oScene:CreateActor("Fresh", "")
		local oFreshText = oFresh:AddText()
		local oFreshImage = oFresh:AddImage()
		oCheck:Expect(not oFreshText:IsLit() and not oFreshImage:IsLit(), "a new text or image is unlit")
		oFreshText:SetLit(true)
		oFreshImage:SetLit(true)
		oCheck:Expect(oFreshText:IsLit() and oFreshImage:IsLit(), "lit from Lua")
		oFreshText:SetLit(false)
		oCheck:Expect(not oFreshText:IsLit(), "unlit again")
		oFresh:Destroy()

		-- The UI library
		oCheck:Expect(not t.oLabel:IsLit(), "a label is unlit by default")
		t.oLabel:SetLit(true)
		oCheck:Expect(t.oLabel:IsLit() and t.oLabel.m_TextComponent:IsLit(), "a lit label lights its text")
		t.oLabel:SetLit(false)
		oCheck:Expect(not t.oLabel.m_TextComponent:IsLit(), "and unlights it")

		t.oPanel:SetLit(true)
		oCheck:Expect(t.oPanel:IsLit(), "a lit panel")
	elseif nFrame == 5 then
		-- Its background is created when the panel is painted, lit then
		oCheck:Expect(t.oPanel.m_Image ~= nil and t.oPanel.m_Image:IsLit(), "a lit panel's background is lit")
		t.oPanel:SetLit(false)
		oCheck:Expect(t.oPanel.m_Image ~= nil and not t.oPanel.m_Image:IsLit(), "and unlit with it")
	elseif nFrame == 25 then
		oCheck:Finish()
	end
end

return LitWorldUICheck
