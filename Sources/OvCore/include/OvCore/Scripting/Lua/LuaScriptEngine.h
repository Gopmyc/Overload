/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <OvCore/Scripting/Common/ScriptPropertyValue.h>
#include <OvCore/Scripting/Common/TScriptEngine.h>

namespace OvCore::ECS::Components
{
	class Behaviour;
}

namespace sol
{
	template <bool b>
	class basic_reference;
	using reference = basic_reference<false>;

	template <typename base_type>
	class basic_object;
	using object = basic_object<reference>;

	class state;
}

namespace OvCore::Scripting
{
	/**
	* The callbacks of the global Update and LateUpdate events (Events.AddUpdate...), which live with the Lua state
	*/
	struct LuaFrameEvents;

	/**
	* Lua script engine context
	*/
	struct LuaScriptEngineContext
	{
		std::unique_ptr<sol::state> luaState;
		std::unique_ptr<LuaFrameEvents> frameEvents;
		std::filesystem::path projectAssetsPath;
		std::filesystem::path engineAssetsPath;
		std::vector<std::reference_wrapper<OvCore::ECS::Components::Behaviour>> behaviours;
		uint32_t errorCount;
	};

	using LuaScriptEngineBase = TScriptEngine<EScriptingLanguage::LUA, LuaScriptEngineContext>;

	/**
	* Lua script engine implementation
	*/
	class LuaScriptEngine : public LuaScriptEngineBase
	{
	public:
		/**
		* Constructor of the lua script engine
		* @param p_projectAssetsPath
		* @param p_engineAssetsPath
		*/
		LuaScriptEngine(
			const std::filesystem::path& p_projectAssetsPath,
			const std::filesystem::path& p_engineAssetsPath
		);

		/**
		* Destructor of the Lua script engine
		*/
		virtual ~LuaScriptEngine();

		/**
		* Create the Lua state
		*/
		void CreateContext();

		/**
		* Destroy the lua state
		*/
		void DestroyContext();

		/**
		* Loads (if not already loaded) and returns the value returned by the script identified
		* by the given path. Returns nil on failure.
		* The returned value is shared by every script, and is discarded along with the lua state.
		* @param p_path
		*/
		sol::object GetScript(const std::string& p_path);

		/**
		* What require returns: the value GetScript returns, the extension optional. A script that cannot be loaded
		* raises a Lua error that says why, where require was called
		* @param p_path
		*/
		sol::object RequireScript(const std::string& p_path);

		/**
		* The global events a script can listen to, outside any Behaviour
		*/
		enum class EFrameEvent
		{
			UPDATE,     // Every frame, before every Behaviour's OnUpdate
			LATE_UPDATE // Every frame, after every Behaviour's OnLateUpdate
		};

		/**
		* Calls the Lua function with the frame's step at each event from the next frame on, until it is removed, it
		* fails, or its owner (when given) is no longer alive. Returns its id
		* @param p_event
		* @param p_callback
		* @param p_owner
		*/
		uint64_t AddFrameCallback(EFrameEvent p_event, const sol::object& p_callback, std::optional<ActorRef> p_owner);

		/**
		* Removes a callback, and returns whether it was there
		* @param p_id
		*/
		bool RemoveFrameCallback(uint64_t p_id);

	private:
		sol::object LoadModule(const std::string& p_path, std::string& p_error);
	};
}
