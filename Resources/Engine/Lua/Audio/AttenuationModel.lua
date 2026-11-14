---@meta

--- How a sound in the world fades with its distance d to the listener, between its minimum distance (an audio source's
--- attenuation threshold) and its maximum distance, at a rolloff r (see the engine's Docs/Audio.md)
---@enum AttenuationModel
AttenuationModel = {
    --- It does not fade
    NONE = 0,
    --- min / (min + r * (d - min))
    INVERSE_DISTANCE = 1,
    --- 1 - r * (d - min) / (max - min): silent at the maximum distance with a rolloff of 1
    LINEAR_DISTANCE = 2,
    --- (d / min) ^ -r: a quarter as loud four times as far with a rolloff of 1
    EXPONENTIAL_DISTANCE = 3,
}
