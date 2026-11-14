# Audio

The engine plays sounds through [SoLoud](https://solhsa.com/soloud/), on the system's audio device by miniaudio. An
`AudioSource` plays a sound, flat or in the world; an `AudioListener` hears the world's sounds. Sounds go through the
project's buses, whose volumes a settings menu sets. Without an audio device, the sounds are mixed and discarded, so
that a game, its tests and its checks behave the same on any machine.

## Sounds

- **Formats**: WAV, OGG, MP3 and FLAC. A sound is read whole into memory when it is loaded: a long piece of music costs
  its decoded length (about 10 MB a minute in stereo at 44.1 kHz).
- **A file that cannot be read** (missing, or not a sound) gives no sound, with a warning that names it:
  `Resources.GetSound(path)` returns `nil`, and a source set to it plays nothing.

## Sources

An `AudioSource` (the "Audio Source" component) plays its sound:

- `Play()` starts it from the beginning, `Pause()` and `Resume()` hold it and let it go on, `Stop()` ends it. A source
  plays one sound of its own at a time: `Play()` stops the one before.
- `SetVolume`, `SetPan` (-1 left, 1 right, for a flat sound), `SetPitch` (2 is an octave up, and twice as fast) and
  `SetLooped` take effect on the sound playing.
- `SetPitchVariation(v)` draws the pitch at random between `pitch - v` and `pitch + v` each time the source plays a
  sound, so that a step or a blow repeated does not sound the same each time. A pitch is never below 0.01.
- `SetBus(name)` sends its sounds to a bus of the project (see below); `"Master"` for none, the default. A sound playing
  moves to it, where it was.
- `IsPlaying()` is true from `Play()` to the end of a sound that does not loop, except while it is paused.
- With autoplay, a source plays when its actor is enabled; a disabled actor stops its source.

## Sounds played once

For a blow, a step or a click, a sound played once beside the others, with no source of its own:

```lua
-- Flat, on the Interface bus
Audio.PlayOneShot(Resources.GetSound("Sounds/Click.wav"), { bus = "Interface" })

-- In the world, at a place, with a pitch drawn between 0.9 and 1.1
Audio.PlayOneShot(hit, { position = point, pitchVariation = 0.1, bus = "Effects" })

-- From a source: from its place, which it follows, on its bus, with its attenuation and pitch
self.owner:GetAudioSource():PlayOneShot(step, 0.8)
```

- `Audio.PlayOneShot(sound, options)` takes `position` (in the world, flat without it), `volume`, `pan`, `pitch`,
  `pitchVariation`, `bus`, and the attenuation of a sound in the world: `attenuation` (an `AttenuationModel`),
  `minDistance`, `maxDistance`, `rolloff`. An option it does not know raises an error. It returns whether the sound
  plays.
- `AudioSource:PlayOneShot(sound[, volume])` plays it as the source would, its volume scaled by `volume`, beside the
  source's own sound: `Play()` leaves it alone, `Pause()`, `Resume()` and `Stop()` reach it, and it stops when the
  source goes.
- Each ends by itself. Many may overlap, up to the voices there are (see below).
- **One in the world too far to be heard** (under a thousandth of its volume) ends at once, and `PlayOneShot` returns
  `false`, rather than wait to be heard.

## Buses

The project names its buses in its file (`audio_buses`, see [Projects](Projects.md)): `Music`, `Effects`, `Interface`
and `Ambience` by default, 8 at most. Each plays into the **Master**, the volume of every sound.

```lua
Audio.SetVolume("Music", 0.6)   -- a bus
Audio.SetVolume("Master", 0.8)  -- every sound
local volume = Audio.GetVolume("Effects")
for _, name in ipairs(Audio.GetBuses()) do print(name) end
```

- A volume is 1 by default, and not below 0. A bus the project lacks raises an error in Lua.
- A sound on a bus at a volume of 0 is silent, and still moves on: it ends, and loops, on time.
- A source, or a sound played once, whose bus the project no longer has plays on the Master, with a warning.
- The volumes are the game's to keep: a settings menu saves them (see [Saving](Saving.md)) and sets them at start.

## In the world

A spatial source (`SetSpatial(true)`) is heard from where its actor is, from the listener's place:

- **The listener** is the last `AudioListener` created and enabled. It hears as its actor looks, along its forward with
  its up above: a sound on the left of a camera's picture is heard on the left, so a listener goes on the camera's
  actor. Without one, the listener stands at the origin and looks along +Z, as a camera with no turn.
- **The position** of a source is followed every frame, and so are the sounds it played once.
- **The volume** falls with the distance `d` between the source's minimum distance (its attenuation threshold,
  `SetAttenuationThreshold`, 1 metre by default) and its maximum distance (`SetMaxDistance`, 500 metres), as its model
  says (`SetAttenuationModel`), at its rolloff `r` (`SetRolloff`, 1 by default):

  | `AttenuationModel` | Share of the volume | With a rolloff of 1 |
  | --- | --- | --- |
  | `EXPONENTIAL_DISTANCE`, the default | `(d / min) ^ -r` | four times as far, a quarter as loud |
  | `INVERSE_DISTANCE` | `min / (min + r * (d - min))` | gentler: at `5 * min`, a fifth |
  | `LINEAR_DISTANCE` | `1 - r * (d - min) / (max - min)` | silent at the maximum distance |
  | `NONE` | 1 | no fading |

  Within the minimum distance, the sound is at its full volume; beyond the maximum, it fades no more (the linear model
  is silent there). The linear model's rolloff is at most 1. The inspector draws the model in use, and the listener's
  distance on it.
