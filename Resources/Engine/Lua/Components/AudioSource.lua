---@meta

--- Represents an audio source. Its position in the world is important if the spatial sound settings is on
---@class AudioSource : Component
AudioSource = {}

--- Returns the actor that owns this component
---@return Actor
function AudioSource:GetOwner() end

--- Plays the attached sounds
function AudioSource:Play() end

--- Stops playback
function AudioSource:Stop() end

--- Suspend playback
function AudioSource:Pause() end

--- Resume the playback from where it get suspended
function AudioSource:Resume() end

--- Returns the sound attached to the audio source
---@return Sound|nil
function AudioSource:GetSound() end

--- Returns the current volume
---@return number
function AudioSource:GetVolume() end

--- Returns the pan
---@return number
function AudioSource:GetPan() end

--- Returns true if the AudioSource is setup to loop
---@return boolean
function AudioSource:IsLooped() end

--- Returns the pitch
---@return number
function AudioSource:GetPitch() end

--- Returns true if the audio source is currently playing
---@return boolean
function AudioSource:IsPlaying() end

--- Returns true if the audio source is spatialized
---@return boolean
function AudioSource:IsSpatial() end

--- Returns the audio source attenuation threshold (Minimum distance before applying sound attenuation)
---@return number
function AudioSource:GetAttenuationThreshold() end

--- Defines the sound to play on the audio source
---@param sound Sound
function AudioSource:SetSound(sound) end

--- Defines the audio source volume
---@param volume number
function AudioSource:SetVolume(volume) end

--- Defines the audio source pan (Left / Right)
---@param pan number
function AudioSource:SetPan(pan) end

--- Defines if the audio source should loop
---@param looped boolean
function AudioSource:SetLooped(looped) end

--- Defines the audio source pitch (= frequency or playback speed)
---@param pitch number
function AudioSource:SetPitch(pitch) end

--- Defines if the audio source should be spatialized or not
---@param spatial boolean
function AudioSource:SetSpatial(spatial) end

--- Defines the audio source attenuation threshold (Minimum distance before applying sound attenuation)
---@param threshold number
function AudioSource:SetAttenuationThreshold(threshold) end

--- Plays a sound once, beside the source's own and the others played so: from the source's place, which it follows,
--- on its bus, with its attenuation, its pitch and its pitch variation. It ends by itself; Stop and Pause reach it
---@param sound Sound
---@param volume? number A scale of the source's volume (1 by default)
function AudioSource:PlayOneShot(sound, volume) end

--- Sets the bus the source's sounds play on: "Master" (or "") for none, else one of the project's audio_buses. A sound
--- playing moves to it, where it was. Raises an error for a bus the project lacks
---@param bus string
function AudioSource:SetBus(bus) end

--- Returns the bus the source's sounds play on, "Master" for none
---@return string
function AudioSource:GetBus() end

--- Sets by how much the pitch varies at random, up or down, each time the source plays a sound (0 by default)
---@param variation number
function AudioSource:SetPitchVariation(variation) end

--- Returns by how much the pitch varies at random
---@return number
function AudioSource:GetPitchVariation() end

--- Sets how the source's sound in the world fades with the distance (exponential by default)
---@param model AttenuationModel
function AudioSource:SetAttenuationModel(model) end

--- Returns how the source's sound in the world fades with the distance
---@return AttenuationModel
function AudioSource:GetAttenuationModel() end

--- Sets how fast the source's sound in the world fades: 1 by default, more for faster (at most 1 for the linear model)
---@param rolloff number
function AudioSource:SetRolloff(rolloff) end

--- Returns how fast the source's sound in the world fades
---@return number
function AudioSource:GetRolloff() end

--- Sets the distance past which the source's sound in the world fades no more (500 by default): the linear model is
--- silent there with a rolloff of 1
---@param distance number
function AudioSource:SetMaxDistance(distance) end

--- Returns the distance past which the source's sound in the world fades no more
---@return number
function AudioSource:GetMaxDistance() end
