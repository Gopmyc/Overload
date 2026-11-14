/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <set>
#include <string_view>

#include <OvCore/Scripting/Lua/LuaComponentRef.h>

#include <OvCore/Scripting/Lua/LuaApi.h>

void BindLuaActor(sol::state& p_state);
void BindLuaComponents(sol::state& p_state);
void BindLuaGlobal(sol::state& p_state);
void BindLuaMath(sol::state& p_state);
void BindLuaProfiler(sol::state& p_state);
void BindLuaStorage(sol::state& p_state);
void BindLuaJson(sol::state& p_state);
void BindLuaNavigation(sol::state& p_state);
void BindLuaLocalization(sol::state& p_state);
void BindLuaAudio(sol::state& p_state);

namespace
{
	// Every usertype compares by identity (__eq): only the operators a type defines are part of its API
	const std::set<std::string> kOperators = {
		"__add", "__sub", "__mul", "__div", "__mod", "__pow", "__unm", "__idiv", "__concat", "__len", "__lt", "__le",
		"__call", "__tostring"
	};

	/**
	* Whether the value is the constructor sol gives a usertype bound with sol::no_constructor: a closure of its own for
	* each type, which only raises. Calling it is the one way to tell it from a real constructor, and Describe works on a
	* state of its own, which nothing else uses
	*/
	bool IsNoConstructor(const std::string& p_name, const sol::object& p_value)
	{
		if (p_name != "new" || p_value.get_type() != sol::type::function)
			return false;

		const sol::protected_function constructor = p_value.as<sol::protected_function>();
		const sol::protected_function_result result = constructor();

		if (result.valid())
			return false;

		const sol::error error = result;
		return std::string_view(error.what()).find("non-constructible") != std::string_view::npos;
	}

	void AddKeys(const sol::table& p_table, const std::string& p_prefix, std::vector<std::string>& p_names)
	{
		for (const auto& [key, value] : p_table)
		{
			if (key.get_type() == sol::type::string && !IsNoConstructor(key.as<std::string>(), value))
			{
				const auto name = key.as<std::string>();

				// The operators a script may use: the other metamethods and names are the machinery of sol's usertypes
				const bool isOperator = kOperators.contains(name);
				if (isOperator || (!name.starts_with("__") && name != "class_cast" && name != "class_check"))
				{
					p_names.push_back(p_prefix + name);
				}
			}
		}
	}
}

void OvCore::Scripting::LuaApi::OpenLibraries(sol::state& p_state)
{
	// The pure libraries only: nothing reaching the files, the system or the interpreter's internals. A coroutine runs
	// within one callback: it cannot be suspended across a call back into the engine.
	p_state.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table, sol::lib::coroutine, sol::lib::utf8);

	// Code is loaded through require and Resources.GetScript only, from the assets folders
	p_state["dofile"] = sol::nil;
	p_state["loadfile"] = sol::nil;
	p_state["load"] = sol::nil;
}

void OvCore::Scripting::LuaApi::Bind(sol::state& p_state)
{
	BindLuaActor(p_state);
	BindLuaComponents(p_state);
	BindLuaGlobal(p_state);
	BindLuaMath(p_state);
	BindLuaProfiler(p_state);
	BindLuaStorage(p_state);
	BindLuaJson(p_state);
	BindLuaNavigation(p_state);
	BindLuaLocalization(p_state);
	BindLuaAudio(p_state);
}

std::vector<std::string> OvCore::Scripting::LuaApi::Describe()
{
	sol::state state;
	OpenLibraries(state);

	std::set<std::string> standardNames;
	for (const auto& [key, value] : state.globals())
	{
		if (key.get_type() == sol::type::string)
		{
			standardNames.insert(key.as<std::string>());
		}
	}

	Bind(state);

	const sol::table registry = state.registry();
	std::vector<std::string> names;

	for (const auto& [key, value] : state.globals())
	{
		if (key.get_type() != sol::type::string)
		{
			continue;
		}

		const auto name = key.as<std::string>();

		// sol keeps a few objects of its own among the globals ("sol.<type>.♻")
		if (standardNames.contains(name) || name.starts_with("sol."))
		{
			continue;
		}

		names.push_back(name);

		if (value.get_type() != sol::type::table)
		{
			continue;
		}

		const sol::table table = value.as<sol::table>();

		if (const auto userTypeName = table.raw_get<sol::optional<std::string>>("__name"))
		{
			// A usertype: its methods, properties and static functions are in the metatable of its instances
			std::string metatableName = *userTypeName;
			if (metatableName.ends_with(".user"))
			{
				metatableName.resize(metatableName.size() - 5);
			}

			if (const auto metatable = registry.raw_get<sol::optional<sol::table>>(metatableName))
			{
				AddKeys(*metatable, name + ".", names);
			}
		}
		else
		{
			// A table of functions, or an enum, whose values its metatable holds
			AddKeys(table, name + ".", names);

			if (const auto metatable = table[sol::metatable_key].get<sol::optional<sol::table>>())
			{
				if (const auto values = metatable->raw_get<sol::optional<sol::table>>("__index"))
				{
					AddKeys(*values, name + ".", names);
				}
			}
		}
	}

	std::sort(names.begin(), names.end());
	names.erase(std::unique(names.begin(), names.end()), names.end());
	return names;
}
