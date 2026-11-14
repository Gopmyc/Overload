---@meta

--- The project's texts in every language it has (see the engine's Docs/Localization.md): one table per language,
--- "<code>.json" in the folder of assets the project's `translations` key names
---@class Localization
Localization = {}

--- Returns a text in the current language, its "{name}" replaced by the parameters, and its plural form chosen by
--- `count` after the language's rules. A text missing from the current language comes from the project's default
--- one, and one missing from both is the key itself; each is logged once as a warning. A parameter is a text, a
--- number or a boolean, else an error is raised
---@param key string
---@param params? table<string, string|number|boolean>
---@return string
function Localization.Get(key, params) end

--- Returns true if the current language or the default one has the text
---@param key string
---@return boolean
function Localization.Has(key) end

--- Returns the current language's code ("en", "fr", "pt-BR"), or "" when the project has no table
---@return string
function Localization.GetLanguage() end

--- Returns the project's default language, the one a missing text is taken from
---@return string
function Localization.GetDefaultLanguage() end

--- Gives the texts in another language, and saves the choice in the project's save folder (language.json), read at
--- the next start. Returns true, or false and why not
---@param code string
---@return boolean set
---@return string|nil error
function Localization.SetLanguage(code) end

--- Returns every language the project has a table for, by code, with its name in itself
---@return { code: string, name: string }[]
function Localization.GetLanguages() end

--- Returns the font that stands for a font in the current language (a table's "fonts"), or the same font
---@param fontPath string
---@return string
function Localization.GetFont(fontPath) end

--- Returns a number that grows each time the texts change: what shows a text gets it again when it differs
---@return integer
function Localization.GetRevision() end
