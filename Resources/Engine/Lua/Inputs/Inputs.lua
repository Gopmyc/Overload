---@meta

--- Some global inputs functions. In the editor, the game only gets input while the Game View has the focus
---@class Inputs
Inputs = {}

--- Returns true if the key has been pressed during the current frame
---@param key Key
---@return boolean
function Inputs.GetKeyDown(key) end

--- Returns true if the key has been released during the current frame
---@param key Key
---@return boolean
function Inputs.GetKeyUp(key) end

--- Returns true if the key is currently down
---@param key Key
---@return boolean
function Inputs.GetKey(key) end

--- Returns the name of a printable key in the keyboard's current layout, as it prints it (the "z" of a French
--- keyboard where the key is Key.W), or an empty string for a key that prints nothing
---@param key Key
---@return string
function Inputs.GetKeyName(key) end

--- Returns true if the mouse button has been pressed during the current frame
---@param button MouseButton
---@return boolean
function Inputs.GetMouseButtonDown(button) end

--- Returns true if the mouse button has been released during the current frame
---@param button MouseButton
---@return boolean
function Inputs.GetMouseButtonUp(button) end

--- Returns true if the mouse button is currently down
---@param button MouseButton
---@return boolean
function Inputs.GetMouseButton(button) end

--- Returns the current position of the mouse cursor, relative to the top-left corner of the Game View in the editor, or of the window in a built game
--- The position isn't clamped, so it can be outside of the rendered area
---@return Vector2
function Inputs.GetMousePos() end

--- Returns the size of the area where the game is rendered, which is the Game View in the editor, or the window in a built game
--- The size can be 0 when this area is minimized or too small
---@return Vector2
function Inputs.GetViewportSize() end

--- Returns the current scroll value
---@return Vector2
function Inputs.GetMouseScroll() end

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
---@return boolean
function Inputs.GetGamepadButton(button, pad) end

--- Returns true if the gamepad button has been pressed during the current frame
---@param button GamepadButton
---@param pad? integer
---@return boolean
function Inputs.GetGamepadButtonDown(button, pad) end

--- Returns true if the gamepad button has been released during the current frame
---@param button GamepadButton
---@param pad? integer
---@return boolean
function Inputs.GetGamepadButtonUp(button, pad) end

--- Returns a gamepad axis: a stick from -1 to 1 (right and up positive), 0 inside the dead zone and scaled beyond it;
--- a trigger from 0 at rest to 1. 0 when the pad is not connected
---@param axis GamepadAxis
---@param pad? integer
---@return number
function Inputs.GetGamepadAxis(axis, pad) end

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
