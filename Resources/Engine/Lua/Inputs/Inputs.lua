---@meta

--- Some global inputs functions. In the editor, the game only gets input while the Game View has the focus.
--- An input consumed (Inputs.Consume), or a key while the keyboard is captured, reads as up; a read given
--- includeConsumed = true still sees it, for the code that consumed it
---@class Inputs
Inputs = {}

--- Returns true if the key has been pressed during the current frame
---@param key Key
---@param includeConsumed? boolean
---@return boolean
function Inputs.GetKeyDown(key, includeConsumed) end

--- Returns true if the key has been released during the current frame
---@param key Key
---@param includeConsumed? boolean
---@return boolean
function Inputs.GetKeyUp(key, includeConsumed) end

--- Returns true if the key is currently down
---@param key Key
---@param includeConsumed? boolean
---@return boolean
function Inputs.GetKey(key, includeConsumed) end

--- Returns how many times the system repeated the key held down during the current frame (0 most frames): what a text
--- field moves its caret or deletes by, at the system's repeat delay and rate
---@param key Key
---@param includeConsumed? boolean
---@return integer
function Inputs.GetKeyRepeatCount(key, includeConsumed) end

--- Returns the characters typed during the current frame, in UTF-8, as the keyboard's layout and the system's input
--- method make them: accents, dead keys and AltGr included. An empty string most frames; hidden while the keyboard is
--- captured, unless includeConsumed
---@param includeConsumed? boolean
---@return string
function Inputs.GetTypedText(includeConsumed) end

--- Returns the name of a printable key in the keyboard's current layout, as it prints it (the "z" of a French
--- keyboard where the key is Key.W), or an empty string for a key that prints nothing
---@param key Key
---@return string
function Inputs.GetKeyName(key) end

--- Returns true if the mouse button has been pressed during the current frame
---@param button MouseButton
---@param includeConsumed? boolean
---@return boolean
function Inputs.GetMouseButtonDown(button, includeConsumed) end

--- Returns true if the mouse button has been released during the current frame
---@param button MouseButton
---@param includeConsumed? boolean
---@return boolean
function Inputs.GetMouseButtonUp(button, includeConsumed) end

--- Returns true if the mouse button is currently down
---@param button MouseButton
---@param includeConsumed? boolean
---@return boolean
function Inputs.GetMouseButton(button, includeConsumed) end

--- Returns the current position of the mouse cursor, relative to the top-left corner of the Game View in the editor, or of the window in a built game
--- The position isn't clamped, so it can be outside of the rendered area
---@return Vector2
function Inputs.GetMousePos() end

--- Returns the size of the area where the game is rendered, which is the Game View in the editor, or the window in a built game
--- The size can be 0 when this area is minimized or too small
---@return Vector2
function Inputs.GetViewportSize() end

--- Returns the current scroll value
---@param includeConsumed? boolean
---@return Vector2
function Inputs.GetMouseScroll(includeConsumed) end

--- Locks and hides the mouse cursor
function Inputs.LockMouse() end

--- Unlocks and shows the mouse cursor
function Inputs.UnlockMouse() end

--- Sets the shape of the mouse cursor
---@param shape CursorShape
function Inputs.SetCursorShape(shape) end

--- Returns true if the gamepad is connected, or, without a pad, if any is. Pads go from 1 to 4; only a device the
--- system maps to a gamepad layout counts
---@param pad? integer
---@return boolean
function Inputs.IsGamepadConnected(pad) end

--- Returns the name of the gamepad (the first connected one without a pad), or an empty string when there is none
---@param pad? integer
---@return string
function Inputs.GetGamepadName(pad) end

--- Returns true if the gamepad button is currently down, on the given pad (1 to 4) or the first connected one
---@param button GamepadButton
---@param pad? integer
---@param includeConsumed? boolean
---@return boolean
function Inputs.GetGamepadButton(button, pad, includeConsumed) end

--- Returns true if the gamepad button has been pressed during the current frame
---@param button GamepadButton
---@param pad? integer
---@param includeConsumed? boolean
---@return boolean
function Inputs.GetGamepadButtonDown(button, pad, includeConsumed) end

--- Returns true if the gamepad button has been released during the current frame
---@param button GamepadButton
---@param pad? integer
---@param includeConsumed? boolean
---@return boolean
function Inputs.GetGamepadButtonUp(button, pad, includeConsumed) end

