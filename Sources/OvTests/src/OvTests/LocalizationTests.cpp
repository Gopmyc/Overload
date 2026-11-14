/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cstdlib>
#include <optional>
#include <string>

#include <doctest/doctest.h>
#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/UI/CText.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Localization/Localization.h>
#include <OvCore/ResourceManagement/UIResourceRegistry.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvTests/TestUtils.h>
#include <OvTools/Filesystem/IniFile.h>
#include <OvTools/Filesystem/SaveStorage.h>

using OvCore::Localization::EPluralCategory;
using OvCore::Localization::GetPluralCategory;
using OvCore::Localization::Localization;
using OvCore::Localization::Table;

namespace
{
	constexpr std::string_view kEnglish = R"({
		"name": "English",
		"strings": {
			"menu.play": "Play",
			"menu.quit": "Quit",
			"greeting": "Hello, {name}!",
			"apples": { "one": "{count} apple", "other": "{count} apples" }
		}
	})";

	constexpr std::string_view kFrench = R"({
		"name": "Français",
		"strings": {
			"menu.play": "Jouer",
			"greeting": "Bonjour, {name} !",
			"apples": { "one": "{count} pomme", "other": "{count} pommes" }
		},
		"fonts": { ":Fonts/Roboto-Regular.ttf": "Fonts/Serif.ttf" }
	})";

	constexpr std::string_view kRussian = R"({
		"name": "Русский",
		"strings": {
			"apples": { "one": "{count} яблоко", "few": "{count} яблока", "many": "{count} яблок", "other": "{count} яблока" }
		}
	})";

	Table Parse(std::string_view p_code, std::string_view p_text)
	{
		std::vector<std::string> warnings;
		auto table = OvCore::Localization::ParseTable(p_code, p_text, warnings);
		REQUIRE(table.has_value());
		CHECK(warnings.empty());
		return std::move(*table);
	}

	Localization MakeLocalization()
	{
		std::vector<Table> tables;
		tables.push_back(Parse("ru", kRussian));
		tables.push_back(Parse("fr", kFrench));
		tables.push_back(Parse("en", kEnglish));

		Localization localization;
		localization.SetTables(std::move(tables), "en");
		return localization;
	}

	bool HasWarning(const std::vector<std::string>& p_warnings, std::string_view p_text)
	{
		for (const std::string& warning : p_warnings)
		{
			if (warning.find(p_text) != std::string::npos)
			{
				return true;
			}
		}

		return false;
	}

#if !defined(_WIN32)
	/**
	* An environment variable set while it lives, then put back
	*/
	class ScopedVariable final
	{
	public:
		ScopedVariable(const char* p_name, const char* p_value) : m_name(p_name)
		{
			if (const char* previous = std::getenv(p_name))
			{
				m_previous = previous;
			}

			Set(p_value);
		}

		~ScopedVariable()
		{
			Set(m_previous ? m_previous->c_str() : nullptr);
		}

	private:
		void Set(const char* p_value)
		{
			if (p_value)
			{
				setenv(m_name.c_str(), p_value, 1);
			}
			else
			{
				unsetenv(m_name.c_str());
			}
		}

		std::string m_name;
		std::optional<std::string> m_previous;
	};
#endif

	/**
	* A project with translations, and its save folder
	*/
	struct Project
	{
		OvTests::TempFolder folder;
		OvTools::Filesystem::IniFile ini;
		OvTools::Filesystem::SaveStorage storage;

		explicit Project(std::string_view p_lines) :
			ini(folder.WriteFile("Test.ovproject", p_lines).string()),
			storage(folder.GetPath() / "Saves")
		{
			folder.WriteFile("Assets/Translations/en.json", kEnglish);
			folder.WriteFile("Assets/Translations/fr.json", kFrench);
			folder.WriteFile("Assets/Translations/ru.json", kRussian);
		}

		void Load(Localization& p_localization)
		{
			OvCore::Localization::LoadProjectTranslations(ini, folder.GetPath() / "Assets", storage, p_localization);
		}
	};
}

