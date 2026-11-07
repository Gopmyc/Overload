# Animation

## Bones

- `GetBoneIndex(name)`, then `GetBoneGlobalPosition/Rotation/Scale(index)`, return values in the model's own space,
  and `nil` until the first pose. Convert them to world space through the model's actor.
- `SetBoneLocalRotation` recomputes the whole skeleton at each call: write a bone once a frame.
- A mesh hung on a bone without skin weights (a helmet, a hood) follows that bone: the importer binds it to its nearest
  bone ancestor. Under an animated node that is not a bone, it only follows that ancestor.
- A `BoneAttachment` makes an actor ride a bone, placed by the engine after every script.

## Layers

- Every animation layer plays clips from one source model (`SetAnimationSourceModel`).
- `SetLayerWeight` is not free: every call makes the renderer evaluate the whole pose again. Write a weight or a
  playback speed only when it changes.
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
