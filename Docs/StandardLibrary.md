# Standard library

The building blocks most games write for themselves, shipped with the engine in
[`Resources/Engine/Libraries/Std/`](../Resources/Engine/Libraries/Std), in plain Lua. Each module documents its
functions with LuaLS annotations, for a language server.

```lua
local Std = require(":Libraries/Std/Init")       -- every module
local Timers = require(":Libraries/Std/Timers")  -- or one of them
```

| Module | What it is |
| --- | --- |
| `Class` | Classes with single inheritance, instances, final names |
| `Signal` | An object's events, with listeners that end with their owner |
| `Timers` | Delays and repeats, on game or real time, advancing by themselves |
| `Tween` | Values carried over time, eased, looped, chained, advancing by themselves |
| `Ease` | Easing curves, and an approach that ignores the frame rate |
| `Pool` | Objects made once and taken again |
| `StateMachine` | States tried in order, with enter, update and exit |

It is not part of the Lua API that `Application.GetApiVersion()` versions (see [Scripting](Scripting.md)): it is
loaded like any module, and the engine's [`CHANGELOG.md`](../CHANGELOG.md) says when it changes.

## Rules shared by every module

- **A wrong argument raises an error** that names the module and what it expected
  (`Std.Timers: After takes a function`), where the call was made.
- **A function of yours that raises an error** while a timer, a tween or a signal calls it is logged once and dropped
  alone: its timer is cancelled, its tween stopped, its listener disconnected. The others go on.
- **An owner** (`owner` in the options of a timer or a tween, the second argument of `Signal:Connect`) is an actor, or
  a Behaviour's table, whose `owner` actor is taken. What it owns ends once that actor is no longer alive, which is
  how a module-level timer, tween or listener ends with the scene's actors. A signal also skips a listener while its
  owner is switched off.
- **Timers and tweens advance by themselves**, once a frame, in the global `LateUpdate` event, so after every
  Behaviour's `OnLateUpdate` (see [Update order and time](Scripting.md#update-order-and-time)). No Behaviour drives
  them. They live with the Lua state, like every module: they outlive a scene change, and the editor drops them when
  play starts and stops.
- **Game time and real time**: by default they run on the frame's step, which follows `Time.SetScale` and stops when
  the game is paused. With `unscaled = true` they run on `Time.GetUnscaledDeltaTime()`, for a menu or a fade that must
  go on.

## Class

```lua
local Weapon = Std.Class.New()
function Weapon.New(damage) return Std.Class.Make(Weapon, { damage = damage }) end
function Weapon:Hit(target) target:Hurt(self.damage) end

local Sword = Std.Class.New(Weapon)
function Sword.New() return Std.Class.Make(Sword, { damage = 12 }) end

Std.Class.IsA(Sword.New(), Weapon) -- true
```

- `Class.New([base])` returns a class. Its instances read the class, then its base, and so on. `class.Base` is its
  base.
- `Class.Make(class[, fields])` returns an instance made of the given fields.
- `Class.IsA(value, class)` tells an instance of the class or of a class deriving from it.
- `Class.Final(class, names)` makes names final: a class derived from it afterwards raises an error where it sets one
  of them. Instances may still set them.

## Signal

```lua
self.Damaged = Std.Signal.New()                                    -- in the emitter
self.Damaged:Emit(amount, source)

local id = health.Damaged:Connect(function(amount, source) end, self) -- in a listener
health.Damaged:Disconnect(id)
```

- `Signal.New()`, then `signal:Connect(callback[, owner])`, which returns an id for `signal:Disconnect(id)`.
- `signal:Emit(...)` calls the listeners in the order they connected, with what it is given. One connected during an
  Emit is first called at the next; one disconnected during an Emit is not called after that.
- `signal:GetCount()` counts the listeners, those whose owner is gone left out; `signal:Clear()` disconnects them all.

## Timers

```lua
Std.Timers.After(2, function() door:Open() end, { owner = self })
local blink = Std.Timers.Every(0.5, function() light:SetActive(not light:IsActive()) end)
blink:Cancel()
```

- `Timers.After(delay, callback[, options])` calls once; `Timers.Every(period, callback[, options])` calls every period
  until the callback returns `false` or the timer is cancelled. Both return a timer, with `Cancel()` and
  `IsPending()`.
