/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cstring>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>

#include <OvCore/Scripting/Lua/LuaComponentRef.h>

namespace
{
	template<typename T>
	void MarkMetatable(lua_State* p_luaState, const std::string& p_name)
	{
		luaL_getmetatable(p_luaState, p_name.c_str());

		if (lua_istable(p_luaState, -1))
		{
			lua_pushboolean(p_luaState, 1);
			lua_rawsetp(p_luaState, -2, &OvCore::Scripting::kLuaMathMark<T>);
		}

		lua_pop(p_luaState, 1);
	}

	template<typename T>
	void MarkMath(lua_State* p_luaState)
	{
		MarkMetatable<T>(p_luaState, sol::usertype_traits<T>::metatable());
		MarkMetatable<T>(p_luaState, sol::usertype_traits<const T>::metatable());
		MarkMetatable<T>(p_luaState, sol::usertype_traits<T*>::metatable());
		MarkMetatable<T>(p_luaState, sol::usertype_traits<const T*>::metatable());
		MarkMetatable<T>(p_luaState, sol::usertype_traits<sol::d::u<T>>::metatable());
	}

	const char* GetMathName(lua_State* p_luaState, int p_index)
	{
		using namespace OvCore::Scripting;
		using namespace OvMaths;

		if (HoldsLuaMath<FVector2>(p_luaState, p_index)) return LuaMathName<FVector2>;
		if (HoldsLuaMath<FVector3>(p_luaState, p_index)) return LuaMathName<FVector3>;
		if (HoldsLuaMath<FVector4>(p_luaState, p_index)) return LuaMathName<FVector4>;
		if (HoldsLuaMath<FQuaternion>(p_luaState, p_index)) return LuaMathName<FQuaternion>;
		if (HoldsLuaMath<FMatrix3>(p_luaState, p_index)) return LuaMathName<FMatrix3>;
		if (HoldsLuaMath<FMatrix4>(p_luaState, p_index)) return LuaMathName<FMatrix4>;
		return nullptr;
	}

	/**
	* The class a sol metatable is named after, without its namespaces: "sol.sol::d::u<OvCore::ECS::Components::
	* CTransform>" (a component's handle) is "CTransform". Any other name is kept
	*/
	std::string ShortenSolName(std::string_view p_name)
	{
		if (!p_name.starts_with("sol."))
		{
			return std::string(p_name);
		}

		p_name.remove_prefix(4);

		if (p_name.starts_with("sol::d::u<") && p_name.ends_with(">"))
		{
			p_name = p_name.substr(10, p_name.size() - 11);
		}

		while (p_name.ends_with("*") || p_name.ends_with(" "))
		{
			p_name.remove_suffix(1);
		}

		if (p_name.starts_with("const "))
		{
			p_name.remove_prefix(6);
		}

		if (const size_t scope = p_name.rfind("::"); scope != std::string_view::npos)
		{
			p_name.remove_prefix(scope + 2);
		}

		return p_name.empty() ? std::string("userdata") : std::string(p_name);
	}

	/**
	* The name of what a stack index holds: a maths type by its Lua name, another userdata by the class or the __name of
	* its metatable if it has one, anything else by its Lua type, as Lua's own argument errors name it
	*/
	std::string DescribeValue(lua_State* p_luaState, int p_index)
	{
		switch (lua_type(p_luaState, p_index))
		{
		case LUA_TUSERDATA:
			if (const char* name = GetMathName(p_luaState, p_index))
			{
				return name;
			}

			if (luaL_getmetafield(p_luaState, p_index, "__name") != LUA_TNIL)
			{
				const char* name = lua_type(p_luaState, -1) == LUA_TSTRING ? lua_tostring(p_luaState, -1) : nullptr;
				std::string described = name ? ShortenSolName(name) : "userdata";
				lua_pop(p_luaState, 1);
				return described;
			}

			return "userdata";

		case LUA_TLIGHTUSERDATA:
			return "light userdata";

		default:
			return luaL_typename(p_luaState, p_index);
		}
	}
}

void OvCore::Scripting::MarkMathMetatables(lua_State* p_luaState)
{
	MarkMath<OvMaths::FVector2>(p_luaState);
	MarkMath<OvMaths::FVector3>(p_luaState);
	MarkMath<OvMaths::FVector4>(p_luaState);
	MarkMath<OvMaths::FQuaternion>(p_luaState);
	MarkMath<OvMaths::FMatrix3>(p_luaState);
	MarkMath<OvMaths::FMatrix4>(p_luaState);
}

void OvCore::Scripting::ThrowBadMathArgument(lua_State* p_luaState, int p_index, const char* p_expected)
{
	const std::string problem = std::format("{} expected, got {}", p_expected, DescribeValue(p_luaState, p_index));

	// As luaL_argerror words it, without raising from inside the C++ call: sol turns the exception into the Lua error
	lua_Debug frame{};
	if (!lua_getstack(p_luaState, 0, &frame))
	{
		throw std::runtime_error(std::format("bad argument #{} ({})", p_index, problem));
	}

	lua_getinfo(p_luaState, "n", &frame);
	const char* name = frame.name ? frame.name : "?";
	int argument = p_index;

	// A method call counts its arguments after self
	if (frame.namewhat && std::strcmp(frame.namewhat, "method") == 0)
	{
		--argument;

		if (argument == 0)
		{
			throw std::runtime_error(std::format("calling '{}' on bad self ({})", name, problem));
		}
	}

	throw std::runtime_error(std::format("bad argument #{} to '{}' ({})", argument, name, problem));
}
