# Input

- `Key.*` constants name physical keys after the US layout: `Key.W`, `Key.A`, `Key.S`, `Key.D` are the keys labelled
  Z, Q, S, D on an AZERTY keyboard. `Inputs.GetKeyName(key)` gives the layout's own name of a printable key, in lower
  case, and `""` for the others.
- Mouse buttons are `MouseButton.BUTTON_LEFT`, `BUTTON_RIGHT`, `BUTTON_MIDDLE`, or `BUTTON_1` to `BUTTON_8`.

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
- The engine has no input actions: a game that lets the player rebind keys maps its own action names to keys.
- Input is not consumed: a click or a key meant for the UI also reaches gameplay scripts. Gameplay checks whether the
  UI holds the focus before reading input (see the UI library's `EngineLimitations.md`).
- In the editor, the Game View's focus decides: the game reads keys, clicks, scroll and the mouse lock only while the
  Game View has the focus. Elsewhere it sees nothing, a key held is released for it, its cursor is freed and play goes
  on. The click that gives the focus is not seen by the game.
- F5 starts and stops play by default. Settings > Play Shortcuts sets either key to F1 to F12, Pause, Escape or none.
  The stop key also reaches the game, so Escape is best left to it.
