# UI library: engine limitations

Features of the UI library that are missing or incomplete because the engine lacks the primitive,
not just its Lua binding. Features that only need a binding are listed in
[LuaExposureGaps.md](LuaExposureGaps.md).

## 1. No clipping of UI elements

- **Feature:** `DScrollPanel` content, and more generally any child overflowing its parent.
- **Missing primitive:** a clip rectangle, mask or scissor for UI drawables.
  `SceneRenderer::AppendHierarchyUIDrawables` emits `Image` and `Text` drawables without any clip
  information.
- **Impact:** `DScrollPanel` hides the children entirely outside its view, but partially visible
  children overflow it. Hit-testing is clipped to the parent bounds, so an overflowing part is
  visible but can't be hovered or clicked.

## 2. No immediate-mode drawing

- **Feature:** `Paint(width, height)` drawing arbitrary content, as with `surface.Draw*` in Garry's Mod.
- **Missing primitive:** a UI drawing API (rectangles, outlines, lines, text at a position). UI rendering
  is retained: only `Image` and `Text` components are drawn.
- **Impact:** `Paint` is called when the visual state changes (size, hover, pressed, enabled, focus) and
  configures the components of the panel. Custom shapes such as borders, rounded corners or gradients
  can only be obtained through textures.

## 3. No character input and no key repeat

- **Feature:** typing in `DTextEntry`.
- **Missing primitive:** character events (#848). `OvWindowing::Window` doesn't bind the GLFW character
  callback, and `InputManager` only records key presses and releases, without repeat.
- **Impact:** `Core/Keyboard.lua` derives characters from key codes with a US QWERTY layout and Shift.
  Other keyboard layouts, AltGr, dead keys, accents, IME and the Caps Lock state aren't supported. Key
  repeat is emulated by the controller (0.4 s delay, then every 0.05 s).

## 4. Inputs can't be consumed

- **Feature:** keeping UI input away from gameplay.
- **Missing primitive:** a way to mark an input as handled.
- **Impact:** game scripts still receive the keys and clicks meant for the UI. For instance, typing in
  a `DTextEntry` also triggers gameplay key bindings. Games have to check
  `controller:GetFocusedPanel()` and `controller:GetHoveredPanel()` themselves.

## 5. A zero size axis means "intrinsic size"

- **Feature:** panels with a zero width or height.
- **Missing primitive:** `CTransform::SetUISize` reserves 0 for the intrinsic size of the element
  (texture size, text size).
- **Impact:** the library sends 0.001 instead of 0, so an empty panel doesn't grow to the size of its
  image or text.

## 6. No sibling index API

- **Feature:** `SetZPos`.
- **Missing primitive:** moving an actor to a given index among its siblings (#384). Actors are drawn in
  the order of their parent children list, and `Actor::SetParent` always appends.
- **Impact:** changing the Z position re-attaches every sibling in order, which fires Attach and Detach
  events and costs O(n) per change.

## 7. UI updates need a Behaviour of the project

- **Feature:** a controller that updates itself once `UI.CreateController()` has been called.
- **Missing primitive:** a per-frame hook for Lua code outside Behaviours, or Behaviours loaded from the
  engine assets. `LuaScriptEngineBase::AddBehaviour` always resolves the script against the project
  assets (`projectAssetsPath / name`), and no update event is exposed to Lua.
- **Impact:** `controller:Update(deltaTime)` must be called from `OnUpdate` of a project Behaviour,
  attached to any actor. The canvas actor created by the controller doesn't depend on that actor: if
  the actor is destroyed without calling `controller:Destroy()` in `OnDestroy`, the UI stays on screen
  without updating. `controller:Destroy()` is safe in `OnDestroy`, including while the scene unloads.

## 8. Component references aren't checked

- **Feature:** panels surviving the destruction of their actors by other scripts.
- **Missing primitive:** lifetime checks on components. With #862, actors are exposed to Lua as handles
  checked on every call (`Actor:IsAlive()`, and an error on a destroyed actor), but components
  (`Transform`, `Image`, `Text`...) are still raw pointers.
- **Impact:** panels cache their components. The controller checks its canvas actor every frame and
  discards its panels once the canvas is destroyed, but destroying the actor of a single panel outside
  `Panel:Remove` leaves its cached components dangling, and isn't supported.

## 9. UI is always drawn over the scene

- **Feature:** panels in the 3D world (`DWorldPanel`, `DWorldPath`, `DWorldArc`, see
  [WorldSpace.md](WorldSpace.md)).
- **Missing primitive:** a world space render mode per canvas. The game draws every canvas as a screen
  overlay. `UIRenderingUtils::UIFrameResolver` can already resolve canvases in the world, but only for a
  whole view: `renderUIInScreenSpace` is false in the editor Scene View preview.
- **Impact:** world panels are projected on the canvas every frame:
  - a flat panel can't be tilted in perspective, like a menu floating next to a character;
  - the scene doesn't hide them. `SetOcclusionCheck` hides a `DWorldPanel` when a raycast hits
    another collider, and `SetFrontOnly` hides the far half of a path around a character;
  - they aren't sorted with the scene, and only a `DWorldLayer` sorts them between themselves.
