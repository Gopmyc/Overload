--- Default colors used by the built-in Paint implementations. Values can be replaced at runtime,
--- panels pick them up on their next Paint.
---@class UISkin
local Skin = {
	PanelBackground = Vector4.new(0.15, 0.15, 0.17, 0.95),

	Text = Vector4.new(0.92, 0.92, 0.92, 1.0),
	TextDisabled = Vector4.new(0.5, 0.5, 0.5, 1.0),
	TextPlaceholder = Vector4.new(0.55, 0.55, 0.58, 1.0),

	ButtonNormal = Vector4.new(0.26, 0.27, 0.31, 1.0),
	ButtonHovered = Vector4.new(0.33, 0.35, 0.41, 1.0),
	ButtonDown = Vector4.new(0.2, 0.36, 0.62, 1.0),
	ButtonDisabled = Vector4.new(0.2, 0.2, 0.22, 1.0),

	TextEntryBackground = Vector4.new(0.1, 0.1, 0.12, 1.0),
	TextEntryBackgroundFocused = Vector4.new(0.08, 0.1, 0.16, 1.0),

	ScrollBarBackground = Vector4.new(0.11, 0.11, 0.13, 1.0)
}

return Skin
