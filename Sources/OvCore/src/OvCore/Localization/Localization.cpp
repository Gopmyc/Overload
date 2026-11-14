/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <format>
#include <fstream>
#include <sstream>

#include <OvCore/Localization/Localization.h>
#include <OvDebug/Logger.h>
#include <OvTools/Filesystem/IniFile.h>
#include <OvTools/Filesystem/SaveStorage.h>
#include <OvTools/Utils/Json.h>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
	using OvCore::Localization::EPluralCategory;
	using OvTools::Utils::Json::Object;
	using OvTools::Utils::Json::Value;

	constexpr double kMillion = 1000000.0;

	std::string Lower(std::string_view p_text)
	{
		std::string lower(p_text);
		std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char p_character) { return static_cast<char>(std::tolower(p_character)); });
		std::replace(lower.begin(), lower.end(), '_', '-');
		return lower;
	}

	// The language alone, without its region: "pt" for "pt-BR"
	std::string BaseLanguage(std::string_view p_code)
	{
		const std::string lower = Lower(p_code);
		return lower.substr(0, lower.find('-'));
	}

	/**
	* The operands of CLDR's plural rules: n the absolute count, i its whole part, v the count of its visible
	* decimals (up to 6)
	*/
	struct Operands
	{
		double n = 0.0;
		long long i = 0;
		int v = 0;
	};

	Operands ToOperands(double p_count)
	{
		Operands operands;
		operands.n = std::isfinite(p_count) ? std::abs(p_count) : 0.0;
		operands.i = static_cast<long long>(std::floor(std::min(operands.n, 1e15)));

		// Decimals written with up to 6 digits, trailing zeros left out: 1.50 has one
		const double fraction = operands.n - static_cast<double>(operands.i);
		if (fraction > 0.0)
		{
			long long digits = std::llround(fraction * 1e6);
			operands.v = digits == 0 ? 0 : 6;
			while (operands.v > 0 && digits % 10 == 0)
			{
				digits /= 10;
				--operands.v;
			}
		}

		return operands;
	}

	bool InRange(long long p_value, long long p_min, long long p_max)
	{
		return p_value >= p_min && p_value <= p_max;
	}

	// "many" in French, Spanish, Italian and Portuguese: a whole number of millions
	bool IsWholeMillions(const Operands& p_operands)
	{
		return p_operands.v == 0 && p_operands.i != 0 && p_operands.i % static_cast<long long>(kMillion) == 0;
	}

	// Russian, Ukrainian and Belarusian
	EPluralCategory EastSlavic(const Operands& p_operands)
	{
		if (p_operands.v != 0)
		{
			return EPluralCategory::OTHER;
		}

		const long long mod10 = p_operands.i % 10;
		const long long mod100 = p_operands.i % 100;

		if (mod10 == 1 && mod100 != 11)
		{
			return EPluralCategory::ONE;
		}

		if (InRange(mod10, 2, 4) && !InRange(mod100, 12, 14))
		{
			return EPluralCategory::FEW;
		}

		return EPluralCategory::MANY;
	}

	EPluralCategory Polish(const Operands& p_operands)
	{
		if (p_operands.v != 0)
		{
			return EPluralCategory::OTHER;
		}

		if (p_operands.i == 1)
		{
			return EPluralCategory::ONE;
		}

		const long long mod10 = p_operands.i % 10;
		const long long mod100 = p_operands.i % 100;

		if (InRange(mod10, 2, 4) && !InRange(mod100, 12, 14))
		{
			return EPluralCategory::FEW;
		}

		return EPluralCategory::MANY;
	}

	EPluralCategory Czech(const Operands& p_operands)
	{
		if (p_operands.v != 0)
		{
			return EPluralCategory::MANY;
		}

		if (p_operands.i == 1)
		{
			return EPluralCategory::ONE;
		}

		return InRange(p_operands.i, 2, 4) ? EPluralCategory::FEW : EPluralCategory::OTHER;
	}

	EPluralCategory Arabic(const Operands& p_operands)
	{
		if (p_operands.v != 0)
		{
			return EPluralCategory::OTHER;
		}

		const long long mod100 = p_operands.i % 100;

		if (p_operands.i == 0) return EPluralCategory::ZERO;
		if (p_operands.i == 1) return EPluralCategory::ONE;
		if (p_operands.i == 2) return EPluralCategory::TWO;
		if (InRange(mod100, 3, 10)) return EPluralCategory::FEW;
		if (InRange(mod100, 11, 99)) return EPluralCategory::MANY;
		return EPluralCategory::OTHER;
	}

	EPluralCategory Hebrew(const Operands& p_operands)
	{
		if ((p_operands.i == 1 && p_operands.v == 0) || (p_operands.i == 0 && p_operands.v != 0))
		{
			return EPluralCategory::ONE;
		}

		return p_operands.i == 2 && p_operands.v == 0 ? EPluralCategory::TWO : EPluralCategory::OTHER;
	}

	// English and the languages that count the same way: "one" for exactly 1
	EPluralCategory OneOrOther(const Operands& p_operands)
	{
		return p_operands.i == 1 && p_operands.v == 0 ? EPluralCategory::ONE : EPluralCategory::OTHER;
	}

	std::string Quoted(std::string_view p_name)
	{
		return std::format("[\"{}\"]", p_name);
	}

	// Reads a text or an object of plural forms into an entry, saying what is wrong
	std::optional<OvCore::Localization::Entry> ReadEntry(const Value& p_value, const std::string& p_path, std::vector<std::string>& p_warnings)
	{
		OvCore::Localization::Entry entry;

		if (const std::string* text = p_value.AsString())
		{
			entry.text = *text;
			return entry;
		}

		const Object* forms = p_value.AsObject();
		if (!forms)
		{
			p_warnings.push_back(p_path + ": must be a text, or an object of plural forms");
			return std::nullopt;
		}

		entry.plural = true;

		for (const auto& member : *forms)
		{
			const auto category = std::find(OvCore::Localization::kPluralCategoryNames.begin(), OvCore::Localization::kPluralCategoryNames.end(), member.name);
			if (category == OvCore::Localization::kPluralCategoryNames.end())
			{
				p_warnings.push_back(p_path + "." + member.name + ": unknown plural form (expected zero, one, two, few, many or other)");
				continue;
			}

			const std::string* text = member.value.AsString();
			if (!text)
			{
				p_warnings.push_back(p_path + "." + member.name + ": must be a text");
				continue;
			}

			entry.forms[static_cast<size_t>(category - OvCore::Localization::kPluralCategoryNames.begin())] = *text;
		}

		const auto& other = entry.forms[static_cast<size_t>(EPluralCategory::OTHER)];
		if (!other)
		{
			p_warnings.push_back(p_path + ".other: is missing, the form any count falls back to");
			return std::nullopt;
		}

		entry.text = *other;
		return entry;
	}

	std::optional<double> ParseCount(std::string_view p_value)
	{
		const std::string text(p_value);
		char* end = nullptr;
		const double count = std::strtod(text.c_str(), &end);
		if (end == text.c_str() || *end != '\0' || !std::isfinite(count))
		{
			return std::nullopt;
		}

		return count;
	}
}

