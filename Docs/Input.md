# Input

- `Key.*` constants name physical keys after the US layout: `Key.W`, `Key.A`, `Key.S`, `Key.D` are the keys labelled
  Z, Q, S, D on an AZERTY keyboard. `Inputs.GetKeyName(key)` gives the layout's own name of a printable key, in lower
  case, and `""` for the others.
- Mouse buttons are `MouseButton.BUTTON_LEFT`, `BUTTON_RIGHT`, `BUTTON_MIDDLE`, or `BUTTON_1` to `BUTTON_8`.
- The engine has no input actions: a game that lets the player rebind keys maps its own action names to keys.
- Input is not consumed: a click or a key meant for the UI also reaches gameplay scripts. Gameplay checks whether the
  UI holds the focus before reading input (see the UI library's `EngineLimitations.md`).
- In the editor, the Game View's focus decides: the game reads keys, clicks, scroll and the mouse lock only while the
  Game View has the focus. Elsewhere it sees nothing, a key held is released for it, its cursor is freed and play goes
  on. The click that gives the focus is not seen by the game. F5 starts and stops play; Escape belongs to the game.
