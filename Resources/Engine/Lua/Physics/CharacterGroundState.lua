---@meta

--- What holds a CharacterController
---@enum CharacterGroundState
CharacterGroundState = {
    --- Ground it can stand on
    ON_GROUND = 0,
    --- Ground steeper than its maximum slope: it slides down
    ON_STEEP_GROUND = 1,
    --- Something it touches but that cannot hold it
    NOT_SUPPORTED = 2,
    --- Nothing
    IN_AIR = 3,
}