EPluralCategory OvCore::Localization::GetPluralCategory(std::string_view p_language, double p_count)
{
	const Operands operands = ToOperands(p_count);
	const std::string language = BaseLanguage(p_language);

	// French and Brazilian Portuguese: "one" for 0 and 1, and below 2
	const bool europeanPortuguese = language == "pt" && Lower(p_language) == "pt-pt";
	if (language == "fr" || (language == "pt" && !europeanPortuguese))
	{
		if (operands.i == 0 || operands.i == 1)
		{
			return EPluralCategory::ONE;
		}

		return IsWholeMillions(operands) ? EPluralCategory::MANY : EPluralCategory::OTHER;
	}

	// Spanish, Italian, Catalan and European Portuguese: "one" for 1, "many" for whole millions
	if (language == "es" || language == "it" || language == "ca" || europeanPortuguese)
	{
		if (language == "es" ? operands.n == 1.0 : operands.i == 1 && operands.v == 0)
		{
			return EPluralCategory::ONE;
		}

		return IsWholeMillions(operands) ? EPluralCategory::MANY : EPluralCategory::OTHER;
	}

	if (language == "ru" || language == "uk" || language == "be")
	{
		return EastSlavic(operands);
	}

	if (language == "pl")
	{
		return Polish(operands);
	}

	if (language == "cs" || language == "sk")
	{
		return Czech(operands);
	}

	if (language == "ar")
	{
		return Arabic(operands);
	}

	if (language == "he")
	{
		return Hebrew(operands);
	}

	if (language == "ja" || language == "zh" || language == "ko" || language == "th" || language == "vi" || language == "id" || language == "ms")
	{
		return EPluralCategory::OTHER;
	}

	return OneOrOther(operands);
}

