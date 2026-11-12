/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <format>
#include <stdexcept>

#include <OvCore/ECS/Actor.h>
#include <OvCore/Scripting/Lua/LuaComponentRef.h>

namespace
{
	// Its address is the key of the mark on the handles' metatables
	const char kHandleMark = 0;
}

OvCore::Scripting::ComponentHandle OvCore::Scripting::ComponentHandle::Make(ECS::Components::AComponent& p_component)
{
	ComponentHandle handle;
	handle.component = &p_component;
	handle.lifetime = p_component.owner.GetComponentShared(p_component);
	handle.actorGUID = p_component.owner.GetGUID();
	return handle;
}

bool OvCore::Scripting::ComponentHandle::IsAlive() const
{
	return component && !lifetime.expired();
}

OvCore::ECS::Components::AComponent& OvCore::Scripting::ComponentHandle::Resolve() const
{
	if (!IsAlive())
	{
		throw std::runtime_error(std::format(
			"attempt to use a component that was removed, or whose actor was destroyed (actor GUID: {:016X})", actorGUID));
	}

	return *component;
}

const OvCore::Scripting::ComponentHandle* OvCore::Scripting::GetComponentHandle(lua_State* p_luaState, int p_index)
{
	if (lua_type(p_luaState, p_index) != LUA_TUSERDATA)
	{
		return nullptr;
	}

	const int index = lua_absindex(p_luaState, p_index);

	if (!lua_getmetatable(p_luaState, index))
	{
		return nullptr;
	}

	const bool isHandle = lua_rawgetp(p_luaState, -1, &kHandleMark) != LUA_TNIL;
	lua_pop(p_luaState, 2);

	if (!isHandle)
	{
		return nullptr;
	}

	// Where sol keeps the handle in the userdata of a unique usertype; every ComponentRef has the layout of a
	// ComponentHandle
	return static_cast<const ComponentHandle*>(sol::detail::align_usertype_unique<ComponentHandle>(lua_touserdata(p_luaState, index)));
}

void OvCore::Scripting::MarkComponentMetatable(lua_State* p_luaState, const char* p_metatableName)
{
	luaL_getmetatable(p_luaState, p_metatableName);

	if (lua_istable(p_luaState, -1))
	{
		lua_pushboolean(p_luaState, 1);
		lua_rawsetp(p_luaState, -2, &kHandleMark);

		// Two handles on one component are equal, even once it is gone: Lua reads __eq from the operands' own
		// metatables, never from a base usertype's
		lua_pushcfunction(p_luaState, &LuaComponentEquals);
		lua_setfield(p_luaState, -2, "__eq");
	}

	lua_pop(p_luaState, 1);
}

int OvCore::Scripting::LuaComponentEquals(lua_State* p_luaState)
{
	const auto* left = GetComponentHandle(p_luaState, 1);
	const auto* right = GetComponentHandle(p_luaState, 2);

	// The same object and the same owner: a component made where a freed one was is another
	const bool equal =
		left && right &&
		left->component == right->component &&
		!left->lifetime.owner_before(right->lifetime) &&
		!right->lifetime.owner_before(left->lifetime);

	lua_pushboolean(p_luaState, equal);
	return 1;
}

void OvCore::Scripting::ThrowWrongSelf(const std::string& p_expected)
{
	throw std::runtime_error(std::format(
		"a method of {} was called on a value that is not one (use ':' to call a method)", p_expected));
}

int OvCore::Scripting::LuaComponentIsAlive(lua_State* p_luaState)
{
	const auto* handle = GetComponentHandle(p_luaState, 1);
	lua_pushboolean(p_luaState, handle && handle->IsAlive());
	return 1;
}