- **A source made spatial or flat while it plays** plays on from where it was, and stays paused if it was.

## Many sounds

- **Up to 64 sounds are heard at once**, the loudest. The others wait, unheard and where they are, until there is room.
  The buses take none of them.
- **A source's sound in the world too far to be heard** (under a thousandth of its volume) waits, unheard and where it
  is, until it can be heard again; one played once ends.
- `Audio.GetVoiceCount()` counts the sounds playing or paused, the sounds played once included.

## Pausing in the editor

Pausing a game in the editor pauses its sounds, those played once included; resuming lets them go on, and leaves
paused those the game paused itself. Stopping a game stops the sounds it played once, sets every volume back to 1, and
leaves the audio ready for the next one, even when stopped while paused.

## Without an audio device

On a machine without an audio device (a CI runner, a server, a container), the engine logs a warning and plays the
sounds on the **null driver**: they are mixed and discarded. They play, end and loop as with a device, so that a game's
logic and its checks see the same sounds playing, and `Audio.GetVoiceCount()` the same count.

- `Audio.GetDriver()` says which: `"device"`, `"null"`, or `"none"` if the audio could not start at all.
- `OvGame --audio null` plays on the null driver on a machine that has a device: a silent run (see
  [Command line](CommandLine.md)).
- The null driver mixes each frame's step, as the game's clock counts it without its time scale (capped by
  `max_frame_delta`, see [Projects](Projects.md)): the sounds keep step with the game, and the time a game is paused in
  the editor counts for neither.

## Not done yet

- The speed of a source and the Doppler effect, and the delay of a far sound.
- Effects on a bus (reverb, filters).
- Streaming a long sound from its file instead of reading it whole.

## C++

`OvAudio::Core::AudioEngine` takes its driver (`EDriver::AUTO`, the device or else the null driver, and
`EDriver::NULL_DRIVER`). `Update(deltaTime)` follows the sources and the listener, and on the null driver mixes the
step it is given. A test mixes the sounds itself instead: `SetManualMixing(true)`, then
`Mix(buffer, frames)` fills interleaved stereo samples at 44.1 kHz, the left channel first. `GetDriverName()`,
`IsNullDriver()` and `GetVoiceCount()` say what it plays on and how much.

The buses are `SetBuses(names)` (the contexts read them with `OvCore::Helpers::ProjectSettings::ReadAudioBuses`),
`GetBusNames()`, `SetVolume(bus, volume)`, `GetVolume(bus)` and `ResetVolumes()`; the sounds played once,
`PlayOneShot(sound, OneShotSettings)` and `StopOneShots()`; the pitch drawn, `VaryPitch(pitch, variation)`, which
`SetRandomSeed(seed)` repeats for a test. `OvAudio::Settings::AttenuationSettings` holds a source's model, distances
and rolloff, and `Evaluate(distance)` gives the share of its volume as the engine works it out.
