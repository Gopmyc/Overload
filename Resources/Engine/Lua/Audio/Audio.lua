---@meta

--- The audio engine (see the engine's Docs/Audio.md)
---@class Audio
Audio = {}

--- Returns what the sounds are played on: "device" (the system's audio device), "null" (no device: the sounds are
--- mixed and discarded, so that they play, end and loop as with one), or "none" when the audio could not start
---@return string
function Audio.GetDriver() end

--- Returns the number of sounds playing or paused
---@return integer
function Audio.GetVoiceCount() end

--- Returns the names of the project's buses (its audio_buses), the Master left out
---@return string[]
function Audio.GetBuses() end

--- Sets the volume of a bus, or of every sound for "Master" (1 by default). Raises an error for a bus the project
--- lacks, or a volume below 0
---@param bus string
---@param volume number
function Audio.SetVolume(bus, volume) end

--- Returns the volume of a bus, or of every sound for "Master"
---@param bus string
---@return number
function Audio.GetVolume(bus) end

--- How Audio.PlayOneShot plays a sound
---@class AudioOneShotOptions
---@field position? Vector3 In the world at this position, flat without it
---@field volume? number 1 by default
---@field pan? number -1 left to 1 right, for a flat sound
---@field pitch? number 1 by default
---@field pitchVariation? number The pitch is drawn between pitch - pitchVariation and pitch + pitchVariation
---@field bus? string A bus of the project, or "Master" (the default)
---@field attenuation? AttenuationModel Exponential by default
---@field minDistance? number 1 by default
---@field maxDistance? number 500 by default
---@field rolloff? number 1 by default

--- Plays a sound once, beside the others, with no source: it ends by itself. A sound in the world too far to be heard
--- ends at once. Returns true while it plays. Raises an error for an unknown option, a bus the project lacks, or no
--- sound
---@param sound Sound
---@param options? AudioOneShotOptions
---@return boolean
function Audio.PlayOneShot(sound, options) end