TEST_SUITE("Localization")
{
	TEST_CASE("a table is read with its name, texts, plural forms and fonts")
	{
		const Table french = Parse("fr", kFrench);
		CHECK(french.code == "fr");
		CHECK(french.name == "Français");
		CHECK(french.strings.size() == 3);
		CHECK(french.strings.at("menu.play").text == "Jouer");
		CHECK_FALSE(french.strings.at("menu.play").plural);

		const auto& apples = french.strings.at("apples");
		CHECK(apples.plural);
		CHECK(apples.text == "{count} pommes");
		CHECK(apples.forms[static_cast<size_t>(EPluralCategory::ONE)] == "{count} pomme");
		CHECK_FALSE(apples.forms[static_cast<size_t>(EPluralCategory::FEW)].has_value());

		CHECK(french.fonts.at(":Fonts/Roboto-Regular.ttf") == "Fonts/Serif.ttf");

		// Without a name, the code stands for it
		CHECK(Parse("de", R"({ "strings": {} })").name == "de");
	}

	TEST_CASE("each mistake of a table is named with where it is, and the rest is kept")
	{
		std::vector<std::string> warnings;
		const auto table = OvCore::Localization::ParseTable("xx", R"({
			"name": 3,
			"strings": {
				"good": "Fine",
				"number": 12,
				"noOther": { "one": "One thing" },
				"badForm": { "several": "x", "other": "Things" },
				"badText": { "one": true, "other": "Things" }
			},
			"fonts": { "a.ttf": "" },
			"colour": "blue"
		})", warnings);

		REQUIRE(table.has_value());
		CHECK(table->name == "xx");
		CHECK(table->strings.contains("good"));
		CHECK_FALSE(table->strings.contains("number"));
		CHECK_FALSE(table->strings.contains("noOther"));
		CHECK(table->strings.contains("badForm"));
		CHECK(table->strings.contains("badText"));
		CHECK(table->fonts.empty());

		CHECK(warnings.size() == 7);
		CHECK(HasWarning(warnings, "name: must be the language's name"));
		CHECK(HasWarning(warnings, "strings[\"number\"]: must be a text, or an object of plural forms"));
		CHECK(HasWarning(warnings, "strings[\"noOther\"].other: is missing"));
		CHECK(HasWarning(warnings, "strings[\"badForm\"].several: unknown plural form"));
		CHECK(HasWarning(warnings, "strings[\"badText\"].one: must be a text"));
		CHECK(HasWarning(warnings, "fonts[\"a.ttf\"]: must be a font path"));
		CHECK(HasWarning(warnings, "colour: unknown member"));

		// What is not a JSON object is not a table
		warnings.clear();
		CHECK_FALSE(OvCore::Localization::ParseTable("xx", "[1, 2]", warnings).has_value());
		CHECK(warnings.size() == 1);
		warnings.clear();
		CHECK_FALSE(OvCore::Localization::ParseTable("xx", R"({ "name": )", warnings).has_value());
		CHECK(warnings.size() == 1);
	}

	TEST_CASE("a missing text comes from the default language, then is the key, each warned once")
	{
		Localization localization = MakeLocalization();
		CHECK(localization.GetLanguage() == "en");
		CHECK(localization.GetDefaultLanguage() == "en");
		CHECK(localization.SetLanguage("fr"));
		CHECK_FALSE(localization.SetLanguage("de"));
		CHECK(localization.GetLanguage() == "fr");

		OvTests::LogCapture logs;
		CHECK(localization.Get("menu.play") == "Jouer");
		CHECK(localization.Get("menu.quit") == "Quit");
		CHECK(localization.Get("menu.quit") == "Quit");
		CHECK(localization.Get("menu.nowhere") == "menu.nowhere");
		CHECK(localization.Get("menu.nowhere") == "menu.nowhere");

		CHECK(logs.Count(OvDebug::ELogLevel::LOG_WARNING) == 2);
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_WARNING, "\"menu.quit\" is missing from fr, the en text is used"));
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_WARNING, "\"menu.nowhere\" is in no table"));

		CHECK(localization.Has("menu.play"));
		CHECK(localization.Has("menu.quit"));
		CHECK_FALSE(localization.Has("menu.nowhere"));

		// The languages by code, with their names
		const auto languages = localization.GetLanguages();
		REQUIRE(languages.size() == 3);
		CHECK(languages[0].first == "en");
		CHECK(languages[1].second == "Français");
		CHECK(languages[2].first == "ru");

		// Without its default language, the first table is the current one
		std::vector<Table> tables;
		tables.push_back(Parse("fr", kFrench));
		Localization alone;
		alone.SetTables(std::move(tables), "de");
		CHECK(alone.GetLanguage() == "fr");
	}

	TEST_CASE("parameters take the place of their names, and count chooses the plural form")
	{
		using OvCore::Localization::Format;

		CHECK(Format("Hello, {name}!", { { "name", "Ada" } }) == "Hello, Ada!");
		CHECK(Format("{a}{b}{a}", { { "a", "1" }, { "b", "2" } }) == "121");
		CHECK(Format("{unknown} and {name", { { "name", "x" } }) == "{unknown} and {name");
		CHECK(Format("{{name}}", { { "name", "x" } }) == "{x}");

		Localization localization = MakeLocalization();
		CHECK(localization.Get("greeting", { { "name", "Ada" } }) == "Hello, Ada!");
		CHECK(localization.Get("apples", { { "count", "1" } }) == "1 apple");
		CHECK(localization.Get("apples", { { "count", "0" } }) == "0 apples");
		CHECK(localization.Get("apples", { { "count", "1.5" } }) == "1.5 apples");
		CHECK(localization.Get("apples") == "{count} apples");

		localization.SetLanguage("fr");
		CHECK(localization.Get("apples", { { "count", "0" } }) == "0 pomme");
		CHECK(localization.Get("apples", { { "count", "1.5" } }) == "1.5 pomme");
		CHECK(localization.Get("apples", { { "count", "2" } }) == "2 pommes");

		// A form the table does not give falls back to "other": French "many" for millions
		CHECK(localization.Get("apples", { { "count", "1000000" } }) == "1000000 pommes");

		localization.SetLanguage("ru");
		CHECK(localization.Get("apples", { { "count", "1" } }) == "1 яблоко");
		CHECK(localization.Get("apples", { { "count", "3" } }) == "3 яблока");
		CHECK(localization.Get("apples", { { "count", "5" } }) == "5 яблок");
		CHECK(localization.Get("apples", { { "count", "11" } }) == "11 яблок");
		CHECK(localization.Get("apples", { { "count", "21" } }) == "21 яблоко");
		CHECK(localization.Get("apples", { { "count", "nine" } }) == "nine яблока");
	}

	TEST_CASE("plural categories follow each language's rules")
	{
		// English, German and the languages without rules here
		CHECK(GetPluralCategory("en", 1) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("en", 0) == EPluralCategory::OTHER);
		CHECK(GetPluralCategory("en", 2) == EPluralCategory::OTHER);
		CHECK(GetPluralCategory("en", 1.5) == EPluralCategory::OTHER);
		CHECK(GetPluralCategory("en-GB", 1) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("de", 1) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("xx", 1) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("en", -1) == EPluralCategory::ONE);

		// French: 0 and 1 and what lies between are "one", whole millions "many"
		CHECK(GetPluralCategory("fr", 0) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("fr", 1) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("fr", 1.5) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("fr", 2) == EPluralCategory::OTHER);
		CHECK(GetPluralCategory("fr", 1000000) == EPluralCategory::MANY);
		CHECK(GetPluralCategory("fr", 2000000) == EPluralCategory::MANY);
		CHECK(GetPluralCategory("fr", 1000001) == EPluralCategory::OTHER);

		// Portuguese of Brazil as French, of Portugal as English; Spanish
		CHECK(GetPluralCategory("pt", 0) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("pt-BR", 0) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("pt_PT", 0) == EPluralCategory::OTHER);
		CHECK(GetPluralCategory("pt-PT", 1) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("es", 1) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("es", 0) == EPluralCategory::OTHER);
		CHECK(GetPluralCategory("es", 1000000) == EPluralCategory::MANY);

		// Russian
		CHECK(GetPluralCategory("ru", 1) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("ru", 21) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("ru", 101) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("ru", 11) == EPluralCategory::MANY);
		CHECK(GetPluralCategory("ru", 2) == EPluralCategory::FEW);
		CHECK(GetPluralCategory("ru", 24) == EPluralCategory::FEW);
		CHECK(GetPluralCategory("ru", 12) == EPluralCategory::MANY);
		CHECK(GetPluralCategory("ru", 0) == EPluralCategory::MANY);
		CHECK(GetPluralCategory("ru", 5) == EPluralCategory::MANY);
		CHECK(GetPluralCategory("ru", 111) == EPluralCategory::MANY);
		CHECK(GetPluralCategory("ru", 1.5) == EPluralCategory::OTHER);
		CHECK(GetPluralCategory("uk", 22) == EPluralCategory::FEW);

		// Polish
		CHECK(GetPluralCategory("pl", 1) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("pl", 2) == EPluralCategory::FEW);
		CHECK(GetPluralCategory("pl", 22) == EPluralCategory::FEW);
		CHECK(GetPluralCategory("pl", 12) == EPluralCategory::MANY);
		CHECK(GetPluralCategory("pl", 21) == EPluralCategory::MANY);
		CHECK(GetPluralCategory("pl", 5) == EPluralCategory::MANY);
		CHECK(GetPluralCategory("pl", 0) == EPluralCategory::MANY);
		CHECK(GetPluralCategory("pl", 0.5) == EPluralCategory::OTHER);

		// Japanese and Chinese have one form
		CHECK(GetPluralCategory("ja", 1) == EPluralCategory::OTHER);
		CHECK(GetPluralCategory("ja", 0) == EPluralCategory::OTHER);
		CHECK(GetPluralCategory("zh-Hans", 2) == EPluralCategory::OTHER);

		// Czech, Arabic, Hebrew
		CHECK(GetPluralCategory("cs", 1) == EPluralCategory::ONE);
		CHECK(GetPluralCategory("cs", 3) == EPluralCategory::FEW);
		CHECK(GetPluralCategory("cs", 5) == EPluralCategory::OTHER);
		CHECK(GetPluralCategory("cs", 1.5) == EPluralCategory::MANY);
		CHECK(GetPluralCategory("ar", 0) == EPluralCategory::ZERO);
		CHECK(GetPluralCategory("ar", 2) == EPluralCategory::TWO);
		CHECK(GetPluralCategory("ar", 103) == EPluralCategory::FEW);
		CHECK(GetPluralCategory("ar", 11) == EPluralCategory::MANY);
		CHECK(GetPluralCategory("ar", 100) == EPluralCategory::OTHER);
		CHECK(GetPluralCategory("he", 2) == EPluralCategory::TWO);
		CHECK(GetPluralCategory("he", 1) == EPluralCategory::ONE);
	}

	TEST_CASE("a language's fonts stand for the project's fonts, in that language only")
	{
		Localization localization = MakeLocalization();
		const std::string roboto = ":Fonts/Roboto-Regular.ttf";
		const std::string other = "Fonts/Other.ttf";

		CHECK(localization.GetFont(roboto) == roboto);

		const uint64_t revision = localization.GetRevision();
		localization.SetLanguage("fr");
		CHECK(localization.GetRevision() != revision);
		CHECK(localization.GetFont(roboto) == "Fonts/Serif.ttf");
		CHECK(localization.GetFont(other) == other);

		// Choosing the same language again changes nothing
		const uint64_t french = localization.GetRevision();
		localization.SetLanguage("fr");
		CHECK(localization.GetRevision() == french);
	}

	TEST_CASE("the system's locale gives a language code, matched to a table by code then by language")
	{
		using OvCore::Localization::MatchLanguage;
		using OvCore::Localization::ToLanguageCode;

		CHECK(ToLanguageCode("fr_FR.UTF-8") == "fr-FR");
		CHECK(ToLanguageCode("de_DE.UTF-8@euro") == "de-DE");
		CHECK(ToLanguageCode("pt-BR") == "pt-BR");
		CHECK(ToLanguageCode("ja") == "ja");
		CHECK(ToLanguageCode("C") == "");
		CHECK(ToLanguageCode("C.UTF-8") == "");
		CHECK(ToLanguageCode("POSIX") == "");
		CHECK(ToLanguageCode("") == "");

		const std::vector<std::string> codes = { "en", "fr", "pt-BR" };
		CHECK(MatchLanguage("fr-FR", codes) == "fr");
		CHECK(MatchLanguage("pt-br", codes) == "pt-BR");
		CHECK(MatchLanguage("pt_BR", codes) == "pt-BR");
		CHECK(MatchLanguage("pt-PT", codes) == "");
		CHECK(MatchLanguage("EN", codes) == "en");
		CHECK(MatchLanguage("de-DE", codes) == "");
		CHECK(MatchLanguage("", codes) == "");

#if !defined(_WIN32)
		ScopedVariable all("LC_ALL", nullptr);
		ScopedVariable messages("LC_MESSAGES", nullptr);
		ScopedVariable lang("LANG", "ru_RU.UTF-8");
		CHECK(OvCore::Localization::GetSystemLanguage() == "ru-RU");

		ScopedVariable allSet("LC_ALL", "fr_CA.UTF-8");
		CHECK(OvCore::Localization::GetSystemLanguage() == "fr-CA");
#endif
	}

	TEST_CASE("a project's tables are read from its folder, and the player's choice is kept")
	{
#if !defined(_WIN32)
		ScopedVariable all("LC_ALL", nullptr);
		ScopedVariable messages("LC_MESSAGES", nullptr);
		ScopedVariable lang("LANG", "ru_RU.UTF-8");
#endif

		Project project("translations=Translations\ndefault_language=en\n");

		// The system's language first
		Localization localization;
		project.Load(localization);
		CHECK(localization.GetLanguages().size() == 3);
		CHECK(localization.GetDefaultLanguage() == "en");
#if !defined(_WIN32)
		CHECK(localization.GetLanguage() == "ru");
#endif

		// Then the player's choice
		REQUIRE(localization.SetLanguage("fr"));
		OvCore::Localization::SaveLanguage(project.storage, localization);

		Localization again;
		project.Load(again);
		CHECK(again.GetLanguage() == "fr");

		// A saved language without a table is told, and the next choice is made
		std::string error;
		REQUIRE(project.storage.Write(OvCore::Localization::kLanguageFile, R"({"language": "de"})", error));
		OvTests::LogCapture logs;
		Localization third;
		project.Load(third);
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_WARNING, "language.json: no language with a table"));
#if !defined(_WIN32)
		CHECK(third.GetLanguage() == "ru");