std::optional<OvCore::Localization::Table> OvCore::Localization::ParseTable(std::string_view p_code, std::string_view p_text, std::vector<std::string>& p_warnings)
{
	std::string error;
	const auto document = OvTools::Utils::Json::Parse(p_text, error);

	if (!document)
	{
		p_warnings.push_back(error);
		return std::nullopt;
	}

	const Object* root = document->AsObject();
	if (!root)
	{
		p_warnings.push_back("must be an object with \"name\", \"strings\" and \"fonts\"");
		return std::nullopt;
	}

	Table table;
	table.code = std::string(p_code);
	table.name = table.code;

	for (const auto& member : *root)
	{
		if (member.name == "name")
		{
			if (const std::string* name = member.value.AsString(); name && !name->empty())
			{
				table.name = *name;
			}
			else
			{
				p_warnings.push_back("name: must be the language's name, a text");
			}
		}
		else if (member.name == "strings")
		{
			const Object* strings = member.value.AsObject();
			if (!strings)
			{
				p_warnings.push_back("strings: must be an object of keys and their texts");
				continue;
			}

			for (const auto& string : *strings)
			{
				const std::string path = "strings" + Quoted(string.name);

				if (table.strings.contains(string.name))
				{
					p_warnings.push_back(path + ": is given twice, the first is kept");
					continue;
				}

				if (auto entry = ReadEntry(string.value, path, p_warnings))
				{
					table.strings.emplace(string.name, std::move(*entry));
				}
			}
		}
		else if (member.name == "fonts")
		{
			const Object* fonts = member.value.AsObject();
			if (!fonts)
			{
				p_warnings.push_back("fonts: must be an object of font paths and the fonts that stand for them");
				continue;
			}

			for (const auto& font : *fonts)
			{
				const std::string* replacement = font.value.AsString();
				if (!replacement || replacement->empty())
				{
					p_warnings.push_back("fonts" + Quoted(font.name) + ": must be a font path");
					continue;
				}

				table.fonts[font.name] = *replacement;
			}
		}
		else
		{
			p_warnings.push_back(member.name + ": unknown member (expected name, strings or fonts)");
		}
	}

	return table;
}

std::string OvCore::Localization::Format(std::string_view p_text, const Parameters& p_parameters)
{
	std::string result;
	result.reserve(p_text.size());

	size_t position = 0;
	while (position < p_text.size())
	{
		const size_t open = p_text.find('{', position);
		if (open == std::string_view::npos)
		{
			result.append(p_text.substr(position));
			break;
		}

		result.append(p_text.substr(position, open - position));

		const size_t close = p_text.find('}', open + 1);
		if (close == std::string_view::npos)
		{
			result.append(p_text.substr(open));
			break;
		}

		const std::string_view name = p_text.substr(open + 1, close - open - 1);
		const auto parameter = std::find_if(p_parameters.begin(), p_parameters.end(), [name](const auto& p_parameter) { return p_parameter.first == name; });

		if (parameter != p_parameters.end())
		{
			result.append(parameter->second);
			position = close + 1;
		}
		else
		{
			// Not a parameter: kept, and the search goes on after its brace
			result.push_back('{');
			position = open + 1;
		}
	}

	return result;
}

std::string OvCore::Localization::ToLanguageCode(std::string_view p_locale)
{
	// "fr_FR.UTF-8@euro": the language and the region, without the encoding and the modifier
	const std::string_view locale = p_locale.substr(0, p_locale.find_first_of(".@"));

	if (locale.empty() || locale == "C" || locale == "POSIX")
	{
		return "";
	}

	std::string code(locale);
	std::replace(code.begin(), code.end(), '_', '-');
	return code;
}

std::string OvCore::Localization::MatchLanguage(std::string_view p_code, const std::vector<std::string>& p_available)
{
	if (p_code.empty())
	{
		return "";
	}

	const std::string wanted = Lower(p_code);

	for (const std::string& code : p_available)
	{
		if (Lower(code) == wanted)
		{
			return code;
		}
	}

	const std::string language = BaseLanguage(p_code);

	for (const std::string& code : p_available)
	{
		if (Lower(code) == language)
		{
			return code;
		}
	}

	return "";
}

