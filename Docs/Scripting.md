# Scripting

## The Lua sandbox and modules

- Lua 5.4, with the `base`, `math`, `string`, `table`, `coroutine` and `utf8` libraries. There is no `io`, `os` or
  `package`: files go through `Storage`, in the save folder only (see [Saving](Saving.md)).
- There is no `require`. A module is loaded with `Resources.GetScript(path)`:
  - the path is relative to the project's `Assets` (`"Scripts/Core/Signal.lua"`);
  - a leading colon resolves in the engine's resources (`":Libraries/UI/Init.lua"`);
  - the module's table is cached, so every caller shares one instance: module-level tables are process-wide;
  - it returns `nil` for a module that is still loading, so a circular dependency fails; it does not cache a failure.
    Load such a dependency lazily and remember the outcome.
- Modules outlive a scene change: `Scenes.Load(path)` replaces the actors at the next frame and keeps the Lua state, so
  what a module holds carries into the next scene. A registry of actors must drop the entries it can no longer reach
  (`IsAlive`). The editor rebuilds the state when play starts and stops.
- The engine's functions are `userdata` to Lua: `type(Inputs.GetKeyName)` is `"userdata"`, never `"function"`, so a
  `type(...) == "function"` check silently skips them.

## Behaviours

- A Behaviour is a script returning a table. Its fields with default values become inspector properties: number,
  string, boolean, an asset, or `Actor()` for an actor reference. A colour, a vector or a list stays a constant of the
  script, or a string the script parses.
- Values set in a prefab or a scene are saved as `script_properties` and override the script defaults: the value in
  play is the prefab's or the scene's.
- In a prefab or a scene file, the property types are `number`, `string`, `bool`, `actor` and `asset`. A property
  written `type="boolean"` is not read at all, and the script default silently wins.
- `self.owner` is the actor the Behaviour sits on.
- Callbacks: `OnAwake`, `OnStart`; `OnEnable`, `OnDisable`, `OnDestroy`; `OnUpdate(dt)`, `OnFixedUpdate(dt)`,
  `OnLateUpdate(dt)`; `OnCollisionEnter/Stay/Exit`, `OnTriggerEnter/Stay/Exit`.
- The Behaviours of one actor run in their declared order, the order of the prefab or the scene file, for every
  callback. Across actors, the order follows the scene. A Behaviour asked for something by a sibling before its own
  `OnStart` should build its state lazily.
- A Behaviour's script is compiled once per Lua state and run for each Behaviour, which gets a table of its own: a
  field written on that table belongs to that Behaviour. State shared by every copy lives in a module.
- `Actor:SetBehaviourUpdating(behaviour, false)` stops a Behaviour's `OnUpdate`, `OnFixedUpdate` and `OnLateUpdate`
  until it is set back to `true`; its other callbacks still come. A Behaviour with nothing to do should sleep this way.
- `GetBehaviour(name)` accepts a script stem (`"Health"`) or a path. It is a linear scan: cache hot lookups.

## Actors and references

- Lua holds actors as handles: calling a method on a destroyed actor raises `attempt to use a destroyed actor`, and
  `IsAlive()` tests a handle without raising. Components are not checked the same way: a component kept after its
  actor is destroyed must not be used.
- `FindChild` and `GetParent` return `nil` when there is none. `GetChildren` and `Scene:FindActorsByName/ByTag` return
  plain Lua tables.
- An actor property saved in a prefab does not survive instantiation: every instantiated actor gets a new GUID, and the
  script receives `nil`. Resolve such a reference through the hierarchy (`FindChild(name, true)`) or by tag.
- `Destroy()` marks an actor and its children; the engine frees them at the end of the frame, in a built game as in the
  editor's play mode. Until then the actor is still there: switch it off with `SetActive(false)` first if it must vanish
  at once. Its handles then fail (`IsAlive()` is `false`).
- A prefab spawned under an inactive parent starts once the parent is switched on: its `OnAwake` and `OnStart` wait.
- An actor switched off receives no callback until it is switched on again.
- Instantiating a prefab from Lua:

  ```lua
  local oPrefab = Prefab()
  oPrefab.path = "Prefabs/Item.ovprefab"

  local oActor = Scenes.GetCurrentScene():InstantiatePrefab(oPrefab, oParent)
  ```

  It is safe from any callback, `OnStart` included.

## Update order and time

- Actors update in scene order, the order of the scene file. Actors instantiated at run time come last.
- A `BoneAttachment` is placed after every script, so a script reading such an actor's place during the frame sees
  where it was the frame before.
- The frame step handed to the Behaviours is in [0, 0.1] seconds, times the time scale, which is never below 0.
- `Time.SetScale(scale)` scales every step handed to the Behaviours, the physics and the animations, so the whole world
  slows down together. What must keep its pace runs on `Time.GetUnscaledDeltaTime()`. Stopping play in the editor sets
  the scale back to 1. `Time.GetGameTime()` is the scaled clock, also `ubo_GameTime` in the shaders.
- Never assume a short frame: on a slow machine a frame can last 0.1 s. A watchdog counting seconds allows for one
  frame's length.
- `Application.Quit()` closes a built game once the frame is over and leaves play mode in the editor.
- `Application.GetEngineVersion()` returns the engine's version, so that a project can check the one it was made for.
