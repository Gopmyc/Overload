# Particles

A particle system (the `ParticleSystem` component) emits particles the GPU draws: all of a system's particles in one
draw per mesh, whatever their number. Each particle is a record written once, at its birth: where it was, its
velocity, its life, its colour and size multipliers, its rotation. The shader works out where it is now, how large and
of which colour, from that record and the system's clock. No script and no CPU moves a particle once born.

## A system

Add it from the inspector (Add Component > Particle System), from the menu (Actors > Others > Particle System), or from
Lua:

```lua
local sparks = actor:AddParticleSystem()
sparks:SetMaterial(Resources.GetMaterial("Materials/Sparks.ovmat"))
sparks:SetRate(300)
sparks:SetLifetime(0.6, 1.2)
sparks:SetSpeed(4, 7)
sparks:SetGravity(Vector3.new(0, -9.81, 0))
sparks:SetSize(0.12, 0.03)
sparks:SetColor(Vector4.new(1, 0.75, 0.3, 1), Vector4.new(1, 0.2, 0.05, 0))
```

| Setting | Lua | In a scene file | What it does |
| --- | --- | --- | --- |
| Max particles | `Set`/`GetMaxParticles` | `max_particles` | Alive at once (1 to 100000). One born past it takes the oldest's place. A new maximum clears the system |
| Rate | `Set`/`GetRate` | `rate` | Born per second while playing, each at its own time within the frame |
| Bursts | `Set`/`GetBursts` | `bursts` | `{ time, count }`: so many born at once, that long after each cycle starts |
| Duration, looping | `Set`/`GetDuration`, `SetLooping`/`IsLooping` | `duration`, `looping` | A cycle's seconds; without looping, the system stops emitting after one |
| Play on start | `Set`/`GetPlayOnStart` | `play_on_start` | Plays when the scene starts |
| Lifetime, speed | `Set`/`GetLifetime(min, max)`, `Set`/`GetSpeed(min, max)` | `lifetime_min`, `lifetime_max`, `speed_min`, `speed_max` | Drawn between the two for each particle |
| Shape | `Set`/`GetShape`, `Set`/`GetShapeRadius`, `Set`/`GetShapeAngle`, `Set`/`GetShapeSize` | `shape`, `shape_radius`, `shape_angle`, `shape_size` | `ParticleShape.POINT`, `SPHERE`, `HEMISPHERE`, `CONE` (the default, 25°) or `BOX` |
| Gravity, drag | `Set`/`GetGravity`, `Set`/`GetDrag` | `gravity`, `drag` | An acceleration, and the share of their speed particles lose per second |
| Size, colour | `Set`/`GetSize(start, end)`, `Set`/`GetColor(start, end)` | `size_start`, `size_end`, `color_start`, `color_end` | Over a particle's life, from birth to death |
| Fades | `Set`/`GetFade(in, out)` | `fade_in`, `fade_out` | Shares of the life over which the opacity rises from 0, and falls to 0 |
| Spin | `Set`/`GetSpin(min, max)`, `SetRandomRotation`/`HasRandomRotation` | `spin_min`, `spin_max`, `random_rotation` | Degrees per second, and a random angle at birth |
| Space | `Set`/`GetSpace` | `space` | `ParticleSpace.WORLD`: particles stay where they were born. `LOCAL`: they follow the emitter |
| Unscaled | `SetUnscaled`/`IsUnscaled` | `unscaled` | Runs on real time, going on while the game is paused |
| Seed | `Set`/`GetSeed` | `seed` | The same seed gives the same particles; 0 gives every system its own |
| Model, material | `Set`/`GetModel`, `Set`/`GetMaterial` | `model`, `material` | What a particle is drawn as, and with; nil for billboards and the engine's material |

- The cone and the hemisphere open along the emitter's up axis (local +Y), and a box sends its particles that way. A
  sphere and a point send them every way.
- Drag is exponential. With gravity, a particle's speed eases towards a terminal speed, gravity / drag, as a falling
  spark or a rising puff of smoke does.
- In the local space, gravity acts along the emitter's axes.
- A setting changed applies to the particles born from then on, except the curves over life (size, colour, fades),
  gravity and drag, which the shader reads for every particle.

## Playing, and bursts from a script

- `Play()` starts emitting from the start of a cycle, `Stop()` stops emitting (the particles alive live on), and
  `Clear()` removes every particle. `IsPlaying()` and `GetAliveCount()` say where the system is.
