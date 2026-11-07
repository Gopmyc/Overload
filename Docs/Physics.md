# Physics

The physics is Jolt Physics 5.6.0. It steps at a fixed 1/60 s; `OnFixedUpdate` runs after each step.

## Bodies

- `PhysicalBox`, `PhysicalSphere`, `PhysicalCapsule` and `PhysicalHeightField` are the colliders.
- A capsule's `height` is its straight part: the body is `height + 2 × radius` tall, centred on its actor.
- A collider scales with its actor: a capsule's radius follows the X scale and its straight part the Y scale.
- A linear or angular factor only frees or locks an axis: 0 locks it, anything else frees it whole. A body with no axis
  free is simulated as kinematic.
- A static body (`SetStatic(true)`, `static` in a file) never moves by itself and only follows its transform when the
  transform changes; it ignores the kinematic setting. Level geometry and props stand on static bodies.
- A height field (`Actor:AddPhysicalHeightField`, then `SetHeights(sampleCount, spacing, heights)`) is static. The heights
  are row-major, along +X first, then +Z: the sample (x, z) is at index `z * sampleCount + x + 1`, at
  `(x * spacing, height, z * spacing)` from the actor, each cell cut along its (x, z)–(x + 1, z + 1) diagonal. It
  returns `true`, or `false` and why. Bodies rest about 2 cm into it.

## Collision layers

- Every physical object is on one of 16 collision layers, 0 to 15: `SetCollisionLayer(layer)` and
  `GetCollisionLayer()`, `collision_layer` in a file (0 when absent), a list in the inspector. Out of range, a layer is
  clamped.
- By default every layer collides with every other. The project names the layers and says which pairs go through each
  other (see [Projects](Projects.md)):

  ```ini
  physics_layer_names=Default,Player,Enemy,Projectile,Debris
  physics_ignored_layer_pairs=Player:Projectile, Debris:Debris, Debris:Player
  ```

  A pair names its layers by name or by index. An entry that cannot be read is logged and skipped.
- From Lua: `Physics.GetLayerIndex(name)` (`nil` when no layer has that name), `Physics.GetLayerName(layer)`,
  `Physics.DoLayersCollide(a, b)` and `Physics.SetLayersCollide(a, b, collide)`, which applies from the next physics
  step. Stopping play in the editor puts back the project's settings.
- `Physics.Raycast` sees every layer. A trigger only reports the objects of the layers it collides with.

## Triggers and contacts

- A trigger has no collision response. A collider can be switched to a trigger and back at run time
  (`PhysicalObject:SetTrigger`).
- `OnTriggerStay` comes once per physics step, so not every frame above 60 FPS, for every collider in the trigger that
  is not itself a trigger: a test of who stands in a trigger must allow for gaps.

## Moving bodies

- The physics step runs before the `OnFixedUpdate` that sets a velocity: reading the velocity back returns the order
  given, even against a wall. Measure real speed from displacement.
- A script that rewrites the linear velocity every step wipes any impulse; apply pushes through the same script.
- A body moved in `OnFixedUpdate` only moves on fixed steps: anything sampled per frame (a camera) must follow an
  interpolated visual, not the body itself.

## Queries

- `Physics.CastRay(origin, direction, distance[, layerMask, includeTriggers])` returns the objects the ray meets,
  nearest first, each once, as a table of `PhysicsHit`: `Object`, `Point`, `Normal` (out of the object) and
  `Distance`. Nothing met gives an empty table. A ray that starts inside an object meets it at distance 0.
- `Physics.CastSphere(origin, radius, direction, distance[, layerMask, includeTriggers])` sweeps a sphere the same way.
  An object the sphere overlaps at its origin is met at distance 0.
- `Physics.OverlapSphere(center, radius[, layerMask, includeTriggers])` and
  `Physics.OverlapBox(center, halfExtents[, rotation, layerMask, includeTriggers])` return the physical objects that
  overlap the shape, each once, in no particular order.
- A query sees every layer unless given a mask: `Physics.GetLayerMask("Default", "Enemy")` builds one from names or
  indices. The collision matrix plays no part in queries. Triggers are left out unless `includeTriggers` is `true`.
- Queries never return a ragdoll's bodies.
- `Physics.Raycast(origin, direction, distance)`, the older query, returns `nil` or a `RaycastHit` with
  `FirstResultObject` and `ResultObjects`: every object crossed, triggers included, in no particular order and without
  distances.

## Ragdolls

- `SkinnedMeshRenderer:StartRagdoll(parts, velocity)` hands the skeleton over to capsules joined by swing-twist
  constraints, from its current pose; `StopRagdoll`, `IsRagdoll`, `AddRagdollImpulse`.
- A ragdoll's bodies are on the project's `physics_ragdoll_layer`, 15 (`Ragdoll`) by default, and collide as that
  layer does. They raise no collision event, and raycasts never return them.
- With `physics_ragdoll_ignores_dynamic_bodies=true`, they also go through every dynamic body, so that a falling body
  never pushes nor trips a living one; they still fall on static and kinematic bodies and on each other. The option is
  off by default.
- While a ragdoll drives a skeleton, the animation does not; the bones it does not move keep their pose relative to
  their parent.
