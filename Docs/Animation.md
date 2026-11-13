# Animation

## Bones

- `GetBoneIndex(name)`, then `GetBoneGlobalPosition/Rotation/Scale(index)`, return values in the model's own space,
  and `nil` until the first pose. Convert them to world space through the model's actor.
- Writing a bone (`SetBoneLocalPosition/Rotation/Scale`) only marks the skeleton: a thousand writes in a frame work it
  out once (see [When the pose is worked out](#when-the-pose-is-worked-out)).
- A mesh hung on a bone without skin weights (a helmet, a hood) follows that bone: the importer binds it to its nearest
  bone ancestor. Under an animated node that is not a bone, it only follows that ancestor.
- A `BoneAttachment` makes an actor ride a bone, placed by the engine after every script.

## Layers

- Every animation layer plays clips from one source model (`SetAnimationSourceModel`).
- A layer's settings (`SetLayerWeight`, `SetTime`, `SetAnimation`, `SetLayerMask`, `SetAnimationSourceModel`, `Play`,
  `Stop`) only mark the pose: changed a hundred times in a frame, it is sampled once.
- `SetLayerMask(rootName, layer)` makes a layer drive only the node it names and its descendants; `""` clears it, an
  unknown name warns and leaves the layer whole. Weights are normalised node by node, so a masked clip at full weight
  shares the masked part with the layers below, while the rest stays theirs.

## Clips on other rigs

- **Matching**: each of the model's bones is driven by the clip's bone of the same name, or of the name the layer's
  bone map gives it: `SetAnimationBoneMap({ ModelBone = "ClipBone", ... }, layer)`, `GetAnimationBoneMap(layer)`,
  `bone_map` in a file. A bone the map does not name keeps its own name; an empty map matches every bone by name.
- **What must match**: the parents (the model's bone hangs under the bone that the clip's matching bone hangs under)
  and the rest scales.
- **What may differ**:
  - the rest positions: each bone starts from the model's rest and makes the clip's move away from the clip's rest,
    scaled by the ratio of the two rest lengths (a longer leg dips its hips further);
  - the rest rotations: a key is carried through the two rests, so the bone turns in the model's space as the clip's
    turns in its own. A rig exported with other axes, or with bones rolled another way, plays the clip;
  - scale keys are taken as they are.
- **A refusal names the bone**: the warning says why a source cannot play, and so do
  `GetAnimationSourceError(layer)` and the layer in the inspector. No animation plays from a refused source.
- **The same rig** plays its clips exactly as before: nothing is carried through when the rests are the same.
- **Bones of the model's own** (hair, cloth) may hang under a bone that both its rig and the clip's have. No track
  drives them: they keep their bind pose for scripts to turn, and the engine puts them back at rest whenever it
  evaluates the pose, so a script that turns them writes them every frame. A bone the clip has and the model lacks,
  and bones directly under the armature, are still refused: a renamed bone needs the bone map.

## Animator

The `Animator` component plays an animation graph on its actor's skinned mesh: a JSON file of the project
(`.ovanim`) whose layers hold states and the transitions between them, driven by parameters a script
sets.

- **Setting it up**: `SetGraphPath("Animations/Hero.ovanim")`, and the model whose clips the graph plays
  (`SetAnimationSourceModel`, the rendered model by default), with a bone map for clips of another rig (`SetBoneMap`, as
  for a layer, see [Clips on other rigs](#clips-on-other-rigs)). In a file: `graph`, `animation_source`, `bone_map`.
- **While it plays**, the skinned mesh renderer samples the graph's clips instead of its own layers, which come back
  when the animator is disabled or removed. Its pose is worked out at the renderer's evaluation rate, and at once when
  a state changes or a script changes the graph.
- **When**: once a frame, after every `OnUpdate` and before every `OnLateUpdate`: a parameter set in `OnUpdate` shows in
  the same frame, and `OnLateUpdate`, the looks and the spring bones see the graph's pose.
- **Parameters**: `SetFloat`, `SetBool`, `SetTrigger` (set until a transition uses it up) and `ResetTrigger`, each
  returning `false` for an unknown name or a parameter of another kind; `GetFloat`, `GetBool`, `GetParameterNames`.
- **Layers**: `GetState(layer)` (the state it is in, or going to while it fades), `GetStateTime(layer)` (normalized, 1
  per loop), `IsInTransition(layer)`, `SetLayerWeight(layer, weight)`, `GetLayerNames`. A layer is given by its index
  from 0 or by its name, the first one when none is given.
- **From a script**: `Play(state[, fade[, layer]])` moves to a state at once, whatever the transitions say; `Reset()`
  puts every parameter and layer back to its default; `Reload()` reads the file again.
- **A file that cannot play** is named, with the JSON path of the mistake, in a warning, in `GetError()` and in the
  inspector, and plays nothing: an unknown member, a missing name, an unknown state or parameter, a clip the model
  lacks (`layers[0].states[1].clip: no clip 'Run' in 'Animations/Rig.fbx'`).
- **The inspector** shows each layer's state and every parameter, which can be changed while the game plays.
- **In the editor**, the Asset Browser lists the `.ovanim` files: the Graph field picks one or takes one dropped on it,
  "Open" edits it in the code editor ("Reload Graph" reads it again), and renaming one updates the animators that use it.

### The graph file

```json
{
	"parameters": [
		{ "name": "Speed", "type": "float", "default": 0 },
		{ "name": "Grounded", "type": "bool", "default": true },
		{ "name": "Attack", "type": "trigger" }
	],
	"layers": [
		{
			"name": "Base",
			"states": [
				{ "name": "Locomotion", "blend": { "parameter": "Speed", "motions": [
					{ "clip": "Idle", "position": 0 },
					{ "clip": "Walk", "position": 1.5 },
					{ "clip": "Run", "position": 5 }
				] } },
				{ "name": "Fall", "clip": "Fall" },
				{ "name": "Land", "clip": "Land", "loop": false }
			],
			"transitions": [
				{ "from": "Locomotion", "to": "Fall", "conditions": [ { "parameter": "Grounded", "value": false } ], "duration": 0.15 },
				{ "from": "Fall", "to": "Land", "conditions": [ { "parameter": "Grounded" } ], "duration": 0.05 },
				{ "from": "Land", "to": "Locomotion", "exitTime": 0.8, "duration": 0.2 }
			]
		},
		{
			"name": "UpperBody",
			"mask": "spine_02",
			"states": [
				{ "name": "Rest" },
				{ "name": "Slash", "clip": "Slash", "loop": false, "speed": 1.2 }
			],
			"transitions": [
				{ "from": "*", "to": "Slash", "conditions": [ { "parameter": "Attack" } ], "duration": 0.1 },
				{ "from": "Slash", "to": "Rest", "exitTime": 1, "duration": 0.25 }
			]
		}
	]
}
```

- **Parameters**: `name`, `type` (`"float"`, `"bool"` or `"trigger"`) and `default` (none for a trigger).
- **Layers**, in order, each over the ones before: `name`, `weight` (0 to 1, 1 by default), `mask` (a bone: the layer
  plays on it and the bones under it only), `default` (its first state otherwise), `states`, `transitions`. A layer
  covers the layers before it by its weight, on the bones its clips drive.
- **States**: `name`, then
  - `clip`: one clip of the source model;
  - or `blend`: clips placed along a number `parameter` (`motions`, each a `clip` and a `position`), the two around the
    parameter's value weighed by how near they are, their times kept in step (a walk and a run put their feet down
    together);
  - or neither: an empty state, which plays nothing and lets the layers before show through (an upper body at rest);
  - and `speed` (1 by default), `speedParameter` (a number parameter it is multiplied by), `loop` (true by default; a
    state that does not loop holds its last frame).
- **Transitions**: `from` (a state, or `"*"` for any state but the target), `to`, `conditions`, `duration` (the fade,
  in seconds, 0.25 by default) and `exitTime` (in the state's normalized time: within a loop, it comes back every
  loop; past 1, it counts loops). A transition needs conditions, an exit time, or both.
  - A **condition** names its `parameter`: a number takes an `op` (`">"`, `">="`, `"<"`, `"<="`, `"=="`, `"!="`) and a
    `value`; a boolean a `value` (true by default) and `"=="` (by default) or `"!="`; a trigger nothing else, and the
    transition uses it up.
  - Each frame, a layer takes the first transition that holds: from any state first, then from its state, in the
    file's order. It may leave a state while still fading into it: the fade then starts from the blend it had
    reached, without a jump.

## Look at

The `LookAt` component ("Look At" in the editor) turns a chain of bones of its actor's skinned mesh towards a target,
over the animation.

- **The bones**: `SetBones({ { "spine_02", 0.2 }, { "neck_01", 0.3 }, { "head", 0.5 } })`, from the root to the tip,
  each hanging under the one before; the parts are taken over their sum. Each bone turns its share on top of the
  ones before it, so the tip (the head) looks the whole way and the bones before it make the turn gradual.
- **The target**: an actor (`SetTarget`), or a point in the world (`SetTargetPosition`), plus an offset
  (`SetTargetOffset`, an actor's eyes above its feet). `ClearTarget()` lets the look go back to the front.
- **The look**: a yaw and a pitch in the model's space, from the front (+Z, the engine's forward, as
  `Transform:GetWorldForward`) and up (+Y), measured from the tip's joint.
  - Within bounds: `SetMaxYaw` either side (70 degrees by default), `SetMinPitch` down (-30), `SetMaxPitch` up (40).
    A target behind is looked at as far as the bound on its side goes.
  - On a critically damped spring: `SetTurnTime` (0.22 s by default, 0 turns at once) and `SetMaxTurnSpeed`
    (degrees a second, 0 for no limit). It reaches the target without passing it.
  - While it has a target, the look holds its direction in the world as the body turns under it.
- **The weight**: `SetWeight(0 to 1)` reached over `SetFadeTime` (0.3 s), to switch the look on and off smoothly;
  `GetCurrentWeight()` is where the fade is.
- **Reading it**: `GetYaw()` and `GetPitch()` (degrees, weighted), `GetLookDirection()` (where the eyes point, in the
  world), `IsAtRest()` (at the front and still), `Reset()`, and `GetError()` (no skinned mesh, an unknown bone, bones
  that make no chain), which the inspector shows too.
- In a file: the bones, the bounds and speeds, the weight, the fade time and the offset. The target is set by a
  script.

## Spring bones

The `SpringBones` component ("Spring Bones" in the editor) swings chains of bones of its actor's skinned mesh (hair,
a cape, a tail) as the body moves, over the animation.

- **Groups**: `SetGroup(name, settings)` adds a group or changes the settings given, the others keeping theirs:
  - `stiffness` (how hard a joint is drawn back to its rest, 150), `damping` (how fast its swing dies, 8),
    `gravity` (a share of the world's, 0.3), `radius` (the joints' thickness, metres, 0.025), `maxAngle` (how far a
    bone strays from its rest, degrees, 45), `link` (0 to 1, how much the chains of the group hold their spacing,
    as the columns of a cape, 0);
  - an unknown setting raises an error.
- **Chains**: `SetGroupChains(group, { "cape.a.1", "cape.b.1" })`, each named by the bone it starts at, side by side.
  A chain runs down the only child bone of each of its bones, and needs two bones at least. Its bones point along
  their rests (not along an axis): the rig needs no convention.
- **Colliders**: `SetCollider(name, { from = "hips", to = "chest", radius = 0.22 })`, a capsule from one bone to
  another; or `{ from = "head", away = "chest", start = 0.16, ["end"] = 0.22, radius = 0.24 }`, from a bone outwards
  between two distances. `SetGroupColliders(group, names)` says which a group meets. They follow the body's bones as
  the animation (and the looks) leave them.
- **The ground**: `SetGround(true, offset)` keeps the joints above a plane at the actor's height plus the offset. It
  wins over the colliders.
- **The simulation** (`OvCore::Animation::SpringSolver`): Verlet in the world, in steps of 1/120 s at most (8 in a
  frame at most), damped towards the body's own move; each joint drawn back to its rest, kept at its bone's length,
  pushed out of the capsules, held within its angle, then above the ground. A frame at 30 or at 144 frames a second
  ends nearly in the same place.
- **Rest and reset**: a model no render pass (a view, the shadows, a reflection) drew the frame before rests and does no
  work (`SetSleepWhenHidden`, on by default; `IsAsleep()`), and starts again from rest once seen. In play in the
  editor, the Scene View is a view too. So does a body that jumps further in a frame than a
  run would take it (a respawn, a teleport), and `Reset()`.
- **Reading it**: `GetChainCount()`, `GetJointPosition(chain, bone)` (the end of a bone as simulated, in the world),
  and `GetError()` (an unknown bone or collider, a chain of one bone), which the inspector shows too.

## Look at and spring bones in the frame

- They apply once a frame, after every script (and the animators) (`OnLateUpdate`, `Events.AddLateUpdate`) and before the skeleton is
  worked out and the bone attachments placed: the looks first, then the spring bones, which hang from the bones as
  the looks turned them.
- They start from the frame's pose, the clips' and the scripts', never from what they wrote the frame before: the
  renderer keeps their rotations apart from that pose, and a script reading a bone in its `OnUpdate` gets the bone
  as they left it the frame before.
- A model whose only motion is theirs (no clip) is skinned while they turn it.

## When the pose is worked out

- **On demand, once per change**: the renderer samples its clips into a pose, then works the skeleton (global
  transforms, skinning matrices) out of it, only when something needs them:
  - a script reading a bone (`GetBoneLocal*`, `GetBoneGlobal*`);
  - the scene, once a frame after every `OnLateUpdate` and `Events.AddLateUpdate` callback and the looks and spring
    bones, before bone attachments are placed;
  - the drawing, in the editor outside play.
- **In the order a script makes its calls**: a write applies what was set before it. A weight changed and then a
  bone turned gives the bone turned; a bone turned and then a weight changed gives the clips' pose, as the weight's
  pose replaces the bone written before it.
- **The clips are sampled at most 60 times a second** (`Pose Eval Rate` in the inspector), as before.
- **The update mode** (`SetUpdateMode`, "Update Mode" in the inspector, `update_mode` in a file):
  - `AnimationUpdateMode.ALWAYS`, the default: every frame, seen or not.
  - `AnimationUpdateMode.WHEN_VISIBLE`: only while a render pass (a view, the shadows, a reflection) drew the model
    the frame before (`WasVisible()`). Out of sight:
    - its clips' time runs on, so it is where it should be when it comes back;
    - a script reading a bone still gets the right pose, worked out for that read;
    - its bone attachments keep the place they last had;
    - back in view, it is worked out from the next frame: the first frame shows the pose it left with.
  - It plays a part only while the scene runs: in the editor outside play, every model is worked out. In play, the
    editor's Scene View is a view too: a model it shows is visible.
- **Diagnostics**: `GetPoseEvaluationCount()` and `GetSkeletonUpdateCount()` count the poses sampled and the skeletons
  worked out since the component was made.
