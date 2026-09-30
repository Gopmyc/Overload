--- Keyboard helpers. Overload only reports key presses, not typed characters, so characters are
--- derived from key codes with a US QWERTY layout (see Docs/EngineLimitations.md).
---@class UIKeyboard
local Keyboard = {}

-- { key name, character, character with shift }
local kCharacterKeys = {
	{ "A", "a", "A" }, { "B", "b", "B" }, { "C", "c", "C" }, { "D", "d", "D" }, { "E", "e", "E" },
	{ "F", "f", "F" }, { "G", "g", "G" }, { "H", "h", "H" }, { "I", "i", "I" }, { "J", "j", "J" },
	{ "K", "k", "K" }, { "L", "l", "L" }, { "M", "m", "M" }, { "N", "n", "N" }, { "O", "o", "O" },
	{ "P", "p", "P" }, { "Q", "q", "Q" }, { "R", "r", "R" }, { "S", "s", "S" }, { "T", "t", "T" },
	{ "U", "u", "U" }, { "V", "v", "V" }, { "W", "w", "W" }, { "X", "x", "X" }, { "Y", "y", "Y" },
	{ "Z", "z", "Z" },
	{ "ALPHA_0", "0", ")" }, { "ALPHA_1", "1", "!" }, { "ALPHA_2", "2", "@" }, { "ALPHA_3", "3", "#" },
	{ "ALPHA_4", "4", "$" }, { "ALPHA_5", "5", "%" }, { "ALPHA_6", "6", "^" }, { "ALPHA_7", "7", "&" },
	{ "ALPHA_8", "8", "*" }, { "ALPHA_9", "9", "(" },
	{ "SPACE", " ", " " }, { "APOSTROPHE", "'", "\"" }, { "COMMA", ",", "<" }, { "MINUS", "-", "_" },
	{ "PERIOD", ".", ">" }, { "SLASH", "/", "?" }, { "SEMICOLON", ";", ":" }, { "EQUAL", "=", "+" },
	{ "LEFT_BRACKET", "[", "{" }, { "BACKSLASH", "\\", "|" }, { "RIGHT_BRACKET", "]", "}" },
	{ "GRAVE_ACCENT", "`", "~" },
	{ "KP_0", "0", "0" }, { "KP_1", "1", "1" }, { "KP_2", "2", "2" }, { "KP_3", "3", "3" },
	{ "KP_4", "4", "4" }, { "KP_5", "5", "5" }, { "KP_6", "6", "6" }, { "KP_7", "7", "7" },
	{ "KP_8", "8", "8" }, { "KP_9", "9", "9" }, { "KP_DECIMAL", ".", "." }, { "KP_DIVIDE", "/", "/" },
	{ "KP_MULTIPLY", "*", "*" }, { "KP_SUBTRACT", "-", "-" }, { "KP_ADD", "+", "+" }, { "KP_EQUAL", "=", "=" }
}

local kControlKeys = {
	"BACKSPACE", "DELETE", "ENTER", "KP_ENTER", "TAB", "ESCAPE", "LEFT", "RIGHT", "UP", "DOWN", "HOME", "END"
}

local characters = nil
local trackedKeys = nil

-- Key is a read-only engine enum that can't be iterated, so every key is looked up by name
local function Build()
	characters = {}
	trackedKeys = {}

	for i = 1, #kCharacterKeys do
		local entry = kCharacterKeys[i]
		local key = Key[entry[1]]

		if key then
			characters[key] = entry
			trackedKeys[#trackedKeys + 1] = key
		end
	end

	for i = 1, #kControlKeys do
		local key = Key[kControlKeys[i]]

		if key then
			trackedKeys[#trackedKeys + 1] = key
		end
	end
end

--- Returns the key codes the UI polls while a panel has the keyboard focus
---@return Key[]
function Keyboard.GetTrackedKeys()
	if not trackedKeys then
		Build()
	end

	return trackedKeys
end

--- Returns the character typed by the given key, or nil if the key doesn't type a character
---@param key Key
---@return string|nil
function Keyboard.ToCharacter(key)
	if not characters then
		Build()
	end

	local entry = characters[key]

	if not entry then
		return nil
	end

	return Keyboard.IsShiftDown() and entry[3] or entry[2]
end

---@return boolean
function Keyboard.IsShiftDown()
	return Inputs.GetKey(Key.LEFT_SHIFT) or Inputs.GetKey(Key.RIGHT_SHIFT)
end

return Keyboard
