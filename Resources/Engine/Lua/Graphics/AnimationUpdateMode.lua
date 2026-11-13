---@meta

--- When a skinned mesh renderer works its pose out during play
---@enum AnimationUpdateMode
AnimationUpdateMode = {
    --- Every frame, seen or not
    ALWAYS = 0,
    --- Only while a render pass (a view, a shadow, a reflection) drew the model the frame before
    WHEN_VISIBLE = 1,
}
