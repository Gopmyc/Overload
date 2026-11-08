# Input

- `Key.*` constants name physical keys after the US layout: `Key.W`, `Key.A`, `Key.S`, `Key.D` are the keys labelled
  Z, Q, S, D on an AZERTY keyboard. `Inputs.GetKeyName(key)` gives the layout's own name of a printable key, in lower
  case, and `""` for the others.
- Mouse buttons are `MouseButton.BUTTON_LEFT`, `BUTTON_RIGHT`, `BUTTON_MIDDLE`, or `BUTTON_1` to `BUTTON_8`.

## Actions

Gameplay reads actions ("Jump", "Attack") rather than keys, so that the player can rebind them and play with a keyboard,
a mouse or a gamepad alike.

- The project declares its actions in a JSON file of its assets, which its `input_actions` key names (see
  [Projects](Projects.md)), each with its default inputs, in the order a menu lists them:

  ```json
  {
  	"actions": [
  		{ "name": "Jump", "inputs": ["SPACE", "GAMEPAD_A"] },
  		{ "name": "Attack", "inputs": ["MOUSE_LEFT", "GAMEPAD_RIGHT_TRIGGER"] },
  		{ "name": "MoveRight", "inputs": ["D", "RIGHT", "GAMEPAD_LEFT_X+"] }
  	]
  }
  ```

- An input is named after the `Key` enum (`SPACE`, `W`, `LEFT_SHIFT`, `F1`), `MOUSE_LEFT`, `MOUSE_RIGHT`,
  `MOUSE_MIDDLE`, `MOUSE_BUTTON_4` to `MOUSE_BUTTON_8`, `GAMEPAD_A` to `GAMEPAD_DPAD_LEFT` (the `GamepadButton` enum),
  `GAMEPAD_LEFT_TRIGGER` and `GAMEPAD_RIGHT_TRIGGER`, or one way of a stick's axis: `GAMEPAD_LEFT_X+`, `GAMEPAD_LEFT_X-`,
  `GAMEPAD_LEFT_Y+` (up), `GAMEPAD_LEFT_Y-`, and the same for `RIGHT`. A gamepad input is read on the first connected
  pad.
- An action has 4 slots. An input bound twice stays only where it comes first. What the file gets wrong (an unknown
  input, an action named twice, a fifth input) is logged as a warning and skipped. The editor reads the file again each
  time play starts.
- `Inputs.GetAction(name)`, `GetActionDown` and `GetActionUp` read an action as the keyboard's functions read a key:
  held while any of its inputs is (an axis or a trigger from halfway), pressed on the frame it begins to be held,
  released on the frame it stops. `Inputs.GetActionValue(name)` is how far it is pushed, 0 to 1, the furthest of its
  inputs: a stick's way for a walk speed. An action the project does not declare is logged once and reads as released.
- A rebinding menu:
  - `Inputs.GetActionNames()` lists the actions;
  - `Inputs.GetBindings(name)` gives their 4 slots (`""` for an empty one);
  - `Inputs.GetInputName(input)` gives a label to show: a printable key as the keyboard's layout names it (`Z` for the
    `W` of a French keyboard), the others in words (`Left Shift`, `Mouse Right`, `Pad A`, `Pad Left Stick Up`);
  - `Inputs.GetPressedInput()` gives the input that went down during the frame, keys before buttons, buttons before
    axes, to bind it;
  - `Inputs.SetBinding(name, slot, input)` binds a slot (1 to 4), or empties it with `""` or `nil`. An input another
    slot holds, of this action or another, moves here, and that slot gets back what this one held. It returns `true`, or
    `false` and why;
  - `Inputs.ResetBindings([name])` brings an action, or all of them, back to their defaults. An action reset takes its
    defaults from whoever holds them.
- The bindings are saved at once in the project's save folder (`input_bindings.json`, see [Saving](Saving.md)), and read
  back when the game starts. What no longer matches the project (an action removed, an input unknown) is logged and
  left at its default.
- In the editor, actions follow the Game View's focus as the inputs they read do.

## Consuming input

A UI keeps from gameplay the input it takes: the click on a button, the keys typed in a text field.

