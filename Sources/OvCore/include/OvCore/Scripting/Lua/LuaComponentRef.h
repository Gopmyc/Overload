/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

#include <sol/sol.hpp>

#include <OvCore/ECS/Components/AComponent.h>

// Every source that uses sol includes it through this header: the customizations below change how sol pushes and
// reads components, and must be seen the same way everywhere

namespace OvCore::ECS::Components
{
	class Behaviour;
}

namespace OvCore::Scripting
{
	/**
	* A component as Lua holds it: checked on every use, so that a component removed from its actor, or whose actor
	* was freed, raises a Lua error instead of being read through a dangling pointer
	*/
	struct ComponentHandle
	{
		ECS::Components::AComponent* component = nullptr;
		std::weak_ptr<ECS::Components::AComponent> lifetime;
		uint64_t actorGUID = 0;

		/**
		* Returns a handle on a component of an actor
		* @param p_component
		*/
		static ComponentHandle Make(ECS::Components::AComponent& p_component);

		/**
		* Returns true while the component is still on its actor
		*/
		bool IsAlive() const;

		/**
		* Returns the component, or throws when it is gone
		*/
		ECS::Components::AComponent& Resolve() const;
	};

	/**
	* A handle typed by the component it holds, which is what gives it its Lua usertype
	*/
	template<typename T>
	struct ComponentRef : ComponentHandle {};

	/**
	* The components Lua holds through handles: every one but behaviours, which Lua reaches through their tables
	*/
	template<typename T>
	constexpr bool IsLuaComponent =
		std::is_class_v<T> &&
		std::is_base_of_v<ECS::Components::AComponent, T> &&
		!std::is_base_of_v<ECS::Components::Behaviour, T>;

	template<typename T>
	using LuaComponentOf = std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>;

	/**
	* Returns the handle a Lua value holds, or nullptr when it holds none
	* @param p_luaState
	* @param p_index
	*/
	const ComponentHandle* GetComponentHandle(lua_State* p_luaState, int p_index);

	/**
	* Marks the metatable of a component usertype's handles, by which GetComponentHandle tells them from any other
	* userdata
	* @param p_luaState
	* @param p_metatableName
	*/
	void MarkComponentMetatable(lua_State* p_luaState, const char* p_metatableName);

	/**
	* Marks the handles of the given component usertypes, once they are bound
	* @param p_luaState
	*/
	template<typename... T>
	void MarkComponentUsertypes(lua_State* p_luaState)
	{
		(MarkComponentMetatable(p_luaState, sol::usertype_traits<sol::d::u<T>>::metatable().c_str()), ...);
	}

	/**
	* Component:IsAlive(), which reads the handle without resolving it
	* @param p_luaState
	*/
	int LuaComponentIsAlive(lua_State* p_luaState);

	/**
	* The __eq of the handles: the same component, alive or not
	* @param p_luaState
	*/
	int LuaComponentEquals(lua_State* p_luaState);

	/**
	* Throws the error of a component method called on a value that is no such component
	* @param p_expected (the C++ type of the component expected)
	*/
	[[noreturn]] void ThrowWrongSelf(const std::string& p_expected);
}

namespace sol
{
	template<typename T>
	struct unique_usertype_traits<OvCore::Scripting::ComponentRef<T>>
	{
		using type = T;
		using actual_type = OvCore::Scripting::ComponentRef<T>;
		static constexpr bool value = true;

		static bool is_null(lua_State*, const actual_type& p_ref) noexcept
		{
			return p_ref.component == nullptr;
		}

		static type* get(lua_State*, const actual_type& p_ref) noexcept
		{
			return static_cast<type*>(p_ref.component);
		}
	};
}

namespace OvCore::ECS::Components
{
	/**
	* sol pushes a component, by pointer or by reference, as a handle
	*/
	template<typename T, std::enable_if_t<OvCore::Scripting::IsLuaComponent<std::remove_cv_t<T>>, int> = 0>
	int sol_lua_push(sol::types<T*>, lua_State* p_luaState, T* p_component)
	{
		if (!p_component)
		{
			lua_pushnil(p_luaState);
			return 1;
		}

		using Component = std::remove_cv_t<T>;
		OvCore::Scripting::ComponentRef<Component> ref;
		static_cast<OvCore::Scripting::ComponentHandle&>(ref) =
			OvCore::Scripting::ComponentHandle::Make(*const_cast<Component*>(p_component));
		return sol::stack::push(p_luaState, std::move(ref));
	}

	/**
	* sol checks a component's handle before it hands the component to C++: as an argument, and as self when its
	* safety checks are on (Debug)
	*/
	template<typename T, std::enable_if_t<OvCore::Scripting::IsLuaComponent<OvCore::Scripting::LuaComponentOf<T>>, int> = 0>
	void sol_lua_check_access(sol::types<T>, lua_State* p_luaState, int p_index, sol::stack::record&)
	{
		if (const auto* handle = OvCore::Scripting::GetComponentHandle(p_luaState, p_index))
		{
			handle->Resolve();
		}
	}

	/**
	* Without its safety checks (Release), sol takes the self of a member function or variable as non_null<T*>, which
	* it would not check. A handle gives its component if it is still there and of that type; any other value is
	* checked before it is read, so a wrong self raises instead of being read as a component
	*/
	template<typename T, std::enable_if_t<OvCore::Scripting::IsLuaComponent<std::remove_cv_t<T>>, int> = 0>
	T* sol_lua_get(sol::types<sol::non_null<T*>>, lua_State* p_luaState, int p_index, sol::stack::record& p_tracking)
	{
		if (const auto* handle = OvCore::Scripting::GetComponentHandle(p_luaState, p_index))
		{
			p_tracking.use(1);

			if (auto* component = dynamic_cast<T*>(&handle->Resolve()))
			{
				return component;
			}

			OvCore::Scripting::ThrowWrongSelf(sol::detail::demangle<std::remove_cv_t<T>>());
		}

		if (lua_isnoneornil(p_luaState, p_index) || !sol::stack::check<T*>(p_luaState, p_index, &sol::no_panic))
		{
			OvCore::Scripting::ThrowWrongSelf(sol::detail::demangle<std::remove_cv_t<T>>());
		}

		return sol::stack::unqualified_getter<sol::non_null<T*>>::get(p_luaState, p_index, p_tracking);
	}

	namespace UI
	{
		// Argument-dependent lookup only looks in a type's own namespace
		using OvCore::ECS::Components::sol_lua_push;
		using OvCore::ECS::Components::sol_lua_check_access;
		using OvCore::ECS::Components::sol_lua_get;
	}
}
