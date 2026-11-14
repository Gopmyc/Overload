/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cmath>
#include <format>
#include <string>
#include <tuple>

#include <OvCore/Scripting/Lua/LuaComponentRef.h>

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Localization/Localization.h>
#include <OvTools/Filesystem/SaveStorage.h>

namespace
{
	OvCore::Localization::Localization& Texts()
	{
		return OVSERVICE(OvCore::Localization::Localization);
	}

	// A parameter's value as a text puts it: a whole number without decimals, as Lua writes the others
	std::string ToParameterText(const std::string& p_name, const sol::object& p_value)
	{
		switch (p_value.get_type())
		{
		case sol::type::string:
			return p_value.as<std::string>();

		case sol::type::number:
		{
			const double number = p_value.as<double>();
			if (std::isfinite(number) && std::floor(number) == number && std::abs(number) < 1e15)
			{
				return std::format("{}", static_cast<long long>(number));
			}

			return std::format("{:.14g}", number);
		}

		case sol::type::boolean:
			return p_value.as<bool>() ? "true" : "false";

		default:
			throw sol::error(std::format("Localization.Get: parameter '{}' must be a text, a number or a boolean", p_name));
		}
	}

	OvCore::Localization::Parameters ReadParameters(const sol::optional<sol::table>& p_parameters)
	{
		OvCore::Localization::Parameters parameters;

		if (!p_parameters)
		{
			return parameters;
		}

		for (const auto& [key, value] : *p_parameters)
		{
			if (key.get_type() != sol::type::string)
			{
				throw sol::error("Localization.Get: the parameters must be named, as in { count = 3 }");
			}

			const std::string name = key.as<std::string>();
			parameters.emplace_back(name, ToParameterText(name, value));
		}

		return parameters;
	}
}

void BindLuaLocalization(sol::state& p_luaState)
{
	p_luaState.create_named_table("Localization",
		"Get", [](const std::string& p_key, sol::optional<sol::table> p_parameters)
		{
			return Texts().Get(p_key, ReadParameters(p_parameters));
		},
		"Has", [](const std::string& p_key) { return Texts().Has(p_key); },
		"GetLanguage", []() { return Texts().GetLanguage(); },
		"GetDefaultLanguage", []() { return Texts().GetDefaultLanguage(); },
		"SetLanguage", [](const std::string& p_code) -> std::tuple<bool, sol::optional<std::string>>
		{
			if (!Texts().SetLanguage(p_code))
			{
				return { false, std::format("no language '{}': the project has no table for it", p_code) };
			}

			OvCore::Localization::SaveLanguage(OVSERVICE(OvTools::Filesystem::SaveStorage), Texts());
			return { true, sol::nullopt };
		},
		"GetLanguages", [](sol::this_state p_state)
		{
			sol::state_view lua(p_state);
			sol::table languages = lua.create_table();

			for (const auto& [code, name] : Texts().GetLanguages())
			{
				languages.add(lua.create_table_with("code", code, "name", name));
			}

			return languages;
		},
		"GetFont", [](const std::string& p_fontPath) { return Texts().GetFont(p_fontPath); },
		"GetRevision", []() { return Texts().GetRevision(); }
	);
}
