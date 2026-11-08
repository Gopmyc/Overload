# Scripting

## The Lua sandbox and modules

- Lua 5.4, with the `base`, `math`, `string`, `table`, `coroutine` and `utf8` libraries. There is no `io`, `os` or
  `package`: files go through `Storage`, in the save folder only (see [Saving](Saving.md)).
- A module is loaded with `require(path)`, or with `Resources.GetScript(path)`:
  - the path is relative to the project's `Assets` (`"Scripts/Core/Signal"`), `.lua` optional for `require`;
  - a leading colon resolves in the engine's resources (`":Libraries/UI/Init"`);
  - nothing outside the two assets folders can be loaded, and `load`, `loadfile` and `dofile` do not exist;
  - the module's value is cached, so every caller shares one instance, whichever function loaded it: module-level
    tables are process-wide;
  - a module that cannot be loaded (missing, outside the assets, a syntax or runtime error, no `return`, or still
    loading because the dependency is circular) makes `require` raise an error that says why, where it was called,
    and `Resources.GetScript` log that error and return `nil`. Neither caches a failure: a circular dependency is
    loaded lazily, and its outcome remembered.
- Modules outlive a scene change: `Scenes.Load(path)` replaces the actors at the next frame and keeps the Lua state, so
  what a module holds carries into the next scene. A registry of actors must drop the entries it can no longer reach
  (`IsAlive`). The editor rebuilds the state when play starts and stops.
- The engine's functions are `userdata` to Lua: `type(Inputs.GetKeyName)` is `"userdata"`, never `"function"`, so a
  `type(...) == "function"` check silently skips them.

## API version and deprecation

The API the engine gives to scripts, the names of `Resources/Engine/Lua`, carries a version of its own, which
`Application.GetApiVersion()` returns as two integers, major and minor:

- **A name added** (a table, a function, a field, an enum value) raises the minor version.
- **A name deprecated** keeps working, with its definition marked `---@deprecated` and the replacement it points to,
  and the `CHANGELOG.md` lists it under "Deprecated". It stays for the rest of the major version.
- **A name removed or changing meaning** only happens to a deprecated name, and raises the major version.

A script made for API 1.2 thus runs on any 1.x from 1.2 on; it checks the version it needs with
`local nMajor, nMinor = Application.GetApiVersion()`.

`OvGame --dump-lua-api` prints the API, one name per line (see [Command line](CommandLine.md)): comparing the output of
two engine versions shows what changed. The engine's tests check that this API and the definitions of
`Resources/Engine/Lua` hold the same names, so a binding never ships undocumented, nor a definition without its
binding.

## Behaviours

- A Behaviour is a script returning a table. Its fields with default values become inspector properties: number,
  string, boolean, an asset, or `Actor()` for an actor reference. A colour, a vector or a list stays a constant of the
  script, or a string the script parses.
- Values set in a prefab or a scene are saved as `script_properties` and override the script defaults: the value in
  play is the prefab's or the scene's.
- In a prefab or a scene file, the property types are `number`, `string`, `bool`, `actor` and `asset`. A property
  written `type="boolean"` is not read at all, and the script default silently wins.
- `self.owner` is the actor the Behaviour sits on.
- A Behaviour's script is a path of the project's assets (`Scripts/Door.lua`), or of the engine's resources with a
  leading colon (`:Behaviours/Spin.lua`), in a scene file as with `actor:AddBehaviour(path)`. `actor:GetBehaviour`
  finds either by its file name (`"Spin"`).
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
- Every instantiated actor gets a new GUID. An actor property that names another actor of the same prefab follows it:
  the instance's property names that actor's instance. One that names an actor outside the prefab keeps its GUID, and
  the script receives `nil` when no actor of the scene has it.
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
- A frame runs, in this order: the global `Update` callbacks, every Behaviour's `OnUpdate`, every Behaviour's
  `OnLateUpdate`, the global `LateUpdate` callbacks, then the bone attachments (`OnFixedUpdate` comes first, with the
  physics, on the frames that step it).
- **Global events** run a module every frame without an actor in the scene (a timer, a tween, a registry):
  - `Events.AddUpdate(callback[, owner])` and `Events.AddLateUpdate(callback[, owner])` return an id;
    `Events.Remove(id)` removes the callback at once, even while its event runs;
  - the callback receives the same step as `OnUpdate`, and runs from the next frame on;
  - one that raises an error is logged and removed; one given an owner, an actor, is removed once that actor is no
    longer alive;
  - the callbacks live with the Lua state, like the modules: they outlive a scene change, and the editor drops them
    when play starts and stops. They run only while the scene plays.
- A `BoneAttachment` is placed after every script, so a script reading such an actor's place during the frame sees
  where it was the frame before.
- The frame step handed to the Behaviours is in [0, 0.1] seconds, times the time scale, which is never below 0. A
  project sets that ceiling with `max_frame_delta` (see [Projects](Projects.md)).
- `Time.SetScale(scale)` scales every step handed to the Behaviours, the physics and the animations, so the whole world
  slows down together. What must keep its pace runs on `Time.GetUnscaledDeltaTime()`. Stopping play in the editor sets
  the scale back to 1. `Time.GetGameTime()` is the scaled clock, also `ubo_GameTime` in the shaders.
- Never assume a short frame: on a slow machine a frame can last the whole ceiling. A watchdog counting seconds
  allows for one frame's length.
- `Application.Quit()` closes a built game once the frame is over and leaves play mode in the editor.
- `Application.GetEngineVersion()` returns the engine's version, so that a project can check the one it was made for.
