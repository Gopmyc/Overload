--- Named fonts, modeled after surface.CreateFont. A font pairs a font asset path with a size.
---@class UIFonts
local Fonts = {}

---@class UIFontData
---@field path string Font asset path
---@field size number Font size in canvas units

local fonts = {}

--- Registers (or replaces) a named font
---@param name string
---@param data { path: string|nil, size: number|nil }
function Fonts.Create(name, data)
	assert(type(name) == "string", "UI: a font name must be a string")
	assert(type(data) == "table", "UI: font '" .. name .. "' data must be a table")

	local default = fonts.Default
	local size = data.size or (default and default.size)

	assert(type(size) == "number" and size > 0, "UI: font '" .. name .. "' size must be a positive number")

	fonts[name] = {
		path = data.path or (default and default.path),
		size = size
	}
end

--- Returns the named font, or the default font when the name is unknown
---@param name string|nil
---@return UIFontData
function Fonts.Get(name)
	return fonts[name] or fonts.Default
end

---@param name string
---@return boolean
function Fonts.Exists(name)
	return fonts[name] ~= nil
end

Fonts.Create("Default", { path = ":Fonts/Roboto-Regular.ttf", size = 18 })

return Fonts
