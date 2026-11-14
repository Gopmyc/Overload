---@meta

---@enum TextHorizontalAlignment
TextHorizontalAlignment = {
	LEFT = 0,
	CENTER = 1,
	RIGHT = 2
}

---@enum TextVerticalAlignment
TextVerticalAlignment = {
	TOP = 0,
	CENTER = 1,
	BOTTOM = 2
}

--- Represents a renderable user interface text
---@class Text : Component
Text = {}

--- Returns the actor that owns this component
---@return Actor
function Text:GetOwner() end

--- Returns the text content
---@return string
function Text:GetText() end

--- Defines the text content, in UTF-8. A character the font has no glyph for is drawn as "?"
---@param text string
function Text:SetText(text) end

--- Returns the size the text's lines take, in canvas pixels, wrapped at the element's width when it has one
---@return Vector2
function Text:GetContentSize() end

--- Returns the font resource path
---@return string
function Text:GetFontPath() end

--- Defines the font resource path
---@param fontPath string
function Text:SetFontPath(fontPath) end

--- Returns the font size in canvas pixels
---@return number
function Text:GetFontSize() end

--- Defines the font size in canvas pixels
---@param fontSize number
function Text:SetFontSize(fontSize) end

--- Returns the text color
---@return Vector4
function Text:GetColor() end

--- Defines the text color
---@param color Vector4
function Text:SetColor(color) end

--- Returns the horizontal text alignment
---@return TextHorizontalAlignment
function Text:GetHorizontalAlignment() end

--- Defines the horizontal text alignment
---@param alignment TextHorizontalAlignment
function Text:SetHorizontalAlignment(alignment) end

--- Returns the vertical text alignment
---@return TextVerticalAlignment
function Text:GetVerticalAlignment() end

--- Defines the vertical text alignment
---@param alignment TextVerticalAlignment
function Text:SetVerticalAlignment(alignment) end

--- Returns whether the text is lit as the scene is under a world space canvas
---@return boolean
function Text:IsLit() end

--- Defines whether the text is lit as the scene is (its lights, the shadows it receives, its fog), drawn among its
--- transparent surfaces. Only under a world space canvas: a screen space text stays drawn over the scene, unlit
---@param lit boolean
function Text:SetLit(lit) end