std::string OvCore::Localization::GetSystemLanguage()
{
#if defined(_WIN32)
	wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
	const int length = GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH);
	if (length <= 1)
	{
		return "";
	}

	// Locale names are ASCII ("fr-FR")
	std::string code;
	for (int index = 0; index < length - 1; ++index)
	{
		code.push_back(static_cast<char>(name[index]));
	}

	return ToLanguageCode(code);
#else
	for (const char* variable : { "LC_ALL", "LC_MESSAGES", "LANG" })
	{
		if (const char* value = std::getenv(variable); value && *value)
		{
			return ToLanguageCode(value);
		}
	}

	return "";
#endif
}

void OvCore::Localization::Localization::SetTables(std::vector<Table> p_tables, std::string p_defaultLanguage)
{
	std::sort(p_tables.begin(), p_tables.end(), [](const Table& p_lhs, const Table& p_rhs) { return p_lhs.code < p_rhs.code; });

	m_tables = std::move(p_tables);
	m_defaultLanguage = std::move(p_defaultLanguage);
	m_warnedKeys.clear();

	if (FindTable(m_defaultLanguage))
	{
		m_language = m_defaultLanguage;
	}
	else
	{
		m_language = m_tables.empty() ? "" : m_tables.front().code;
	}

	++m_revision;
}

bool OvCore::Localization::Localization::SetLanguage(std::string_view p_code)
{
	if (!FindTable(p_code))
	{
		return false;
	}

	if (m_language != p_code)
	{
		m_language = std::string(p_code);
		m_warnedKeys.clear();
		++m_revision;
	}

	return true;
}

const std::string& OvCore::Localization::Localization::GetLanguage() const
{
	return m_language;
}

const std::string& OvCore::Localization::Localization::GetDefaultLanguage() const
{
	return m_defaultLanguage;
}

std::vector<std::pair<std::string, std::string>> OvCore::Localization::Localization::GetLanguages() const
{
	std::vector<std::pair<std::string, std::string>> languages;
	languages.reserve(m_tables.size());

	for (const Table& table : m_tables)
	{
		languages.emplace_back(table.code, table.name);
	}

	return languages;
}

bool OvCore::Localization::Localization::Has(std::string_view p_key) const
{
	const std::string key(p_key);

	for (const Table* table : { FindTable(m_language), FindTable(m_defaultLanguage) })
	{
		if (table && table->strings.contains(key))
		{
			return true;
		}
	}

	return false;
}

std::string OvCore::Localization::Localization::Get(std::string_view p_key, const Parameters& p_parameters) const
{
	const std::string key(p_key);
	const Table* current = FindTable(m_language);
	const Table* fallback = FindTable(m_defaultLanguage);

	const Entry* entry = nullptr;
	const Table* source = nullptr;

	if (current)
	{
		if (const auto found = current->strings.find(key); found != current->strings.end())
		{
			entry = &found->second;
			source = current;
		}
	}

	if (!entry && fallback && fallback != current)
	{
		if (const auto found = fallback->strings.find(key); found != fallback->strings.end())
		{
			entry = &found->second;
			source = fallback;
			WarnOnce(key, std::format("Localization: \"{}\" is missing from {}, the {} text is used", key, m_language, m_defaultLanguage));
		}
	}

	if (!entry)
	{
		WarnOnce(key, std::format("Localization: \"{}\" is in no table, the key is shown", key));
		return Format(key, p_parameters);
	}

	std::string_view text = entry->text;

	if (entry->plural)
	{
		const auto count = std::find_if(p_parameters.begin(), p_parameters.end(), [](const auto& p_parameter) { return p_parameter.first == "count"; });
		const auto value = count != p_parameters.end() ? ParseCount(count->second) : std::nullopt;

		if (value)
		{
			const auto& form = entry->forms[static_cast<size_t>(GetPluralCategory(source->code, *value))];
			if (form)
			{
				text = *form;
			}
		}
	}

	return Format(text, p_parameters);
}

const std::string& OvCore::Localization::Localization::GetFont(const std::string& p_fontPath) const
{
	if (const Table* table = FindTable(m_language))
	{
		if (const auto found = table->fonts.find(p_fontPath); found != table->fonts.end())
		{
			return found->second;
		}
	}

	return p_fontPath;
}

