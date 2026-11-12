-- Text beyond Latin-1: UTF-8 strings in Cyrillic and Greek get their own glyphs, baked when first drawn, and a
-- character the font lacks is drawn as "?". Each line is also drawn, for a screenshot (--frames 4 --screenshot)
local Check = require("Scripts/Checks/Check")

local TextCheck = {}
local oCheck = Check.New("Text")
local nFrame = 0
local tTexts = {}

local kFont = ":Fonts/Roboto-Regular.ttf"

local function AddLine(oCanvas, nIndex, sText)
	local oActor = Scenes.GetCurrentScene():CreateActor("Line" .. nIndex, "")
	local oTransform = oActor:GetTransform()
	oTransform:EnableUIData()
	oTransform:SetUIAnchorPreset(AnchorPreset.TOP_LEFT)
	oTransform:SetUIPivot(Vector2.new(-1, -1))
	oTransform:SetUIPosition(Vector2.new(40, -(40 + (nIndex - 1) * 80)))
	oActor:SetParent(oCanvas)

	local oText = oActor:AddText()
	oText:SetFontPath(kFont)
	oText:SetFontSize(48)
	oText:SetText(sText)
	return oText
end

-- A text that is never drawn, to measure a string
local function Width(sText)
	local oText = tTexts.ruler
	oText:SetText(sText)
	return oText:GetContentSize().x
end

function TextCheck:OnAwake()
	local oScene = Scenes.GetCurrentScene()
	oScene:CreateActor("Camera", ""):AddCamera()

	local oCanvas = oScene:CreateActor("Canvas", "")
	oCanvas:AddCanvas()

	tTexts.latin = AddLine(oCanvas, 1, "Latin: H\u{E9}llo w\u{F6}rld")
	tTexts.cyrillic = AddLine(oCanvas, 2, "\u{416}\u{438}\u{442}\u{44C}: \u{41A}\u{438}\u{440}\u{438}\u{43B}\u{43B}\u{438}\u{446}\u{430}")
	tTexts.greek = AddLine(oCanvas, 3, "\u{3A9}\u{3BC}\u{3AD}\u{3B3}\u{3B1}: \u{395}\u{3BB}\u{3BB}\u{3B7}\u{3BD}\u{3B9}\u{3BA}\u{3AC}")
	tTexts.missing = AddLine(oCanvas, 4, "Missing: \u{4E2D}\u{6587}")

	local oRuler = oScene:CreateActor("Ruler", "")
	oRuler:GetTransform():EnableUIData()
	oRuler:SetParent(oCanvas)
	tTexts.ruler = oRuler:AddText()
	tTexts.ruler:SetFontPath(kFont)
	tTexts.ruler:SetFontSize(48)
	tTexts.ruler:SetText("")
end

function TextCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1

	if nFrame ~= 5 then return end

	-- Its own glyphs, not four question marks
	local nQuestionMarks = Width("????")
	local nZhit = Width("\u{416}\u{438}\u{442}\u{44C}")
	local nOmega = Width("\u{3A9}\u{3BC}\u{3AD}\u{3B3}")
	oCheck:Expect(nZhit > 0 and math.abs(nZhit - nQuestionMarks) > 1, "Cyrillic has its glyphs: " .. nZhit .. " wide, ???? " .. nQuestionMarks)
	oCheck:Expect(nOmega > 0 and math.abs(nOmega - nQuestionMarks) > 1, "Greek has its glyphs: " .. nOmega .. " wide")

	-- The font has no CJK: each is drawn as "?"
	oCheck:Near(Width("\u{4E2D}\u{6587}"), Width("??"), "a character the font lacks is drawn as ?", 0.01)

	-- Latin-1 as before, and a broken sequence drawn as the replacement character, which Roboto has
	oCheck:Near(Width("\u{E9}"), Width("e"), "\u{E9} as wide as e", 0.5)
	oCheck:Near(Width("a\128b"), Width("a\u{FFFD}b"), "a broken UTF-8 byte is drawn as U+FFFD", 0.01)
	oCheck:Equal(Width(""), 0, "an empty text takes no room")

	-- Two lines are taller than one
	tTexts.ruler:SetText("A\nB")
	local nTwoLines = tTexts.ruler:GetContentSize().y
	tTexts.ruler:SetText("A")
	oCheck:Expect(nTwoLines > tTexts.ruler:GetContentSize().y * 1.5, "two lines are taller than one")

	-- The drawn lines measure what their strings do
	oCheck:Expect(tTexts.cyrillic:GetContentSize().x > Width("\u{416}\u{438}\u{442}\u{44C}"), "a drawn Cyrillic line has its width")
	oCheck:Equal(tTexts.greek:GetText(), "\u{3A9}\u{3BC}\u{3AD}\u{3B3}\u{3B1}: \u{395}\u{3BB}\u{3BB}\u{3B7}\u{3BD}\u{3B9}\u{3BA}\u{3AC}", "the text reads back as set")

	local nMajor, nMinor = Application.GetApiVersion()
	oCheck:Expect(nMajor == 1 and nMinor >= 7, "the Lua API is 1.7 or a later 1.x: " .. nMajor .. "." .. nMinor)
	oCheck:Finish()
end

return TextCheck
