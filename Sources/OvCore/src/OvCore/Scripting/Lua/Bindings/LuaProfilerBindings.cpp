/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <OvCore/Scripting/Lua/LuaComponentRef.h>
#include <tracy/TracyLua.hpp>

void BindLuaProfiler(sol::state& p_luaState)
{
	tracy::LuaRegister(p_luaState.lua_state());
}
