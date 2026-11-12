/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <string>
#include <tuple>

#include <OvTools/Filesystem/SaveStorage.h>

#include "OvCore/Global/ServiceLocator.h"

#include <OvCore/Scripting/Lua/LuaComponentRef.h>

namespace
{
	using Results = std::tuple<sol::object, sol::object>;

	OvTools::Filesystem::SaveStorage& Storage()
	{
		return OVSERVICE(OvTools::Filesystem::SaveStorage);
	}

	Results Success(sol::state_view p_state, sol::object p_value)
	{
		return { std::move(p_value), sol::make_object(p_state, sol::lua_nil) };
	}

	Results Failure(sol::state_view p_state, const std::string& p_error)
	{
		return { sol::make_object(p_state, sol::lua_nil), sol::make_object(p_state, p_error) };
	}

	// Strings only: a number would be turned into one by Lua, and anything else has no path to give
	bool IsString(const sol::object& p_value)
	{
		return p_value.get_type() == sol::type::string;
	}

	Results Write(sol::this_state p_state, sol::object p_path, sol::object p_data)
	{
		if (!IsString(p_path)) return Failure(p_state, "invalid path");
		if (!IsString(p_data)) return Failure(p_state, "invalid data");

		std::string error;

		if (!Storage().Write(p_path.as<std::string>(), p_data.as<std::string>(), error))
		{
			return Failure(p_state, error);
		}

		return Success(p_state, sol::make_object(p_state, true));
	}

	Results Read(sol::this_state p_state, sol::object p_path)
	{
		if (!IsString(p_path)) return Failure(p_state, "invalid path");

		std::string error;
		auto data = Storage().Read(p_path.as<std::string>(), error);

		if (!data)
		{
			return Failure(p_state, error);
		}

		return Success(p_state, sol::make_object(p_state, std::move(*data)));
	}

	bool Exists(sol::object p_path)
	{
		return IsString(p_path) && Storage().Exists(p_path.as<std::string>());
	}

	Results Delete(sol::this_state p_state, sol::object p_path)
	{
		if (!IsString(p_path)) return Failure(p_state, "invalid path");

		std::string error;

		if (!Storage().Delete(p_path.as<std::string>(), error))
		{
			return Failure(p_state, error);
		}

		return Success(p_state, sol::make_object(p_state, true));
	}

	Results List(sol::this_state p_state, sol::object p_path)
	{
		// No folder named lists the storage itself
		const bool root = p_path.get_type() == sol::type::lua_nil || p_path.get_type() == sol::type::none;

		if (!root && !IsString(p_path)) return Failure(p_state, "invalid path");

		std::string error;
		auto names = Storage().List(root ? std::string{} : p_path.as<std::string>(), error);

		if (!names)
		{
			return Failure(p_state, error);
		}

		sol::state_view state(p_state);
		sol::table list = state.create_table(static_cast<int>(names->size()), 0);

		for (size_t i = 0; i < names->size(); ++i)
		{
			list[i + 1] = (*names)[i];
		}

		return Success(p_state, list);
	}
}

// Named functions rather than lambdas: lambdas of one signature are told apart by their type's name, which some
// compilers give alike
void BindLuaStorage(sol::state& p_luaState)
{
	p_luaState.create_named_table("Storage",
		"Write", &Write,
		"Read", &Read,
		"Exists", &Exists,
		"Delete", &Delete,
		"List", &List
	);
}
