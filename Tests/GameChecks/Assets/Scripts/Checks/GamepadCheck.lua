-- The gamepad API from Lua. The machines that run the checks have no gamepad: every read then says so, without error
local Check = require("Scripts/Checks/Check")

local GamepadCheck = {}
local oCheck = Check.New("Gamepad")
local nFrame = 0

function GamepadCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1

	if nFrame == 3 then
		oCheck:Equal(GamepadButton.A, 0, "GamepadButton.A")
		oCheck:Equal(GamepadButton.DPAD_LEFT, 14, "GamepadButton.DPAD_LEFT")
		oCheck:Equal(GamepadAxis.RIGHT_TRIGGER, 5, "GamepadAxis.RIGHT_TRIGGER")

		if not Inputs.IsGamepadConnected() then
			oCheck:Expect(not Inputs.IsGamepadConnected(1), "pad 1 not connected")
			oCheck:Equal(Inputs.GetGamepadName(), "", "no name without a pad")
			oCheck:Expect(not Inputs.GetGamepadButton(GamepadButton.A), "no button held without a pad")
			oCheck:Expect(not Inputs.GetGamepadButtonDown(GamepadButton.START, 2), "no button pressed on pad 2")
			oCheck:Expect(not Inputs.GetGamepadButtonUp(GamepadButton.B), "no button released without a pad")
			oCheck:Equal(Inputs.GetGamepadAxis(GamepadAxis.LEFT_X), 0, "no axis without a pad")
			oCheck:Equal(Inputs.GetGamepadAxis(GamepadAxis.LEFT_TRIGGER, 1), 0, "no trigger on pad 1")
		end

		-- A pad out of range is never there
		oCheck:Expect(not Inputs.IsGamepadConnected(9), "pad 9")
		oCheck:Expect(not Inputs.GetGamepadButton(GamepadButton.A, 0), "pad 0")
		oCheck:Equal(Inputs.GetGamepadAxis(GamepadAxis.RIGHT_Y, 5), 0, "pad 5")

		oCheck:Near(Inputs.GetGamepadDeadZone(), 0.2, "default dead zone")
		Inputs.SetGamepadDeadZone(0.3)
		oCheck:Near(Inputs.GetGamepadDeadZone(), 0.3, "dead zone set")
		Inputs.SetGamepadDeadZone(5)
		oCheck:Near(Inputs.GetGamepadDeadZone(), 0.95, "dead zone clamped")

		oCheck:Finish()
	end
end

return GamepadCheck
