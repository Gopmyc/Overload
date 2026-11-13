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

- A clip plays on a model whose bones have the same names, parents, rest rotations and rest scales, whatever their rest
  positions: each bone starts from the model's rest and makes the clip's move away from the clip's rest, scaled by the
  ratio of the two rest lengths. Rest rotations and scales must match: a rig turned or scaled otherwise is refused
  (warning, no animation).
- A model may carry bones of its own (hair, cloth) under a bone that both its rig and the clip's have. No track drives
  them: they keep their bind pose for scripts to turn, and the engine puts them back at rest whenever it evaluates the
  pose, so a script that turns them writes them every frame. A bone the clip has and the model lacks, a renamed bone,
  or bones directly under the armature are still refused.

## When the pose is worked out

- **On demand, once per change**: the renderer samples its clips into a pose, then works the skeleton (global
  transforms, skinning matrices) out of it, only when something needs them:
  - a script reading a bone (`GetBoneLocal*`, `GetBoneGlobal*`);
  - the scene, once a frame after every `OnLateUpdate` and `Events.AddLateUpdate` callback, before bone attachments
    are placed;
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