- `Inputs.Consume(input[, pad])` hides an input from the game's reads, direct (`GetKey`, `GetMouseButton`,
  `GetGamepadButton`, `GetGamepadAxis`) and through the actions, until it is released. The input is named as for the
  actions (`"MOUSE_LEFT"`, `"SPACE"`, `"GAMEPAD_A"`, `"GAMEPAD_LEFT_X+"`); a gamepad input is that of the given pad, or
  of the first connected one.
  - An input consumed as it goes down never reaches the game, neither its press nor its release.
  - One the game saw held before this frame is released for it on this frame, so that nothing it drives stays down.
  - A trigger or a way of a stick stays hidden until it is back to 0; the stick's other way still reads.
  - It returns `true` if the input is hidden from now on, `false` when it is not held (there is nothing to hide), or
    `false` and why for a name no input has. Consuming again what is hidden already changes nothing, so a UI can
    consume on every frame it holds an input.
- `Inputs.ConsumeMouseScroll()` hides the scroll of the frame.
- `Inputs.SetKeyboardCaptured(true)`, for a text field, hides every key, directly and through the actions; the mouse
  and the gamepads are still read. A key the game saw held is released for it then, and a key still held when
  `Inputs.SetKeyboardCaptured(false)` frees the keyboard stays hidden until released. The capture lasts until it is
  freed, through a scene change too; `Inputs.IsKeyboardCaptured()` tells whether it is on.
- What consumes still reads what it took: every read takes a last argument, `includeConsumed`, that sees through the
  consumption and the capture: `Inputs.GetKey(key, true)`, `Inputs.GetMouseButtonUp(button, true)`,
  `Inputs.GetGamepadButton(button, nil, true)` (`nil` for the first connected pad), `Inputs.GetMouseScroll(true)`. The
  Game View's focus still applies to it.
- Consume before gameplay reads: in an `Events.AddUpdate` callback, which runs before every Behaviour (see
  [Scripting](Scripting.md)). The actions, read once a frame before the scripts, leave out at once what is consumed.
- In the editor, each play session starts with nothing consumed and the keyboard free.
- The engine's UI library does not consume yet: gameplay still checks whether the UI holds the focus before reading
  input (see the UI library's `EngineLimitations.md`).

## Gamepads

- Up to 4 gamepads, numbered 1 to 4. A device counts as a gamepad when the system can map it to a gamepad layout
  (GLFW's built-in mappings cover the common pads); a joystick it cannot map is not read.
- `Inputs.GetGamepadButton(button[, pad])`, `GetGamepadButtonDown` and `GetGamepadButtonUp` read a button as the
  keyboard's functions read a key: held, pressed during the frame, released during the frame. Buttons are named after
  an Xbox layout: `GamepadButton.A` is the bottom face button (Cross on a PlayStation pad), then `B`, `X`, `Y`,
  `LEFT_BUMPER`, `RIGHT_BUMPER`, `BACK`, `START`, `GUIDE`, `LEFT_THUMB`, `RIGHT_THUMB` and the `DPAD_*`.
- `Inputs.GetGamepadAxis(axis[, pad])` reads `GamepadAxis.LEFT_X`, `LEFT_Y`, `RIGHT_X` and `RIGHT_Y` from -1 to 1,
  right and up positive, and `LEFT_TRIGGER` and `RIGHT_TRIGGER` from 0 at rest to 1.
- A stick reads 0 inside its dead zone, a disc around its centre (0.2 of its travel by default,
  `Inputs.SetGamepadDeadZone(value)`, 0 to 0.95), and is scaled beyond it so that it still reaches 1. A trigger has a
  small dead zone of its own.
- Without a pad, the functions read the first connected one. `Inputs.IsGamepadConnected([pad])` tells whether one is
  there, `Inputs.GetGamepadName([pad])` its name. A pad that is not there, or out of 1 to 4, reads as released and 0.
- A pad unplugged releases its buttons on that frame. In the editor, a pad follows the Game View's focus as the
  keyboard does; each play session starts with the default dead zone.
- In the editor, the Game View's focus decides: the game reads keys, clicks, scroll and the mouse lock only while the
  Game View has the focus. Elsewhere it sees nothing, a key held is released for it, its cursor is freed and play goes
  on. The click that gives the focus is not seen by the game.
- F5 starts and stops play by default. Settings > Play Shortcuts sets either key to F1 to F12, Pause, Escape or none.
  The stop key also reaches the game, so Escape is best left to it.