- `Emit(count[, overrides])` emits at once, playing or not, and returns how many were born. Its overrides apply to
  each particle: `position` and `direction` (`Vector3`, in the world), `color` (`Vector4`, multiplied by the colour over
  life), `speed`, `size` (times the size over life) and `lifetime`. A spark where a blow lands:

  ```lua
  hits:Emit(12, { position = hit.point, direction = hit.normal, color = Vector4.new(1, 0.4, 0.2, 1) })
  ```

- A key the overrides do not know, or a count that is not a whole number, raises an error.
- A system added while the scene plays plays at once, unless its settings say otherwise: call `Stop()` to keep it for
  bursts.

## Time

- A system moves on once a frame, after every Behaviour's `OnLateUpdate`, the global `LateUpdate` event and the bone
  attachments: an emitter follows what moved it in that frame, a weapon on a hand included. Its rate is spread over
  the frame, along the way its emitter went.
- It runs on game time (`Time.SetScale` slows it, 0 stops it), or on real time when unscaled.
- An inactive actor's system waits, and is not drawn.
- In the editor, outside play, systems neither emit nor move: they show nothing until the game runs.

## Drawing

- **Billboards**, by default: a quad facing the camera, drawn with `:Materials/Particle.ovmat` (alpha blended, no
  depth written) and its shader, `:Shaders/Particle.ovfx`:
  - `u_Texture`, white without one, times the particle's colour and `u_Tint`;
  - `u_Disc` (on) draws a round particle whose edge is soft over `u_Softness` of its radius; off, the whole texture;
  - `u_Stretch` stretches a particle along its way by that many seconds of travel, as seen from the camera: sparks,
    rain;
  - `u_Fog` (on) fades the particles out in the scene's fog.
- **A material of your own** for sparks, fire and glows uses the additive blend mode (see
  [Rendering](Rendering.md#blend-modes)).
- **Models**: `SetModel(model)` draws each particle as the model, with a material whose shader places it. Any shader can,
  through the public include `Common/Particles.ovfxh` (see [Shaders](Shaders.md)): `ParticleAt(gl_InstanceID)` gives the
  particle, and `ParticleModelMatrix(particle)` places a model as it, turned around an axis of its own. On `Standard`,
  one extension point is enough, and the model is lit:

  ```glsl
  #shader vertex
  #version 450 core
  #define STANDARD_VERTEX
  #include ":Shaders/Standard/StandardVertex.ovfxh"
  #include ":Shaders/Common/Particles.ovfxh"

  void StandardVertex(inout vec3 worldPosition, inout vec3 worldNormal, vec2 texCoords)
  {
      const mat4 placement = ParticleModelMatrix(ParticleAt(gl_InstanceID));
      worldPosition = (placement * vec4(worldPosition, 1.0)).xyz;
      const vec3 normal = mat3(placement) * worldNormal;
      worldNormal = dot(normal, normal) > 0.0 ? normalize(normal) : worldNormal;
  }

  #shader fragment
  #version 450 core
  #include ":Shaders/Standard/StandardFragment.ovfxh"
  ```

  The engine draws a system's meshes with an identity model matrix: the vertex the shader receives is the model's own.
  A dead particle's matrix is zero, which leaves nothing to draw. `Tests/GameChecks/Assets/Shaders/ParticleMesh.ovfx`
  is this shader.
- **One draw per mesh**: a system is drawn as one instanced draw of its quad, or of each mesh of its model, with as many
  instances as there are slots from its oldest particle alive to its newest. A dead particle's instance is drawn out of
  sight.
- **Culling**: the system keeps a sphere holding every particle alive, recomputed each frame it has some, by which the
  frustum culls it.
- **Passes**: particles are drawn in the geometry pass only. They cast no shadow and are not seen by reflection probes.
  A blended material's particles are drawn with the transparent objects, sorted by their actor's distance; an opaque
  material's with the opaque ones.
- In the editor, a selected system shows no outline.

## Cost

Measured on Linux, Release, with software OpenGL (Mesa) on the CPU:

- 10000 particles alive: 0.24 ms per frame to move the system's clock, emit and keep its bounds, alone; 0.8 ms per
  frame more than an empty scene with the system out of view; 7.3 ms more in view, its 10000 quads drawn by the CPU's
  OpenGL.
- The same work from Lua, one actor per particle moved by a script with four engine calls each, cost 12 ms per frame
  for 1000 particles.
- What goes to the GPU each frame is the system's header and the records born since the last frame, not the ring.