- The options are `unscaled` and `owner`.
- A timer made during a frame counts that frame's step; one made while the timers advance counts from the next frame.
- `Every` fires at most once a frame: a frame longer than its period starts the next period from then, without catching
  up.
- `Timers.Clear()` cancels every timer.

## Tween

```lua
Std.Tween.Run({
	duration = 0.4, from = Vector3.new(0, 0, 0), to = Vector3.new(0, 2, 0), ease = Std.Ease.OutBack,
	onUpdate = function(position) transform:SetLocalPosition(position) end,
	owner = self
}):Then({ duration = 0.2, onUpdate = function(alpha) end })
```

- `Tween.Run(spec)` starts a tween. The spec:
  - `duration` and `delay`, in seconds;
  - `from` and `to`, 0 and 1 by default, or anything that adds and scales, such as a `Vector3`;
  - `ease`, a curve of `Ease` or any function of [0, 1], linear by default;
  - `onUpdate(value, progress)`, with the eased share of the run;
  - `onDone()`, once the tween ends, never after `Cancel`;
  - `loops` (1 by default, 0 forever), and `yoyo`, every other run backwards;
  - `unscaled` and `owner`.
- `Run` applies the start at once. A tween with neither duration nor delay ends at the next frame.
- A tween has `IsRunning()`, `GetProgress()` (the share of the run, before its curve), `Then(spec)` (a tween started as
  this one ends, which returns it), `Cancel()` (stops it and what is chained after it, without `onDone`) and `Finish()`
  (applies its end, calls `onDone`, then starts what is chained).
- `Tween.Clear()` stops every tween.

## Ease

- Curves of [0, 1], their input clamped to it: `Linear`, `InQuad`, `OutQuad`, `InOutQuad`, `InCubic`, `OutCubic`,
  `InOutCubic`, `InBack` and `OutBack` (which go past 0 or 1 on the way, with an optional overshoot), `SmoothStep` and
  `SmootherStep`.
- `Ease.Approach(from, to, speed, deltaTime)` moves a value towards a target, on numbers and vectors: two frames of
  half a step land where one full step does. `Ease.ApproachFactor(speed, deltaTime)` is the share it covers,
  `1 - e^(-speed × deltaTime)`.

## Pool

```lua
local sparks = Std.Pool.New({
	max = 32,
	make = function() return Prefabs.Spawn("Prefabs/Spark.ovprefab") end,
	isFree = function(spark) return not spark:IsActive() end,
	evict = function(spark) spark:SetActive(false) end
})
local spark = sparks:Take()
```

- `Pool.New({ max, make, isFree[, canEvict, evict] })`.
- `pool:Take()` gives a free object first, else makes a new one while there are fewer than `max`, else takes back the
  one taken the longest ago that `canEvict` lets go (all of them by default), calling `evict` on it. It returns `nil`
  when there is none. A `make` that returns `nil` ends the making: the pool asks no more.
- `pool:GetItems()` and `pool:GetCount()` give the objects made; `pool:GetTakeCount()` and `pool:GetTakeIndex(object)`
  number the takes, for the age of an object.

## StateMachine

```lua
self.machine = Std.StateMachine.New(self, {
	{ name = "Flee", condition = function(mind) return mind.health < 10 end, update = function(mind, dt) mind:Run(dt) end },
	{ name = "Fight", condition = function(mind) return mind.target ~= nil end, enter = function(mind) mind:Draw() end },
	{ name = "Idle" }
})
self.machine:Update(deltaTime) -- in OnUpdate
```

- `StateMachine.New(owner, states)`: the states are tried in order. Each has a `name`, and optionally
  `condition(owner)`, `enter(owner, previousName)`, `update(owner, deltaTime)` and `exit(owner, nextName)`.
- `machine:Update(deltaTime)` moves to the first state whose condition holds (or which has none), calling `exit` then
  `enter` as it changes, then calls its `update`. It returns the state's name.
- `machine:GetState()` gives the name of the current state; `machine:SetState(name)` moves to a state whatever the
  conditions, until the next `Update`; `machine:Resolve()` returns the state that should be the current one.
