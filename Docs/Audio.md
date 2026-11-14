# Audio

The engine plays sounds through [SoLoud](https://solhsa.com/soloud/), on the system's audio device by miniaudio. An
`AudioSource` plays a sound, flat or in the world; an `AudioListener` hears the world's sounds. Without an audio device,
the sounds are mixed and discarded, so that a game, its tests and its checks behave the same on any machine.

## Sounds

- **Formats**: WAV, OGG, MP3 and FLAC. A sound is read whole into memory when it is loaded: a long piece of music costs
  its decoded length (about 10 MB a minute in stereo at 44.1 kHz).
- **A file that cannot be read** (missing, or not a sound) gives no sound, with a warning that names it:
  `Resources.GetSound(path)` returns `nil`, and a source set to it plays nothing.

## Sources

An `AudioSource` (the "Audio Source" component) plays its sound:

- `Play()` starts it from the beginning, `Pause()` and `Resume()` hold it and let it go on, `Stop()` ends it. A source
  plays one sound at a time: `Play()` stops the one before.
- `SetVolume`, `SetPan` (-1 left, 1 right, for a flat sound), `SetPitch` (2 is an octave up, and twice as fast) and
  `SetLooped` take effect on the sound playing.
- `IsPlaying()` is true from `Play()` to the end of a sound that does not loop, except while it is paused.
- With autoplay, a source plays when its actor is enabled; a disabled actor stops its source.

## In the world

A spatial source (`SetSpatial(true)`) is heard from where its actor is, from the listener's place:

- **The listener** is the last `AudioListener` created and enabled. It hears as its actor looks, along its forward with
  its up above: a sound on the left of a camera's picture is heard on the left, so a listener goes on the camera's
  actor. Without one, the listener stands at the origin and looks along +Z, as a camera with no turn.
- **The position** of a source is followed every frame.
- **The volume** falls with the distance beyond the source's attenuation threshold (`SetAttenuationThreshold`, 1 metre
  by default): inversely with it, so that four times as far is a quarter as loud. Within the threshold, the sound is at
  its full volume.
- **A source made spatial or flat while it plays** plays on from where it was, and stays paused if it was.

## Many sounds

- **Up to 64 sounds are heard at once**, the loudest. The others wait, unheard and where they are, until there is room.
- `Audio.GetVoiceCount()` counts the sounds playing or paused.

## Pausing in the editor

Pausing a game in the editor pauses its sounds; resuming lets them go on, and leaves paused those the game paused
itself. Stopping a game while it is paused leaves the audio ready for the next one.

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

- Buses, a master volume and sounds played without a source.
- The attenuation model, its rolloff and a maximum distance per source: every spatial source fades as above.
- The speed of a source and the Doppler effect, and the delay of a far sound.
- Streaming a long sound from its file instead of reading it whole.

## C++

`OvAudio::Core::AudioEngine` takes its driver (`EDriver::AUTO`, the device or else the null driver, and
`EDriver::NULL_DRIVER`). `Update(deltaTime)` follows the sources and the listener, and on the null driver mixes the
step it is given. A test mixes the sounds itself instead: `SetManualMixing(true)`, then
`Mix(buffer, frames)` fills interleaved stereo samples at 44.1 kHz, the left channel first. `GetDriverName()`,
`IsNullDriver()` and `GetVoiceCount()` say what it plays on and how much.
