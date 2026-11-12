---@meta

--- Emits particles the GPU draws, all of them in one draw: billboards by default, or any model with a material whose
--- shader reads the particles. The settings change what is born from then on; a new maximum clears the system
---@class ParticleSystem : Component
ParticleSystem = {}

--- Returns the actor that owns this component
---@return Actor
function ParticleSystem:GetOwner() end

--- Starts emitting from the start of a cycle
function ParticleSystem:Play() end

--- Stops emitting: the particles alive live on
function ParticleSystem:Stop() end

--- Removes every particle
function ParticleSystem:Clear() end

--- Returns true while the system emits
---@return boolean
function ParticleSystem:IsPlaying() end

--- Returns the particles alive
---@return integer
function ParticleSystem:GetAliveCount() end

--- Emits particles now, playing or not, and returns how many were born (no more than the maximum). The overrides
--- apply to each of them: `position` and `direction` (Vector3, in the world, instead of the shape's), `color`
--- (Vector4, multiplied by the colour over life), `speed`, `size` (times the size over life) and `lifetime`
---@param count integer
---@param overrides? { position?: Vector3, direction?: Vector3, color?: Vector4, speed?: number, size?: number, lifetime?: number }
---@return integer
function ParticleSystem:Emit(count, overrides) end

--- Returns the model each particle is drawn as, or nil for a billboard
---@return Model|nil
function ParticleSystem:GetModel() end

--- Sets the model each particle is drawn as, or nil for a billboard. Its material's shader must place the particles
--- (Common/Particles.ovfxh)
---@param model Model|nil
function ParticleSystem:SetModel(model) end

--- Returns the material set, or nil while the engine's (:Materials/Particle.ovmat) is used
---@return Material|nil
function ParticleSystem:GetMaterial() end

--- Sets the material the particles are drawn with, or nil for the engine's
---@param material Material|nil
function ParticleSystem:SetMaterial(material) end

--- Returns how many particles may live at once
---@return integer
function ParticleSystem:GetMaxParticles() end

--- Sets how many particles may live at once (1 to 100000): one born past it replaces the oldest. Clears the system
---@param count integer
function ParticleSystem:SetMaxParticles(count) end

--- Returns the particles born per second while playing
---@return number
function ParticleSystem:GetRate() end

--- Sets the particles born per second while playing
---@param rate number
function ParticleSystem:SetRate(rate) end

--- Returns the bursts of each cycle
---@return { time: number, count: integer }[]
function ParticleSystem:GetBursts() end

--- Sets the bursts of each cycle: so many particles born at once, a time after the cycle starts
---@param bursts { time: number, count: integer }[]
function ParticleSystem:SetBursts(bursts) end

--- Returns the seconds of a cycle
---@return number
function ParticleSystem:GetDuration() end

--- Sets the seconds of a cycle
---@param duration number
function ParticleSystem:SetDuration(duration) end

--- Returns true if a cycle follows the last one
---@return boolean
function ParticleSystem:IsLooping() end

--- Sets whether a cycle follows the last one, or the system stops emitting after one
---@param looping boolean
function ParticleSystem:SetLooping(looping) end

--- Returns true if the system plays when the scene starts
---@return boolean
function ParticleSystem:GetPlayOnStart() end

--- Sets whether the system plays when the scene starts
---@param playOnStart boolean
function ParticleSystem:SetPlayOnStart(playOnStart) end

--- Returns the shortest and the longest life of a particle, in seconds
---@return number min
---@return number max
function ParticleSystem:GetLifetime() end

--- Sets the life of a particle, in seconds, drawn between the two (max defaults to min)
---@param min number
---@param max? number
function ParticleSystem:SetLifetime(min, max) end

--- Returns the slowest and the fastest speed at birth
---@return number min
---@return number max
function ParticleSystem:GetSpeed() end

--- Sets the speed at birth, in units per second, drawn between the two (max defaults to min)
---@param min number
---@param max? number
function ParticleSystem:SetSpeed(min, max) end

--- Returns where particles are born
---@return ParticleShape
function ParticleSystem:GetShape() end

--- Sets where particles are born, and the way they leave
---@param shape ParticleShape
function ParticleSystem:SetShape(shape) end

--- Returns the radius of a sphere, a hemisphere, or a cone's base
---@return number
function ParticleSystem:GetShapeRadius() end

--- Sets the radius of a sphere, a hemisphere, or a cone's base
---@param radius number
function ParticleSystem:SetShapeRadius(radius) end

--- Returns the angle of a cone from its axis, in degrees
---@return number
function ParticleSystem:GetShapeAngle() end

--- Sets the angle of a cone from its axis, in degrees (0 to 180)
---@param angle number
function ParticleSystem:SetShapeAngle(angle) end

--- Returns the size of a box
---@return Vector3
function ParticleSystem:GetShapeSize() end

--- Sets the size of a box
---@param size Vector3
function ParticleSystem:SetShapeSize(size) end

--- Returns the acceleration of the particles
---@return Vector3
function ParticleSystem:GetGravity() end

--- Sets the acceleration of the particles, in units per second squared (in the emitter's axes for a local system)
---@param gravity Vector3
function ParticleSystem:SetGravity(gravity) end

--- Returns the share of their speed the particles lose per second
---@return number
function ParticleSystem:GetDrag() end

--- Sets the share of their speed the particles lose per second, exponentially: with gravity, they reach a terminal
--- speed of gravity / drag
---@param drag number
function ParticleSystem:SetDrag(drag) end

--- Returns the size of a particle at birth and at death
---@return number start
---@return number finish
function ParticleSystem:GetSize() end

--- Sets the size of a particle over its life, in units (finish defaults to start)
---@param start number
---@param finish? number
function ParticleSystem:SetSize(start, finish) end

--- Returns the colour of a particle at birth and at death
---@return Vector4 start
---@return Vector4 finish
function ParticleSystem:GetColor() end

--- Sets the colour of a particle over its life, alpha included (finish defaults to start)
---@param start Vector4
---@param finish? Vector4
function ParticleSystem:SetColor(start, finish) end

--- Returns the shares of the life over which a particle fades in and out
---@return number fadeIn
---@return number fadeOut
function ParticleSystem:GetFade() end

--- Sets the shares of the life (0 to 1) over which a particle's opacity rises from 0, and falls to 0
---@param fadeIn number
---@param fadeOut number
function ParticleSystem:SetFade(fadeIn, fadeOut) end

--- Returns the slowest and the fastest turning speed, in degrees per second
---@return number min
---@return number max
function ParticleSystem:GetSpin() end

--- Sets the turning speed, in degrees per second, drawn between the two (max defaults to min)
---@param min number
---@param max? number
function ParticleSystem:SetSpin(min, max) end

--- Returns true if a particle is born at a random angle
---@return boolean
function ParticleSystem:HasRandomRotation() end

--- Sets whether a particle is born at a random angle
---@param randomRotation boolean
function ParticleSystem:SetRandomRotation(randomRotation) end

--- Returns the space the particles live in
---@return ParticleSpace
function ParticleSystem:GetSpace() end

--- Sets the space the particles live in: the world, or the emitter's, which they follow
---@param space ParticleSpace
function ParticleSystem:SetSpace(space) end

--- Returns true if the system runs on real time
---@return boolean
function ParticleSystem:IsUnscaled() end

--- Sets whether the system runs on real time, going on while the game is paused, instead of game time
---@param unscaled boolean
function ParticleSystem:SetUnscaled(unscaled) end

--- Returns the seed
---@return integer
function ParticleSystem:GetSeed() end

--- Sets the seed: the same seed gives the same particles, 0 gives every system its own
---@param seed integer
function ParticleSystem:SetSeed(seed) end
