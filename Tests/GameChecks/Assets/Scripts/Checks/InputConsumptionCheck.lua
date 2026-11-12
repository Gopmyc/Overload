-- Consuming inputs from Lua: the calls and their refusals, the reads that see what is consumed, and the keyboard's
-- capture, set from an Events.AddUpdate callback, which runs before this Behaviour. No device is held while the checks
-- run, so nothing is there to hide: the tests of OvTests cover what consumption hides
local Check = require("Scripts/Checks/Check")

local InputConsumptionCheck = {}
local oCheck = Check.New("InputConsumption")
local nFrame = 0
local bCapturedFirst = nil

function InputConsumptionCheck:OnAwake()
	Events.AddUpdate(function()
		if nFrame + 1 == 3 then
			Inputs.SetKeyboardCaptured(true)
			Inputs.ConsumeMouseScroll()
		elseif nFrame + 1 == 4 then
			bCapturedFirst = Inputs.IsKeyboardCaptured()
			Inputs.SetKeyboardCaptured(false)
		end
	end, self.owner)
end

function InputConsumptionCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1

	if nFrame == 3 then
		oCheck:Expect(Inputs.IsKeyboardCaptured(), "captured by the callback before the Behaviours")
		oCheck:Expect(not Inputs.GetAction("Jump"), "Jump released while the keyboard is captured")
		oCheck:Expect(not Inputs.GetKey(Key.SPACE), "SPACE up while captured")
		oCheck:Expect(not Inputs.GetKey(Key.SPACE, true), "SPACE up for what captured it too")
		oCheck:Equal(Inputs.GetMouseScroll().y, 0, "no scroll once consumed")
		oCheck:Equal(Inputs.GetMouseScroll(true).y, 0, "no scroll to see through")

		-- Nothing is held: there is nothing to hide
		local bHidden, sWhy = Inputs.Consume("MOUSE_LEFT")
		oCheck:Expect(bHidden == false and sWhy == nil, "a button up is not consumed")
		oCheck:Expect(Inputs.Consume("W"), "a key is hidden while the keyboard is captured")
		oCheck:Expect(Inputs.Consume("GAMEPAD_A") == false, "no pad, nothing to consume")
		oCheck:Expect(Inputs.Consume("GAMEPAD_LEFT_X+", 5) == false, "no pad 5")
		bHidden, sWhy = Inputs.Consume("NOPE")
		oCheck:Expect(bHidden == false and sWhy ~= nil and sWhy:find("no input named") ~= nil, "unknown input refused: " .. tostring(sWhy))

		-- The reads that see what is consumed take their usual arguments first
		oCheck:Expect(not Inputs.GetKeyDown(Key.E, true) and not Inputs.GetKeyUp(Key.E, true), "a key's edges, consumed included")
		oCheck:Expect(not Inputs.GetMouseButton(MouseButton.BUTTON_LEFT, true), "a mouse button, consumed included")
		oCheck:Expect(not Inputs.GetMouseButtonDown(MouseButton.BUTTON_LEFT, true) and not Inputs.GetMouseButtonUp(MouseButton.BUTTON_LEFT, true), "a mouse button's edges, consumed included")
		oCheck:Expect(not Inputs.GetGamepadButton(GamepadButton.A, nil, true), "a pad button, consumed included")
		oCheck:Expect(not Inputs.GetGamepadButtonDown(GamepadButton.A, 1, true) and not Inputs.GetGamepadButtonUp(GamepadButton.A, 1, true), "a pad button's edges, consumed included")
		oCheck:Equal(Inputs.GetGamepadAxis(GamepadAxis.LEFT_X, nil, true), 0, "a pad axis, consumed included")
	elseif nFrame == 4 then
		oCheck:Expect(bCapturedFirst == true, "the capture lasts from frame to frame")
		oCheck:Expect(not Inputs.IsKeyboardCaptured(), "freed by the callback")
		oCheck:Expect(Inputs.Consume("W") == false, "a key up is not consumed once the keyboard is free")
		local nMajor, nMinor = Application.GetApiVersion()
		oCheck:Expect(nMajor == 1 and nMinor >= 6, "the Lua API is 1.6 or a later 1.x: " .. nMajor .. "." .. nMinor)
		oCheck:Finish()
	end
end

return InputConsumptionCheck
