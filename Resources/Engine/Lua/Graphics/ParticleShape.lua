---@meta

--- Where a particle system's particles are born, and the way they leave. The cone and the hemisphere open along the
--- emitter's up axis (local +Y), and a box sends its particles that way
---@enum ParticleShape
ParticleShape = {
    POINT = 0,
    SPHERE = 1,
    HEMISPHERE = 2,
    CONE = 3,
    BOX = 4,
}
