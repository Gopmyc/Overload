# UI library: engine features to expose to Lua

Features that already exist in the engine but aren't reachable from Lua, and what exposing them
would bring to the UI library. Missing engine primitives are listed in
[EngineLimitations.md](EngineLimitations.md).

## 1. `string`, `table` and `utf8` standard libraries

- **Exists:** the embedded Lua 5.4 ships them (`Dependencies/lua/src/lstrlib.c`, `ltablib.c`,
  `lutf8lib.c`), but `LuaScriptEngine::CreateContext` only opens `sol::lib::base` and `sol::lib::math`,
  and the generated `.luarc.json` disables them.
- **Proposal:** open `sol::lib::string`, `sol::lib::table` and `sol::lib::utf8`, and enable them in
  `.luarc.json` (#246). None of them gives access to files or to the operating system.
- **Why:** without `string`, a text can't be split into characters: `DTextEntry` edits a value given to
  `SetText` as a single unit, and editing isn't UTF-8 aware. `Core/Array.lua` reimplements insertion,
  removal and sorting, and would be removed.

## 2. Resolved canvas scale and size

- **Exists:** `UIRenderingUtils::GetCanvasScale(const CCanvas&, const FVector2& renderSize)` and
  `UIRenderingUtils::GetCanvasSize`.
- **Proposal:** `Canvas:GetScale(renderSize)` and `Canvas:GetSize(renderSize)`, `renderSize` being
  `Inputs.GetViewportSize()`.
- **Why:** `Core/CanvasSpace.lua` duplicates the scaling formulas (constant pixel size, scale with
  screen size, match / expand / shrink) and can drift from the engine.

## 3. Resolved element geometry

- **Exists:** `UIRenderingUtils::UIFrameResolver::ResolveElement`, whose `ResolvedUIElement` holds the
  `modelMatrix` and `effectiveSize` actually used to draw an element.
- **Proposal:** a query returning the resolved rectangle of a UI actor for a render size, or a
  point-in-element test in canvas space.
- **Why:** panel geometry is computed in Lua. The library handles top-left anchoring, rotation and
  scale, but not the other anchors, stretch or `HorizontalLayout` / `VerticalLayout`. Engine-resolved
  geometry would make hit-testing honor all of them, and remove the duplicated transform math.

## 4. Text measurement

- **Exists:** `CText::GetSize()` (bounds of the generated text) and `TextLayoutEngine::Layout`, whose
  output lists the rectangle of every glyph.
- **Proposal:** `Text:GetSize()` and a glyph or caret position query, such as
  `Text:GetCaretPosition(index)`.
- **Why:** it enables `SizeToContents` and `GetTextSize` on labels, a caret drawn as a separate panel
  instead of a `|` inserted in the text (which shifts the following characters), placing the caret
  under the cursor on click, and horizontal scrolling of long `DTextEntry` values.

## 5. Main camera

- **Exists:** `Scene::FindMainCamera()`, the first active camera of the scene, which renders it.
- **Proposal:** `Scene:GetMainCamera()`.
- **Why:** world panels need the camera rendering the scene, so `controller:SetCamera` must receive it
  from the script. The controller could follow the engine choice instead.

## 6. Camera matrices

- **Exists:** `OvRendering::Entities::Camera::GetViewMatrix()` and `GetProjectionMatrix()`.
- **Proposal:** `Camera:GetViewMatrix()` and `Camera:GetProjectionMatrix()`, or a
  `Camera:WorldToScreen(position)` query.
- **Why:** `Core/WorldSpace.lua` rebuilds the view and projection from the camera transform, its field
  of view and its projection mode, mirroring `FMatrix4::CreateView`, `CreatePerspective` and
  `CreateOrthographic`. It would drift if the engine changed them.
