---@meta

--- Contains all the settings for the auto-exposure effect
---@class AutoExposureSettings : EffectSettings
---@field CenterWeightBias number
---@field MinLuminanceEV number
---@field MaxLuminanceEV number
---@field ExposureCompensationEV number
---@field Progressive boolean
---@field SpeedDown number
---@field SpeedUp number
AutoExposureSettings = {}

--- Creates settings with their default values
---@return AutoExposureSettings
function AutoExposureSettings.new() end
