-- Translations from Lua, on the tables of Assets/Translations (en, fr, ru): the languages, texts with parameters and
-- plural forms, a missing text, a text of a component and of a label of the UI library following the language, the
-- font Russian stands for Roboto, and the player's choice saved. Its first frames hold Russian for a screenshot
-- (--frames 20 --screenshot): the title and the label in Liberation Serif. It ends in English again
local Check = require("Scripts/Checks/Check")
local UI = Resources.GetScript(":Libraries/UI/Init.lua")

local LocalizationCheck = {}
local oCheck = Check.New("Localization")
local nFrame = 0
local t = {}

local kRoboto = ":Fonts/Roboto-Regular.ttf"
local kSerif = "Fonts/LiberationSerif-Regular.ttf"

local function Fails(fCall)
	local bOk, sError = pcall(fCall)
	return not bOk, sError
end

local function Mentions(sText, sPart)
	return type(sText) == "string" and sText:find(sPart, 1, true) ~= nil
end

local function AddText(oCanvas, sName, nY)
	local oActor = Scenes.GetCurrentScene():CreateActor(sName, "")
	local oTransform = oActor:GetTransform()
	oTransform:EnableUIData()
	oTransform:SetUIAnchorPreset(AnchorPreset.TOP_LEFT)
	oTransform:SetUIPivot(Vector2.new(-1, -1))
	oTransform:SetUIPosition(Vector2.new(40, nY))
	oActor:SetParent(oCanvas)

	local oText = oActor:AddText()
	oText:SetFontPath(kRoboto)
	oText:SetFontSize(64)
	return oText
end

function LocalizationCheck:OnAwake()
	local oScene = Scenes.GetCurrentScene()
	oScene:CreateActor("Camera", ""):AddCamera()

	local oCanvas = oScene:CreateActor("Canvas", "")
	oCanvas:AddCanvas()

	t.oTitle = AddText(oCanvas, "Title", -40)
	t.oTitle:SetTextKey("check.title")
	t.oPlain = AddText(oCanvas, "Plain", -360)
	t.oPlain:SetText("Plain text")

	-- A label of the UI library, its text from a key and its parameters
	t.oUI = UI.CreateController()
	t.oLabel = t.oUI:Create("DLabel")
	t.oLabel:SetPos(40, 160)
	t.oLabel:SetSize(900, 100)
	t.oLabel:SetTextKey("check.apples", { count = 3 })

	t.bStartedIn = Localization.GetLanguage()
end

