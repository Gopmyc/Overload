# UI library: panels in the 3D world

The UI meets the 3D world in two ways:

- a **world space canvas** is a plane of the scene: a menu on a plate, a screen on a wall, a label on
  an object. It is seen in perspective and what stands in front of it hides it. See
  [World space canvases](#world-space-canvases).
- **world panels** on a screen space canvas follow positions of the 3D world: name tags, markers,
  health bars, curved bars around a character, rings on the ground, off-screen indicators.

A screen space canvas is drawn over the scene, so world panels are projected on it every frame
rather than drawn in the world (see [EngineLimitations.md](EngineLimitations.md)):

- they move, scale with the distance and rotate, and arcs and paths follow the perspective;
- a flat panel can't be tilted in perspective;
- the scene geometry doesn't hide them. Only an optional raycast hides a panel behind colliders.

## World space canvases

A canvas whose render mode is `CanvasRenderMode.WORLD_SPACE` is laid by the engine on its actor's local
XY plane, centred on the actor and facing its local +Z: turn the actor's forward towards the viewer.

- Its size is its reference resolution, in canvas units, whatever its scaler mode.
- One canvas unit is `Canvas:GetWorldScale()` world units. The actor's own scale is ignored.
- It is drawn with depth testing, so the scene hides what stands behind it, but it never writes depth:
  the layers of a canvas never fight, and 3D content laid on it sits a little in front of or behind
  its plane.

```lua
local plate = Scenes.GetCurrentScene():CreateActor("Menu Plate", "")
local canvas = plate:AddCanvas()
canvas:SetRenderMode(CanvasRenderMode.WORLD_SPACE)
canvas:SetReferenceResolution(Vector2.new(800, 600))
canvas:SetWorldScale(0.002) -- 800 x 600 units: a 1.6 x 1.2 m plate
plate:GetTransform():SetWorldPosition(Vector3.new(0, 1.5, 3))

self.ui = UI.CreateController(plate)
self.ui:SetCamera(Scenes.GetCurrentScene():FindActorByName("Main Camera"))

-- From OnLateUpdate, once the camera has moved
self.ui:Update(deltaTime)
```

Panels are created and laid out as on any canvas, in canvas units with the origin at the top-left.

### Mouse

The cursor is a ray cast from the camera given to `SetCamera`, through the mouse, onto the plane.

- A panel is hovered only on the front of the canvas, in front of the camera.
- While a button is held, the cursor keeps following the plane beyond the edges of the canvas, so a
  drag doesn't stop at the border.
- Without a camera, nothing is hovered and a warning is logged once.
- The ray isn't stopped by the scene: a wall standing in front of the canvas hides it but doesn't keep
  it from being clicked.

### Positions in the world

| Method | Description |
|---|---|
| `controller:IsWorldSpace()` | Whether the canvas is a world space canvas |
| `controller:GetCanvasFrame()` | The plane, read once per update: centre, right, up and normal axes, rotation, `scale` (world units per canvas unit), size in canvas units. nil on a screen space canvas |
| `controller:CanvasToWorld(x, y)` | World position of a canvas position |
| `controller:ViewportToCanvas(x, y)` | Canvas position under a pixel: a ray on a world space canvas, nil when it misses |
| `panel:LocalToWorld(x, y)` | World position of a position local to the panel |
| `panel:GetWorldRect()` | Centre, `right` and `up` axes as laid out on the canvas, `normal`, `width` and `height` in world units |

### 3D content: `DModelPanel`

A panel carrying a 3D model: a plate behind a menu, a key under a label, an item standing in its
slot. Every update, once the layout is final, the model is centred on the panel, turned like the
canvas and the panel on it, then by its own rotation, and pushed along the normal by its depth. It is
shown only while the panel is, and hidden on a screen space canvas. The panel takes no cursor and owns
its model: the model is destroyed when replaced or when the panel is removed.

| Method | Description |
|---|---|
| `SetModel(actor)` | Carries the actor, destroying the previous model |
| `SetModelPrefab(path)` | Instantiates the prefab and carries it, returns the model |
| `SetDepth(depth)` | World units in front of the canvas; negative values sink it behind |
| `SetModelRotation(rotation)` | Rotation in the frame of the panel: X right, Y up, Z towards the viewer |
| `SetModelScale(scale)` | A number or a `Vector3` |
| `SetFillRect(fill)` | Also scales X and Y by the world width and height of the panel, for a model one unit wide and tall |

```lua
-- A slab one unit wide and tall standing behind the whole menu, 1 cm thick
local back = menu:Add("DModelPanel")
back:SetSize(menu:GetSize())
back:SetModelPrefab("Prefabs/UI/Slab.ovprefab")
back:SetFillRect(true)
back:SetModelScale(Vector3.new(1, 1, 0.01))
back:SetDepth(-0.006)
back:SetZPos(-1)

-- An item model standing out of its slot
local item = slot:Add("DModelPanel")
item:SetSize(slot:GetSize())
item:SetModelPrefab("Prefabs/Items/Sword.ovprefab")
item:SetModelRotation(Quaternion.new(Vector3.new(0, 0, 45)))
item:SetModelScale(0.4)
item:SetDepth(0.02)
```

World panels (`DWorldPanel`, `DWorldPath`, `DWorldArc`) follow the screen projection of the world and
can only be created on a screen space canvas.

## World panels

### Setup

The controller needs the camera rendering the scene. Lua can't query the camera the engine picks
(see [LuaExposureGaps.md](LuaExposureGaps.md)), so it is given by the script:

```lua
self.ui = UI.CreateController()
self.ui:SetCamera(Scenes.GetCurrentScene():FindActorByName("Main Camera"))
```

World panels are placed during `controller:Update`. If the camera or the targets move in `OnUpdate`,
call `controller:Update` from `OnLateUpdate`, otherwise the panels lag one frame behind.

World panels are drawn in the order of their siblings, not by distance. Put them in a `DWorldLayer`
to draw the farthest first. The layer covers its parent, so its panels can be clicked anywhere.

### Anchors

`DWorldPanel`, `DWorldPath` and `DWorldArc` share an anchor:

| Method | Description |
|---|---|
| `SetTarget(actor, offset)` | Follows an actor, with an optional `Vector3` offset |
| `SetWorldPosition(position)` | Stays at a fixed world position |
| `SetOffset(offset)` | Offset from the target or the position |
| `SetSpace("world" \| "target")` | Axes of the offset and of the points: world axes (default), or right, up and forward of the target, without its scale |
| `GetDepth()` | Depth in front of the camera during the last update, nil when behind |
| `OnTargetLost()` | Called once when the target is destroyed, removes the panel by default |

A panel without anchor, without camera, or whose target is gone is hidden. Its visibility
(`SetVisible`) isn't changed.

### `DWorldPanel`

A container placed on its anchor. Its children are ordinary panels and receive the mouse.

| Method | Description |
|---|---|
| `SetAlignment(x, y)` | Point of the panel placed on the anchor, as fractions of its size. `(0.5, 1)` by default, the middle of the bottom edge |
| `SetDistanceScale(reference, min, max)` | Scale of `reference / depth`, clamped. nil disables it |
| `SetMaxDistance(distance)` | Hidden beyond that distance |
| `SetClampToScreen(margin)` | Keeps the whole panel on the canvas when the anchor is off-screen or behind the camera |
| `IsClamped()`, `GetEdgeAngle()` | Whether the panel is held on an edge, and the direction of the anchor for an arrow |
| `SetOcclusionCheck(enabled, interval)` | Hidden when a collider other than the target is between the camera and the anchor |

```lua
-- Name above a character, shrinking with the distance
local tag = layer:Add("DWorldPanel")
tag:SetTarget(npc, Vector3.new(0, 2.1, 0))
tag:SetSize(240, 40)
tag:SetDistanceScale(5, 0.4, 1.5)
tag:SetMaxDistance(40)
tag:SetOcclusionCheck(true)

local name = tag:Add("DLabel")
name:SetSize(240, 40)
name:SetText("Wheatley")
name:SetContentAlignment(5)
```

```lua
-- Objective marker held on the screen edges, with an arrow towards the target
local marker = layer:Add("DWorldPanel")
marker:SetWorldPosition(Vector3.new(40, 0, 12))
marker:SetSize(48, 48)
marker:SetAlignment(0.5, 0.5)
marker:SetClampToScreen(16)

local arrow = marker:Add("DPanel")
arrow:SetSize(48, 48)
arrow:SetBackgroundImage(Resources.GetTexture("Textures/Arrow.png"))
arrow:SetTransformOrigin(0.5, 0.5)
arrow.Think = function(self)
	self:SetVisible(marker:IsClamped())
	self:SetRotation(marker:GetEdgeAngle())
end
```

### `DWorldPath` and `DWorldArc`

A line through 3D points, drawn with `DPolyline`. It is cut at the near plane of the camera.

| Method | Description |
|---|---|
| `SetPoints(points)`, `SetClosed(closed)` | Points relative to the anchor (`DWorldPath` only) |
| `SetThickness(thickness, inWorldUnits)` | In canvas units, or in world units so it thins with the distance |
| `SetColor(color)`, `SetBackgroundColor(color)` | Color of the drawn part, and of the whole path below it (none when nil) |
| `SetFraction(fraction)` | Draws the first part of the path, measured in the world: a bar |
| `SetFrontOnly(frontOnly)` | Hides what is farther than the anchor, since the body of a character can't hide it |
| `SetTexture(texture)` | Texture stretched along each segment |

`DWorldArc` generates an arc around the anchor:

| Method | Description |
|---|---|
| `SetRadius(radius)`, `SetSegments(count)` | World radius, number of segments (32 by default) |
| `SetAxis(axis)` | Axis the arc turns around, up by default |
| `SetArc(centerAngle, sweep)` | Center and length in degrees. The fraction fills from `centerAngle - sweep / 2` |
| `SetFaceCamera(faceCamera)` | Angle 0 points towards the camera, so the arc keeps facing the viewer |

```lua
-- Curved health bar around the player, facing the camera
local bar = layer:Add("DWorldArc")
bar:SetTarget(player, Vector3.new(0, 1.2, 0))
bar:SetRadius(0.6)
bar:SetArc(0, 120)
bar:SetSegments(24)
bar:SetFaceCamera(true)
bar:SetThickness(0.05, true)
bar:SetColor(Vector4.new(0.3, 0.9, 0.3, 1))
bar:SetBackgroundColor(Vector4.new(0, 0, 0, 0.5))

function Player:OnHealthChanged(health)
	bar:SetFraction(health / self.maxHealth)
end
```

```lua
-- Selection ring on the ground, only its front half over the character. Facing the camera puts the
-- start and the end of the circle behind the character, so the front half is drawn in one piece.
local ring = layer:Add("DWorldArc")
ring:SetTarget(unit, Vector3.new(0, 0.05, 0))
ring:SetRadius(1)
ring:SetThickness(0.04, true)
ring:SetFaceCamera(true)
ring:SetFrontOnly(true)
```

## Transforms and `DPolyline`

Every panel can be rotated and scaled, and hit-testing follows:

- `SetRotation(degrees)`: counter-clockwise on screen.
- `SetScale(x, y)`.
- `SetTransformOrigin(x, y)`: the point they turn around, the top-left corner by default.

`DPolyline` draws a 2D line in its own coordinates. `SetArc` builds curved HUD bars.

## Cost

A path segment is a panel with an Image: about four component calls per segment and per frame. 16 to
32 segments are enough for an arc. For many characters, prefer `DWorldPanel` with a flat bar, and
`SetMaxDistance`. Each target costs an `IsAlive` check per frame, which searches the scene actors.
