/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace OvTools::Filesystem
{
	class IniFile;
	class SaveStorage;
}

namespace OvCore::Localization
{
	// Where the player's language is kept, in the project's save folder
	constexpr std::string_view kLanguageFile = "language.json";

	/**
	* The plural categories of CLDR: which form of a text a count takes in a language
	*/
	enum class EPluralCategory
	{
		ZERO,
		ONE,
		TWO,
		FEW,
		MANY,
		OTHER
	};

	constexpr std::array<std::string_view, 6> kPluralCategoryNames = { "zero", "one", "two", "few", "many", "other" };

	/**
	* Returns the category a count takes in a language, after CLDR's cardinal rules for whole and decimal counts. The
	* language is a code ("ru", "pt-BR"): its first part chooses the rules. A language without rules here takes
	* English's: "one" for 1, "other" for any other count
	* @param p_language
	* @param p_count
	*/
	EPluralCategory GetPluralCategory(std::string_view p_language, double p_count);

	/**
	* A text of a table: one text, or forms by plural category
	*/
	struct Entry
	{
		std::string text; // The text, or the "other" form of a plural entry
		std::array<std::optional<std::string>, 6> forms; // By EPluralCategory, for a plural entry
		bool plural = false;
	};

	/**
	* The texts of a language, read from its file
	*/
	struct Table
	{
		std::string code; // The file's name without its extension: "en", "fr", "pt-BR"
		std::string name; // The language's name in itself: "Français"
		std::unordered_map<std::string, Entry> strings;
		std::unordered_map<std::string, std::string> fonts; // A font path, and the font that stands for it in this language
	};

	/**
	* Reads a language file. A mistake is added to the warnings with where it is in the file ("strings[\"menu.play\"]:
	* must be a text, or an object of plural forms"), and the rest of the file is kept. Returns nullopt when the file
	* is not a JSON object
	* @param p_code
	* @param p_text
	* @param p_warnings
	*/
	std::optional<Table> ParseTable(std::string_view p_code, std::string_view p_text, std::vector<std::string>& p_warnings);

	/**
	* Named values a text puts in place of its "{name}". "count" also chooses the plural form
	*/
	using Parameters = std::vector<std::pair<std::string, std::string>>;

	/**
	* Puts each parameter in place of its "{name}" in a text. A "{name}" without a parameter is left as it is
	* @param p_text
	* @param p_parameters
	*/
	std::string Format(std::string_view p_text, const Parameters& p_parameters);

	/**
	* Returns the language code of a system locale: "fr_FR.UTF-8" gives "fr-FR", "pt-BR" stays, and "C", "POSIX" or an
	* empty locale give ""
	* @param p_locale
	*/
	std::string ToLanguageCode(std::string_view p_locale);

	/**
	* Returns the code among those available that a language code asks for: the same code ("pt-BR"), else its
	* language alone ("pt"), letter case and "_" aside. Returns "" when there is none
	* @param p_code
	* @param p_available
	*/
	std::string MatchLanguage(std::string_view p_code, const std::vector<std::string>& p_available);

	/**
	* Returns the system's language as a code ("fr-FR"), or "" when it cannot tell: LC_ALL, LC_MESSAGES then LANG on
	* Linux, the user's locale on Windows
	*/
	std::string GetSystemLanguage();

	/**
	* The project's texts in every language it has, and the language they are given in
	*/
	class Localization
	{
	public:
		/**
		* Takes the project's tables and its default language, the one a text missing from the current language is
		* taken from. The current language becomes the default one, or the first table without it
		* @param p_tables
		* @param p_defaultLanguage
		*/
		void SetTables(std::vector<Table> p_tables, std::string p_defaultLanguage);

		/**
		* Gives the texts in another language, and returns false for a language without a table
		* @param p_code
		*/
		bool SetLanguage(std::string_view p_code);

		/**
		* Returns the current language's code, "" without a table
		*/
		const std::string& GetLanguage() const;

		/**
		* Returns the project's default language
		*/
		const std::string& GetDefaultLanguage() const;

		/**
		* Returns the code and the name of every language with a table, by code
		*/
		std::vector<std::pair<std::string, std::string>> GetLanguages() const;

		/**
		* Returns true if the current language or the default one has the text
		* @param p_key
		*/
		bool Has(std::string_view p_key) const;

		/**
		* Returns a text in the current language, with its parameters in place and its plural form chosen by "count".
		* A text missing from the current language comes from the default one, and one missing from both is the key
		* itself; each is logged once as a warning
		* @param p_key
		* @param p_parameters
		*/
		std::string Get(std::string_view p_key, const Parameters& p_parameters = {}) const;

		/**
		* Returns the font that stands for a font in the current language, or the same font
		* @param p_fontPath
		*/
		const std::string& GetFont(const std::string& p_fontPath) const;

		/**
		* Returns a number that grows each time the texts change: a language or tables set. What shows a text gets it
		* again when it differs from the one it had
		*/
		uint64_t GetRevision() const;

	private:
		const Table* FindTable(std::string_view p_code) const;
		void WarnOnce(const std::string& p_key, const std::string& p_message) const;

	private:
		std::vector<Table> m_tables;
		std::string m_defaultLanguage;
		std::string m_language;
		uint64_t m_revision = 1;
		mutable std::unordered_set<std::string> m_warnedKeys;
	};

	/**
	* Reads the project's tables, one "<code>.json" in the folder of assets its "translations" key names, and its
	* "default_language" ("en" without the key). Then gives the player's language (language.json in the save folder),
	* else the system's when it has a table, else the default one. What cannot be read is logged as a warning
	* @param p_projectSettings
	* @param p_projectAssetsPath
	* @param p_saveStorage
	* @param p_localization
	*/
	void LoadProjectTranslations(const OvTools::Filesystem::IniFile& p_projectSettings, const std::filesystem::path& p_projectAssetsPath, OvTools::Filesystem::SaveStorage& p_saveStorage, Localization& p_localization);

	/**
	* Writes the player's language to the save folder, and logs a warning when it cannot
	* @param p_saveStorage
	* @param p_localization
	*/
	void SaveLanguage(OvTools::Filesystem::SaveStorage& p_saveStorage, const Localization& p_localization);
}