function LocalizationCheck:OnUpdate()
	nFrame = nFrame + 1

	if nFrame == 3 then
		local nMajor, nMinor = Application.GetApiVersion()
		oCheck:Expect(nMajor == 1 and nMinor >= 21, "the Lua API is 1.21 or a later 1.x: " .. nMajor .. "." .. nMinor)

		-- The project's languages; the runner starts in the default one, whatever the machine's
		local tLanguages = Localization.GetLanguages()
		oCheck:Equal(#tLanguages, 3, "three languages")
		oCheck:Equal(tLanguages[1] and tLanguages[1].code, "en", "by code, English first")
		oCheck:Equal(tLanguages[2] and tLanguages[2].name, "Français", "each named in itself")
		oCheck:Equal(Localization.GetDefaultLanguage(), "en", "the default language")
		oCheck:Equal(t.bStartedIn, "en", "started in the default language")

		-- English texts, parameters and plural forms
		oCheck:Equal(Localization.Get("check.title"), "Hello, world", "a text")
		oCheck:Equal(Localization.Get("check.greeting", { name = "Ada" }), "Hello, Ada!", "a parameter")
		oCheck:Equal(Localization.Get("check.apples", { count = 1 }), "1 apple", "one apple")
		oCheck:Equal(Localization.Get("check.apples", { count = 3 }), "3 apples", "three apples")
		oCheck:Equal(t.oTitle:GetTextKey(), "check.title", "the title's key")
		oCheck:Equal(t.oTitle:GetText(), "Hello, world", "the title in English")
		oCheck:Equal(t.oLabel:GetText(), "3 apples", "the label in English")
		oCheck:Equal(Localization.GetFont(kRoboto), kRoboto, "English keeps Roboto")

		-- French
		local nRevision = Localization.GetRevision()
		local bSet = Localization.SetLanguage("fr")
		oCheck:Expect(bSet and Localization.GetLanguage() == "fr", "French chosen")
		oCheck:Expect(Localization.GetRevision() > nRevision, "the revision grows")
		oCheck:Equal(t.oTitle:GetText(), "Bonjour, le monde", "the title follows")
		oCheck:Equal(Localization.Get("check.apples", { count = 0 }), "0 pomme", "French counts 0 as one")
		oCheck:Equal(Localization.Get("check.apples", { count = 1.5 }), "1.5 pomme", "and 1.5")
		oCheck:Equal(Localization.Get("check.apples", { count = 2 }), "2 pommes", "and 2 as more")
		oCheck:Equal(Localization.Get("check.greeting", { name = "Ada" }), "Bonjour, Ada !", "a French parameter")

		-- A text missing from French comes from English, one missing from both is its key
		oCheck:Equal(Localization.Get("check.only_english"), "Only in English", "a text from the default language")
		oCheck:Expect(Localization.Has("check.only_english"), "which it has")
		oCheck:Equal(Localization.Get("check.nowhere"), "check.nowhere", "a text in no table is its key")
		oCheck:Expect(not Localization.Has("check.nowhere"), "which it does not have")

		-- Refused
		local bRefused, sWhy = Localization.SetLanguage("de")
		oCheck:Expect(not bRefused and Mentions(sWhy, "no language 'de'"), "a language without a table: " .. tostring(sWhy))
		oCheck:Equal(Localization.GetLanguage(), "fr", "and the language stays")
		local bFailed, sError = Fails(function() Localization.Get("check.title", { name = {} }) end)
		oCheck:Expect(bFailed and Mentions(sError, "parameter 'name' must be a text"), "a parameter of the wrong kind: " .. tostring(sError))
		bFailed, sError = Fails(function() Localization.Get("check.title", { "unnamed" }) end)
		oCheck:Expect(bFailed and Mentions(sError, "must be named"), "an unnamed parameter: " .. tostring(sError))

		-- Russian, held for the screenshot
		Localization.SetLanguage("ru")
	elseif nFrame == 5 then
		oCheck:Equal(t.oTitle:GetText(), "Привет, мир", "the title in Russian")
		oCheck:Equal(t.oLabel:GetText(), "3 яблока", "the label follows the language")
		oCheck:Equal(Localization.Get("check.apples", { count = 1 }), "1 яблоко", "one apple in Russian")
		oCheck:Equal(Localization.Get("check.apples", { count = 5 }), "5 яблок", "five apples")
		oCheck:Equal(Localization.Get("check.apples", { count = 21 }), "21 яблоко", "twenty-one apples")
		oCheck:Equal(Localization.Get("check.apples", { count = 12 }), "12 яблок", "twelve apples")
		oCheck:Equal(Localization.GetFont(kRoboto), kSerif, "Russian stands Liberation Serif for Roboto")

		-- The player's choice is saved
		local sSaved = Storage.Read("language.json")
		oCheck:Expect(Mentions(sSaved, "\"ru\""), "the choice saved: " .. tostring(sSaved))

		-- A text set as it is leaves its key
		t.oPlain:SetTextKey("check.title")
		t.oPlain:SetText("Plain text")
		oCheck:Equal(t.oPlain:GetTextKey(), "", "a text set clears its key")
	elseif nFrame == 25 then
		-- English again, saved, so that another game (in the editor) starts the same
		Localization.SetLanguage(Localization.GetDefaultLanguage())
		oCheck:Finish()
	end
end

return LocalizationCheck
