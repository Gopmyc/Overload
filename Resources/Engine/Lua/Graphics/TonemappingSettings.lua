---@meta

--- Contains all the settings for the tone mapping effect, with the colour grading and the vignette of the same pass
---@class TonemappingSettings : EffectSettings
---@field Exposure number
---@field Mode TonemappingMode
---@field GammaCorrection boolean
---@field Temperature number White balance before the curve, -1 (cool) to 1 (warm)
---@field Tint number White balance before the curve, -1 (green) to 1 (magenta)
---@field Saturation number After the curve, 0 is greyscale and 1 leaves it as is
---@field Contrast number After the curve, around middle grey; 1 leaves it as is
---@field VignetteIntensity number How much the corners darken, 0 to 1
---@field VignetteSmoothness number How far towards the centre the darkening reaches, 0 to 1
TonemappingSettings = {}
