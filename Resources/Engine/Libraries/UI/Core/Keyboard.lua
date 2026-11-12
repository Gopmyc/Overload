--- Keyboard helpers. The characters a panel receives come from the engine, as the keyboard's layout
--- makes them (Panel:OnTextTyped); key codes are for editing and navigation.
---@class UIKeyboard
local Keyboard = {}

-- The keys sent to the focused panel's OnKeyCodePressed and OnKeyCodeReleased
local kTrackedKeys = {
	"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T",
	"U", "V", "W", "X", "Y", "Z",
	"ALPHA_0", "ALPHA_1", "ALPHA_2", "ALPHA_3", "ALPHA_4", "ALPHA_5", "ALPHA_6", "ALPHA_7", "ALPHA_8",
	"ALPHA_9",
	"SPACE", "APOSTROPHE", "COMMA", "MINUS", "PERIOD", "SLASH", "SEMICOLON", "EQUAL", "LEFT_BRACKET",
	"BACKSLASH", "RIGHT_BRACKET", "GRAVE_ACCENT",
	"KP_0", "KP_1", "KP_2", "KP_3", "KP_4", "KP_5", "KP_6", "KP_7", "KP_8", "KP_9", "KP_DECIMAL",
	"KP_DIVIDE", "KP_MULTIPLY", "KP_SUBTRACT", "KP_ADD", "KP_EQUAL",
	"BACKSPACE", "DELETE", "ENTER", "KP_ENTER", "TAB", "ESCAPE", "LEFT", "RIGHT", "UP", "DOWN", "HOME",
	"END"
}

local trackedKeys = nil

--- Returns the key codes the UI polls while a panel has the keyboard focus
---@return Key[]
function Keyboard.GetTrackedKeys()
	-- Key is a read-only engine enum that can't be iterated, so every key is looked up by name
	if not trackedKeys then
		trackedKeys = {}

		for i = 1, #kTrackedKeys do
			local key = Key[kTrackedKeys[i]]

			if key then
				trackedKeys[#trackedKeys + 1] = key
			end
		end
	end

	return trackedKeys
end

--- Returns true while a Shift key is held, read through the keyboard's capture by the UI
---@return boolean
function Keyboard.IsShiftDown()
	return Inputs.GetKey(Key.LEFT_SHIFT, true) or Inputs.GetKey(Key.RIGHT_SHIFT, true)
end

return Keyboard