--- Returns a gamepad axis: a stick from -1 to 1 (right and up positive), 0 inside the dead zone and scaled beyond it;
--- a trigger from 0 at rest to 1. 0 when the pad is not connected
---@param axis GamepadAxis
---@param pad? integer
---@param includeConsumed? boolean
---@return number
function Inputs.GetGamepadAxis(axis, pad, includeConsumed) end

--- Sets the share of a stick's travel, from its centre, that reads as 0 (0 to 0.95, 0.2 by default)
---@param deadZone number
function Inputs.SetGamepadDeadZone(deadZone) end

--- Returns the share of a stick's travel, from its centre, that reads as 0
---@return number
function Inputs.GetGamepadDeadZone() end

--- Returns true if one of the action's inputs is held (an axis pushed at least halfway). The project declares its
--- actions in the file its input_actions key names; an action it does not declare is logged once and reads as released
---@param action string
---@return boolean
function Inputs.GetAction(action) end

--- Returns true if the action began to be held during the current frame
---@param action string
---@return boolean
function Inputs.GetActionDown(action) end

--- Returns true if the action stopped being held during the current frame
---@param action string
---@return boolean
function Inputs.GetActionUp(action) end

--- Returns how far the action is pushed, from 0 to 1: the furthest of its inputs (1 for a key held)
---@param action string
---@return number
function Inputs.GetActionValue(action) end

--- Returns the names of the project's actions, in the order the project declares them
---@return string[]
function Inputs.GetActionNames() end

--- Returns the inputs the action is bound to, its 4 slots in order ("" for an empty slot), or nil for an action the
--- project does not declare. Inputs are named "SPACE" (the names of Key), "MOUSE_LEFT", "MOUSE_BUTTON_4", "GAMEPAD_A",
--- "GAMEPAD_LEFT_TRIGGER", "GAMEPAD_LEFT_X+" (one way of a stick's axis)
---@param action string
---@return string[]|nil
function Inputs.GetBindings(action) end

--- Binds a slot (1 to 4) of an action to an input, or empties it with "" or nil. An input another slot holds moves
--- here, and that slot gets back what this one held. The bindings are saved at once in the save folder, and read back
--- when the game starts. Returns true, or false and why
---@param action string
---@param slot integer
---@param input? string
---@return boolean
---@return string|nil
function Inputs.SetBinding(action, slot, input) end

--- Binds an action, or every action without one, to its default inputs again, and saves the bindings
---@param action? string
function Inputs.ResetBindings(action) end

--- Returns the input that went down during the current frame (the first, keys before buttons, buttons before axes),
--- or nil: what a menu waits for to bind an action
---@return string|nil
function Inputs.GetPressedInput() end

--- Returns a label a menu can show for an input: a printable key as the keyboard's layout names it ("Z" for the W of a
--- French keyboard), the others in words ("Left Shift", "Mouse Right", "Pad A", "Pad Left Stick Up"); "" for a name
--- no input has
---@param input string
---@return string
function Inputs.GetInputName(input) end

--- Hides an input from the game's reads, direct or through an action, until it is released: a UI that takes a click
--- or a key keeps it from gameplay. Consume in an Events.AddUpdate callback, which runs before every Behaviour. An
--- input the game saw held before this frame is released for it on this frame; a way of a stick is hidden until the
--- stick is back to the centre. A gamepad input is that of the given pad (1 to 4) or of the first connected one.
--- Returns true if the input is hidden from now on, false when it is not held (there is nothing to hide), or false and
--- why for a name no input has
---@param input string
---@param pad? integer
---@return boolean
---@return string|nil
function Inputs.Consume(input, pad) end

--- Hides the scroll of the current frame from the game's reads
function Inputs.ConsumeMouseScroll() end

--- Captures the keyboard, for a text field, or frees it. While it is captured, every key reads as up, directly and
--- through the actions; the mouse and the gamepads are still read. A key the game saw held is released for it when the
--- capture starts, and one held when it ends stays hidden until released
---@param captured boolean
function Inputs.SetKeyboardCaptured(captured) end

--- Returns true while the keyboard is captured
---@return boolean
function Inputs.IsKeyboardCaptured() end
