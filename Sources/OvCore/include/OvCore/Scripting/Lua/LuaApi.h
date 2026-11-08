/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <string>
#include <vector>

namespace sol
{
	class state;
}

namespace OvCore::Scripting::LuaApi
{
	/**
	* The version of the API the engine gives to Lua scripts, which Application.GetApiVersion() returns. The minor
	* version grows when names are added; the major version when a name is removed or changes meaning, which only
	* happens to a name deprecated in an earlier version (see Docs/Scripting.md)
	*/
	constexpr int kVersionMajor = 1;
	constexpr int kVersionMinor = 1;

	/**
	* Opens the standard libraries a script may use, and removes what would reach the files or load code
	* @param p_state
	*/
	void OpenLibraries(sol::state& p_state);

	/**
	* Binds the engine's API: its tables, usertypes, enums and functions
	* @param p_state
	*/
	void Bind(sol::state& p_state);

	/**
	* The engine's API as a script sees it, sorted: one line per global name the engine adds ("Physics"), and one per
	* member of a table, a usertype or an enum ("Physics.CastRay", "Vector3.x", "Key.SPACE")
	*/
	std::vector<std::string> Describe();
}
