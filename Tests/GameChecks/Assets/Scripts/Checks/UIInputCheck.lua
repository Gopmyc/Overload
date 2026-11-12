-- The UI library with the engine's input: the controller updates itself once a frame, after every OnLateUpdate, and a
-- manual Update from older code changes nothing; the keyboard is captured while a text entry has the focus, freed when
-- it loses it or its controller goes, whatever other controllers do; a text entry takes typed characters in UTF-8. No
-- device is held while the checks run
local Check = require("Scripts/Checks/Check")
local UI = Resources.GetScript(":Libraries/UI/Init.lua")

local UIInputCheck = {}
local oCheck = Check.New("UIInput")
local nFrame = 0
local nThinks = 0
local nThinksAtFrame3 = 0
local oUI, oEntry

function UIInputCheck:OnStart()
	oUI = UI.CreateController()

	local oProbe = oUI:Create("Panel")
	oProbe.Think = function() nThinks = nThinks + 1 end

	oEntry = oUI:Create("DTextEntry")
	oEntry:SetSize(200, 30)
end

function UIInputCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1

	-- Code written for older versions still updates the controller from a Behaviour
	oUI:Update(nDelta)

	if nFrame == 3 then
		nThinksAtFrame3 = nThinks
	elseif nFrame == 6 then
		oCheck:Equal(nThinks - nThinksAtFrame3, 3, "one update a frame, the manual calls ignored")

		-- What the engine gives, with no keyboard
		oCheck:Equal(Inputs.GetTypedText(), "", "no text typed")
		oCheck:Equal(Inputs.GetTypedText(true), "", "no text typed, captured or not")
		oCheck:Equal(Inputs.GetKeyRepeatCount(Key.BACKSPACE), 0, "no repeat")
		oCheck:Equal(Inputs.GetKeyRepeatCount(Key.BACKSPACE, true), 0, "no repeat, consumed or not")

		-- A focused text entry captures the keyboard
		oCheck:Expect(not Inputs.IsKeyboardCaptured(), "the keyboard free with no focus")
		oEntry:RequestFocus()
		oCheck:Expect(oEntry:HasFocus() and Inputs.IsKeyboardCaptured(), "captured while the entry has the focus")

		oUI:SetConsumesInput(false)
		oCheck:Expect(not Inputs.IsKeyboardCaptured() and not oUI:GetConsumesInput(), "free once the controller consumes nothing")
		oUI:SetConsumesInput(true)
		oCheck:Expect(Inputs.IsKeyboardCaptured(), "captured again")

		-- Typed characters, one UTF-8 character at a time
		oEntry:SetText("h\u{E9}llo")
		oEntry:OnKeyCodePressed(Key.BACKSPACE)
		oCheck:Equal(oEntry:GetText(), "h\u{E9}ll", "Backspace removes one character")
		oEntry:OnKeyCodePressed(Key.LEFT)
		oEntry:OnKeyCodePressed(Key.LEFT)
		oEntry:OnKeyCodePressed(Key.BACKSPACE)
		oCheck:Equal(oEntry:GetText(), "hll", "a two-byte character is one character")
		oEntry:OnTextTyped("\u{416}")
		oCheck:Equal(oEntry:GetText(), "h\u{416}ll", "a typed character goes at the caret")

		oEntry.AllowInput = function(_, sCharacter) return sCharacter == "x" end
		oEntry:OnTextTyped("x")
		oCheck:Equal(oEntry:GetText(), "h\u{416}ll", "AllowInput can refuse a character")

		-- Another controller's focus keeps the keyboard captured until both let go
		local oOther = UI.CreateController()
		local oOtherEntry = oOther:Create("DTextEntry")
		oOtherEntry:RequestFocus()
		oEntry:KillFocus()
		oCheck:Expect(Inputs.IsKeyboardCaptured(), "captured while another controller has a focused entry")
		oOther:Destroy()
		oCheck:Expect(not Inputs.IsKeyboardCaptured(), "free once that controller is destroyed")

		-- Destroying the controller frees the keyboard
		oEntry:RequestFocus()
		oCheck:Expect(Inputs.IsKeyboardCaptured(), "captured once more")
		oUI:Destroy()
		oCheck:Expect(not Inputs.IsKeyboardCaptured(), "free once the controller is destroyed")

		local nMajor, nMinor = Application.GetApiVersion()
		oCheck:Expect(nMajor == 1 and nMinor >= 8, "the Lua API is 1.8 or a later 1.x: " .. nMajor .. "." .. nMinor)
		oCheck:Finish()
	end
end

return UIInputCheck
