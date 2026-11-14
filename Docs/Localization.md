# Localization

A project gives its texts in several languages through tables, one per language: a script or a `Text` asks for a
text by its key, and gets it in the language the player chose. The engine reads the tables, chooses the plural forms
and the fonts, and keeps the player's choice. It does not shape scripts that join their letters (Arabic, the Indic
scripts) nor lay out right-to-left text.

## The tables

The project file names a folder of its assets and a default language (see [Projects](Projects.md)):

```ini
translations=Translations
default_language=en
```

Each `<code>.json` of the folder is a language, its code the file's name: `en.json`, `fr.json`, `pt-BR.json`.

```json
{
	"name": "Français",
	"strings": {
		"menu.play": "Jouer",
		"greeting": "Bonjour, {name} !",
		"inventory.apples": { "one": "{count} pomme", "many": "{count} de pommes", "other": "{count} pommes" }
	},
	"fonts": {
		":Fonts/Roboto-Regular.ttf": "Fonts/MyFont-Regular.ttf"
	}
}
```

- `name` is the language's name in itself, for a menu of languages (the code without it).
- `strings` holds the texts by key. A key is any text; dotted names keep them in order.
- A text is a string, or an object of **plural forms**: `zero`, `one`, `two`, `few`, `many` and `other`, of which
  `other` is required. Which form a count takes follows each language's rules (CLDR's), for whole and decimal counts:

  | Languages | Forms |
  | --- | --- |
  | English, German, Dutch, the Scandinavian languages and any language not listed | `one` for 1, `other` |
  | French, Portuguese of Brazil (`pt`, `pt-BR`) | `one` for 0 and 1 and what lies between, `many` for whole millions, `other` |
  | Spanish, Italian, Catalan, Portuguese of Portugal (`pt-PT`) | `one` for 1, `many` for whole millions, `other` |
  | Russian, Ukrainian, Belarusian | `one` (1, 21, 101), `few` (2 to 4, 22), `many` (0, 5 to 20, 25), `other` for decimals |
  | Polish | `one` (1), `few` (2 to 4, 22), `many` (0, 5 to 21, 25), `other` for decimals |
  | Czech, Slovak | `one` (1), `few` (2 to 4), `many` for decimals, `other` |
  | Arabic | `zero`, `one`, `two`, `few` (3 to 10), `many` (11 to 99), `other` |
  | Hebrew | `one`, `two`, `other` |
  | Japanese, Chinese, Korean, Thai, Vietnamese, Indonesian, Malay | `other` only |

  A form a table leaves out falls back to `other`.
- `fonts` (optional) gives fonts that stand for others in this language: a script in Japanese that Roboto lacks gets a
  font that has it, and every `Text` set to Roboto draws with that font while the language is chosen.
- **A mistake in a file** is logged as a warning with where it is (`translations (Translations/fr.json):
  strings["menu.play"]: must be a text, or an object of plural forms`), and the rest of the file is kept. A file that
  is not JSON is left out.

## Getting a text

```lua
local title = Localization.Get("menu.play")                   -- "Jouer"
local hello = Localization.Get("greeting", { name = "Ada" })  -- "Bonjour, Ada !"
local apples = Localization.Get("inventory.apples", { count = 3 })
```

- Each `{name}` of the text takes its parameter. A parameter is a string, a number or a boolean; a whole number is
  written without decimals, the others as Lua writes them. Numbers are not written after the language's conventions
  (`1 000`, `1,5`): a script that wants them passes a string.
- `count` also chooses the plural form.
- **A missing text** comes from the default language; one missing from both is the key itself. Each is logged once
  as a warning.
- `Localization.Has(key)` tells whether the current language or the default one has it.

## The language

- **At start**: the player's choice (`language.json` in the save folder, see [Saving](Saving.md)), else the system's
  language when the project has a table for it (`LC_ALL`, `LC_MESSAGES` then `LANG` on Linux, the user's locale on
  Windows; `fr-FR` takes `fr.json` when there is no `fr-FR.json`), else the default language.
- `Localization.SetLanguage(code)` gives the texts in another language and saves the choice. It returns `false` and why
  for a language without a table.
- `Localization.GetLanguages()` lists `{ code, name }` by code, for a menu; `GetLanguage()` and `GetDefaultLanguage()`
  give the codes.
- `Localization.GetRevision()` grows each time the texts change. What shows a text gets it again when the revision
  differs from the one it had.
- In the editor, the tables and the player's language are read again each time a game starts, as the input actions are.

## Texts that follow the language

- A `Text` component with a key (`Text:SetTextKey(key)`, "Text Key" in the inspector, `text_key` in the scene file)
  shows the key's text, and gets it again when the language changes. `SetText` clears the key: the text is then as
  given.
- Its font is the one the current language stands for it, if any.
- In the UI library, `DLabel:SetTextKey(key, params)` does the same with parameters, and `DButton` follows it. A panel
  that overrides `Think` calls `DLabel.Think(self, deltaTime)` to keep it.

## C++

`OvCore::Localization` reads a table (`ParseTable`), chooses a plural form (`GetPluralCategory`), puts the parameters in
(`Format`), and holds the tables (`Localization`, a service the game and the editor provide). `LoadProjectTranslations`
and `SaveLanguage` read the project and keep the player's choice.