#endif
	}

	TEST_CASE("a project without translations, or with a missing folder, has no table")
	{
		Project none("");
		Localization localization;
		none.Load(localization);
		CHECK(localization.GetLanguages().empty());
		CHECK(localization.GetLanguage() == "");

		OvTests::LogCapture logs;
		Project missing("translations=Nowhere\n");
		missing.Load(localization);
		CHECK(localization.GetLanguages().empty());
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_WARNING, "translations: \"Nowhere\" is not a folder"));

		// A table that cannot be read is named, the others kept
		Project broken("translations=Translations\ndefault_language=de\n");
		broken.folder.WriteFile("Assets/Translations/xx.json", "{ broken");
		broken.Load(localization);
		CHECK(localization.GetLanguages().size() == 3);
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_WARNING, "translations (Translations/xx.json)"));
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_WARNING, "default_language: \"de\" has no table"));
	}

	TEST_CASE("a text with a key shows its text in the current language, and its font")
	{
		static OvCore::ResourceManagement::UIResourceRegistry registry;
		static Localization localization;

		registry.ProvideDefinition({});
		OvCore::Global::ServiceLocator::Provide(registry);

		std::vector<Table> tables;
		tables.push_back(Parse("fr", kFrench));
		tables.push_back(Parse("en", kEnglish));
		localization.SetTables(std::move(tables), "en");
		OvCore::Global::ServiceLocator::Provide(localization);

		OvCore::SceneSystem::Scene scene("", "");
		auto& text = scene.CreateActor("Text").AddComponent<OvCore::ECS::Components::UI::CText>();

		text.SetTextKey("menu.play");
		CHECK(text.GetTextKey() == "menu.play");
		CHECK(text.GetText() == "Play");

		localization.SetLanguage("fr");
		CHECK(text.GetText() == "Jouer");

		// Saved and read back with its key
		tinyxml2::XMLDocument document;
		auto* node = document.NewElement("data");
		document.InsertFirstChild(node);
		text.OnSerialize(document, node);

		auto& other = scene.CreateActor("Other").AddComponent<OvCore::ECS::Components::UI::CText>();
		other.OnDeserialize(document, node);
		CHECK(other.GetTextKey() == "menu.play");
		CHECK(other.GetText() == "Jouer");

		// A text set as it is clears the key, and keeps through a language change
		text.SetText("Literal");
		CHECK(text.GetTextKey().empty());
		localization.SetLanguage("en");
		CHECK(text.GetText() == "Literal");
		CHECK(other.GetText() == "Play");
	}
}
