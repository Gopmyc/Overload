---@meta

--- Contains all the settings for the distance fog, drawn by the shaders that include Common/Fog.ovfxh
---@class FogSettings
---@field Enabled boolean Defines if the fog is drawn
---@field Color Vector3 Colour the surfaces fade towards
---@field Intensity number Multiplier of the colour, which can go past 1 to match a bright sky
---@field Start number Distance from the eye, in meters, where the fog begins
---@field Density number How fast the fog thickens past its start, per meter
---@field MaxOpacity number Most the fog ever covers, from 0 to 1
FogSettings = {}
