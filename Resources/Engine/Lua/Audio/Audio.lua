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
