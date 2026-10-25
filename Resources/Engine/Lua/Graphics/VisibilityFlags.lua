---@meta

--- Flags naming the passes a renderer is drawn in, combined with the bitwise or (`|`)
---@enum VisibilityFlags
VisibilityFlags = {
    NONE = 0,
    GEOMETRY = 1,
    SHADOW = 2,
    REFLECTION = 4,
    ALL = 7,
}
