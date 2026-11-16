/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <type_traits>

#include <sol/sol.hpp>

#include <OvMaths/FMatrix3.h>
#include <OvMaths/FMatrix4.h>
#include <OvMaths/FQuaternion.h>
#include <OvMaths/FVector2.h>
#include <OvMaths/FVector3.h>
#include <OvMaths/FVector4.h>

// Included by LuaComponentRef.h, through which every source that uses sol includes sol: the customizations below
// change how sol reads the maths usertypes, and must be seen the same way everywhere.
//
// Without its safety checks (Release), sol reads a maths argument without checking it: nil, a table or any other
// value where a Vector3 is taken was read as a pointer to one, and crashed the process. These checks raise a Lua
// error instead, worded as Lua words a bad argument.

namespace OvCore::Scripting
{
	/**
	* The maths types bound as Lua usertypes
	*/
	template<typename T>
	constexpr bool IsLuaMath =
		std::is_same_v<T, OvMaths::FVector2> ||
		std::is_same_v<T, OvMaths::FVector3> ||
		std::is_same_v<T, OvMaths::FVector4> ||
		std::is_same_v<T, OvMaths::FQuaternion> ||
		std::is_same_v<T, OvMaths::FMatrix3> ||
		std::is_same_v<T, OvMaths::FMatrix4>;

	template<typename T>
	using LuaMathOf = std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>;

	/**
	* The Lua name of a maths type
	*/
	template<typename T>
	constexpr const char* LuaMathName =
		std::is_same_v<T, OvMaths::FVector2> ? "Vector2" :
		std::is_same_v<T, OvMaths::FVector3> ? "Vector3" :
		std::is_same_v<T, OvMaths::FVector4> ? "Vector4" :
		std::is_same_v<T, OvMaths::FQuaternion> ? "Quaternion" :
		std::is_same_v<T, OvMaths::FMatrix3> ? "Matrix3" :
		"Matrix4";

	// Its address, one per maths type, is the key of the mark that the metatables of its userdata hold
	template<typename T>
	inline const char kLuaMathMark = 0;

	/**
	* Marks the metatables sol made for the userdata of each maths type (by value, const, by pointer, by const
	* pointer, unique), once the maths usertypes are bound
	* @param p_luaState
	*/
	void MarkMathMetatables(lua_State* p_luaState);

	/**
	* Returns true if the value at a stack index is a maths value of type T: a userdata whose metatable holds the mark
	* of T. It reads one field of the metatable, where sol's own check looks its metatables up by name, for half the
	* cost. A script can give a table the metatable of a vector, never a userdata: the type is checked first
	* @param p_luaState
	* @param p_index
	*/
	template<typename T>
	bool HoldsLuaMath(lua_State* p_luaState, int p_index)
	{
		if (lua_type(p_luaState, p_index) != LUA_TUSERDATA || !lua_getmetatable(p_luaState, p_index))
		{
			return false;
		}

		const bool marked = lua_rawgetp(p_luaState, -1, &kLuaMathMark<T>) != LUA_TNIL;
		lua_pop(p_luaState, 2);
		return marked;
	}

	/**
	* Throws the error of a stack index that holds something else than the maths value expected there: "bad argument
	* #2 to 'Distance' (Vector3 expected, got nil)", or "calling 'Dot' on bad self (...)"
	* @param p_luaState
	* @param p_index
	* @param p_expected (the Lua name of the type expected)
	*/
	[[noreturn]] void ThrowBadMathArgument(lua_State* p_luaState, int p_index, const char* p_expected);

	/**
	* Throws unless the value at a stack index is a maths value of type T, held by value or through a pointer
	* @param p_luaState
	* @param p_index
	*/
	template<typename T>
	void CheckMathArgument(lua_State* p_luaState, int p_index)
	{
		if (!HoldsLuaMath<T>(p_luaState, p_index))
		{
			ThrowBadMathArgument(p_luaState, p_index, LuaMathName<T>);
		}
	}
}

namespace OvMaths
{
	/**
	* sol checks a maths argument before it reads it, with or without its safety checks. A pointer may be nil, which
	* sol takes for a null pointer
	*/
	template<typename T, std::enable_if_t<OvCore::Scripting::IsLuaMath<OvCore::Scripting::LuaMathOf<T>>, int> = 0>
	void sol_lua_check_access(sol::types<T>, lua_State* p_luaState, int p_index, sol::stack::record&)
	{
		if constexpr (std::is_pointer_v<std::remove_reference_t<T>>)
		{
			if (lua_isnoneornil(p_luaState, p_index))
			{
				return;
			}
		}

		OvCore::Scripting::CheckMathArgument<OvCore::Scripting::LuaMathOf<T>>(p_luaState, p_index);
	}

	/**
	* Without its safety checks (Release), sol takes the self of an operator or a field as non_null<T*>, which it would
	* not check: an operator given its operands in the other order (nil + v), or a table given a vector's metatable,
	* would be read as a vector. Lua hands an operator or a field the value it found it on, so another maths type
	* comes only from a script calling a metatable's function itself: the self is checked to be a userdata, which
	* costs next to nothing on the most frequent calls (v.x), where the arguments are checked in full
	*/
	template<typename T, std::enable_if_t<OvCore::Scripting::IsLuaMath<std::remove_cv_t<T>>, int> = 0>
	T* sol_lua_get(sol::types<sol::non_null<T*>>, lua_State* p_luaState, int p_index, sol::stack::record& p_tracking)
	{
		if (lua_type(p_luaState, p_index) != LUA_TUSERDATA)
		{
			OvCore::Scripting::ThrowBadMathArgument(p_luaState, p_index, OvCore::Scripting::LuaMathName<std::remove_cv_t<T>>);
		}

		return sol::stack::unqualified_getter<sol::non_null<T*>>::get(p_luaState, p_index, p_tracking);
	}
}