uint64_t OvCore::Localization::Localization::GetRevision() const
{
	return m_revision;
}

const OvCore::Localization::Table* OvCore::Localization::Localization::FindTable(std::string_view p_code) const
{
	const auto found = std::find_if(m_tables.begin(), m_tables.end(), [p_code](const Table& p_table) { return p_table.code == p_code; });
	return found != m_tables.end() ? &*found : nullptr;
}

void OvCore::Localization::Localization::WarnOnce(const std::string& p_key, const std::string& p_message) const
{
	if (m_warnedKeys.insert(p_key).second)
	{
		OVLOG_WARNING(p_message);
	}
}

void OvCore::Localization::LoadProjectTranslations(const OvTools::Filesystem::IniFile& p_projectSettings, const std::filesystem::path& p_projectAssetsPath, OvTools::Filesystem::SaveStorage& p_saveStorage, Localization& p_localization)
{
	std::string folder;
	std::string defaultLanguage = "en";
	p_projectSettings.TryGet("translations", folder);
	p_projectSettings.TryGet("default_language", defaultLanguage);

	std::vector<Table> tables;

	if (!folder.empty())
	{
		const std::filesystem::path directory = p_projectAssetsPath / folder;
		std::error_code error;

		if (!std::filesystem::is_directory(directory, error))
		{
			OVLOG_WARNING("translations: \"" + folder + "\" is not a folder of the project's assets, the project has no translation");
		}
		else
		{
			std::vector<std::filesystem::path> files;
			for (const auto& file : std::filesystem::directory_iterator(directory, error))
			{
				if (file.is_regular_file() && file.path().extension() == ".json")
				{
					files.push_back(file.path());
				}
			}

			std::sort(files.begin(), files.end());

			for (const auto& file : files)
			{
				const std::string name = folder + "/" + file.filename().string();
				std::ifstream stream(file, std::ios::binary);
				std::stringstream text;
				text << stream.rdbuf();

				std::vector<std::string> warnings;
				auto table = ParseTable(file.stem().string(), text.str(), warnings);

				for (const std::string& warning : warnings)
				{
					OVLOG_WARNING("translations (" + name + "): " + warning);
				}

				if (table)
				{
					tables.push_back(std::move(*table));
				}
			}

			if (!tables.empty() && std::none_of(tables.begin(), tables.end(), [&defaultLanguage](const Table& p_table) { return p_table.code == defaultLanguage; }))
			{
				OVLOG_WARNING("default_language: \"" + defaultLanguage + "\" has no table in \"" + folder + "\"");
			}
		}
	}

	p_localization.SetTables(std::move(tables), defaultLanguage);

	const auto languages = p_localization.GetLanguages();
	if (languages.empty())
	{
		return;
	}

	std::vector<std::string> codes;
	for (const auto& [code, name] : languages)
	{
		codes.push_back(code);
	}

	// The player's choice, else the system's language
	std::string error;
	if (const auto saved = p_saveStorage.Read(kLanguageFile, error))
	{
		std::string parseError;
		const auto document = OvTools::Utils::Json::Parse(*saved, parseError);
		const Value* language = document ? document->Find("language") : nullptr;
		const std::string* code = language ? language->AsString() : nullptr;

		if (code && p_localization.SetLanguage(*code))
		{
			return;
		}

		OVLOG_WARNING(std::string(kLanguageFile) + ": no language with a table, the system's or the default one is used");
	}
	else if (error != "not found")
	{
		OVLOG_WARNING(std::string(kLanguageFile) + ": " + error);
	}

	if (const std::string system = MatchLanguage(GetSystemLanguage(), codes); !system.empty())
	{
		p_localization.SetLanguage(system);
	}
}

void OvCore::Localization::SaveLanguage(OvTools::Filesystem::SaveStorage& p_saveStorage, const Localization& p_localization)
{
	const std::string text = OvTools::Utils::Json::Write(Value(OvTools::Utils::Json::Object{ { "language", Value(p_localization.GetLanguage()) } }));
	std::string error;

	if (!p_saveStorage.Write(kLanguageFile, text, error))
	{
		OVLOG_WARNING(std::string(kLanguageFile) + ": cannot be written, " + error);
	}
}
