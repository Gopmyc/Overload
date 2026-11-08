-- The project's input actions from Lua (Settings/InputActions.json): their order and bindings, rebinding with its
-- swaps and refusals, resets, the bindings saved in the save folder, and the labels a menu shows
local Check = require("Scripts/Checks/Check")

local InputActionsCheck = {}
local oCheck = Check.New("InputActions")
local nFrame = 0

local function Join(tValues)
	return tValues and table.concat(tValues, ",") or "nil"
end

function InputActionsCheck:OnUpdate(nDelta)
	nFrame = nFrame + 1

	if nFrame ~= 3 then return end

	oCheck:Equal(Join(Inputs.GetActionNames()), "Jump,Attack,MoveRight,Interact", "actions in the file's order")
	oCheck:Equal(Join(Inputs.GetBindings("Jump")), "SPACE,GAMEPAD_A,,", "Jump's default bindings")
	oCheck:Equal(Join(Inputs.GetBindings("MoveRight")), "D,RIGHT,GAMEPAD_LEFT_X+,", "MoveRight's default bindings")
	oCheck:Equal(Inputs.GetBindings("Fly"), nil, "no bindings for an action the project does not declare")

	-- Nothing is pressed while the checks run
	oCheck:Expect(not Inputs.GetAction("Jump"), "Jump released")
	oCheck:Expect(not Inputs.GetActionDown("Attack"), "Attack not pressed")
	oCheck:Expect(not Inputs.GetActionUp("Interact"), "Interact not released")
	oCheck:Equal(Inputs.GetActionValue("MoveRight"), 0, "MoveRight at rest")
	oCheck:Equal(Inputs.GetPressedInput(), nil, "no input pressed")

	-- E belongs to Interact: Interact gets back what Jump's first slot held
	oCheck:Expect(Inputs.SetBinding("Jump", 1, "E"), "E bound to Jump")
	oCheck:Equal(Join(Inputs.GetBindings("Jump")), "E,GAMEPAD_A,,", "Jump with E")
	oCheck:Equal(Join(Inputs.GetBindings("Interact")), "SPACE,,,", "Interact with Jump's former input")

	oCheck:Expect(Inputs.SetBinding("Jump", 2, nil), "slot emptied")
	oCheck:Equal(Join(Inputs.GetBindings("Jump")), "E,,,", "Jump's second slot empty")

	local bOk, sWhy = Inputs.SetBinding("Jump", 5, "F")
	oCheck:Expect(not bOk and sWhy:find("no slot 5") ~= nil, "slot 5 refused: " .. tostring(sWhy))
	bOk, sWhy = Inputs.SetBinding("Jump", 1, "NOPE")
	oCheck:Expect(not bOk and sWhy:find("no input named") ~= nil, "unknown input refused: " .. tostring(sWhy))
	bOk, sWhy = Inputs.SetBinding("Fly", 1, "F")
	oCheck:Expect(not bOk and sWhy:find("no action named") ~= nil, "unknown action refused: " .. tostring(sWhy))

	-- Saved at once in the save folder
	local tSaved = Json.Decode(Storage.Read("input_bindings.json") or "{}")
	local tJump = tSaved.bindings and tSaved.bindings[1]
	oCheck:Equal(tJump and tJump.name, "Jump", "Jump saved first")
	oCheck:Equal(tJump and Join(tJump.inputs), "E,,,", "Jump's slots saved")

	-- One action back to its defaults takes them from whoever holds them; then all
	Inputs.ResetBindings("Jump")
	oCheck:Equal(Join(Inputs.GetBindings("Jump")), "SPACE,GAMEPAD_A,,", "Jump reset")
	oCheck:Equal(Join(Inputs.GetBindings("Interact")), ",,,", "Interact gave SPACE back")
	Inputs.ResetBindings()
	oCheck:Equal(Join(Inputs.GetBindings("Interact")), "E,,,", "all reset")

	-- The labels a menu shows
	oCheck:Equal(Inputs.GetInputName("MOUSE_RIGHT"), "Mouse Right", "a mouse button's label")
	oCheck:Equal(Inputs.GetInputName("MOUSE_BUTTON_5"), "Mouse 5", "a side button's label")
	oCheck:Equal(Inputs.GetInputName("GAMEPAD_LEFT_Y+"), "Pad Left Stick Up", "a stick's label")
	oCheck:Equal(Inputs.GetInputName("GAMEPAD_RIGHT_TRIGGER"), "Pad Right Trigger", "a trigger's label")
	oCheck:Equal(Inputs.GetInputName("GAMEPAD_DPAD_UP"), "Pad Dpad Up", "a pad button's label")
	oCheck:Equal(Inputs.GetInputName("LEFT_SHIFT"), "Left Shift", "a key that prints nothing")
	oCheck:Equal(Inputs.GetInputName("NOPE"), "", "no label for a name no input has")

	oCheck:Finish()
end

return InputActionsCheck
