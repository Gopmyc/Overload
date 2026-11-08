/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <OvCore/Scripting/Lua/LuaScriptEngine.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>
#include <sol/sol.hpp>
#include <tracy/Tracy.hpp>

#include <OvDebug/Logger.h>
#include <OvDebug/Assertion.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvCore/Scripting/Lua/LuaApi.h>
#include <OvCore/Scripting/ScriptEngine.h>
#include <OvCore/ECS/Components/Behaviour.h>
#include <OvCore/ECS/Actor.h>
#include <OvTools/Utils/PathParser.h>
#include <OvTools/Utils/String.h>

namespace OvCore::Scripting
{
	struct LuaFrameCallback
	{
		uint64_t id = 0;
		sol::protected_function callback;
		std::optional<ActorRef> owner;
		bool removed = false;
	};

	struct LuaFrameEvents
	{
		// Shared, so that a callback that adds or removes one while the event runs never moves the one running
		std::vector<std::shared_ptr<LuaFrameCallback>> update;
		std::vector<std::shared_ptr<LuaFrameCallback>> lateUpdate;
		uint64_t nextId = 1;
		OvTools::Eventing::ListenerID updateListener = 0;
		OvTools::Eventing::ListenerID lateUpdateListener = 0;
	};
}

namespace
{
	/**
	* Runs the callbacks of an event: those added while it runs start at the next frame, a removed one is skipped at
	* once, and one that fails or whose owner is gone is removed
	*/
	void RunFrameCallbacks(std::vector<std::shared_ptr<OvCore::Scripting::LuaFrameCallback>>& p_callbacks, float p_deltaTime, std::string_view p_eventName)
	{
		const auto running = p_callbacks;

		for (const auto& entry : running)
		{
			if (entry->removed)
				continue;

			if (entry->owner && !entry->owner->IsAlive())
			{
				entry->removed = true;
				continue;
			}

			const sol::protected_function_result result = entry->callback(p_deltaTime);

			if (!result.valid())
			{
				const sol::error error = result;
				OVLOG_ERROR(std::format("Events.{}: a callback failed and is removed: {}", p_eventName, error.what()));
				entry->removed = true;
			}
		}

		std::erase_if(p_callbacks, [](const auto& p_entry) { return p_entry->removed; });
	}

	// Registry keys for the shared scripts. Living in the registry keeps them out of _G,
	// and they die with the lua state, so Reload() invalidates them for free.
	constexpr auto kLoadedScriptsKey = "__OverloadLoadedScripts";
	constexpr auto kLoadingScriptsKey = "__OverloadLoadingScripts";
	constexpr auto kCompiledBehavioursKey = "__OverloadCompiledBehaviours";

	template<typename... Args>
	void ExecuteLuaFunction(OvCore::ECS::Components::Behaviour& p_behaviour, const std::string& p_functionName, Args&& ...p_args)
	{
		auto context = p_behaviour.GetScript();

		OVASSERT(context.has_value(), "The given context is null");
		OVASSERT(context->IsValid(), "The given context is invalid");

		const sol::table& table = *static_cast<OvCore::Scripting::LuaScript&>(context.value()).GetContext().table;

		try
		{
			// Single lookup (it is done for every behaviour, for every callback, every frame)
			const sol::object function = table[p_functionName];

			if (function.valid())
			{
				// One zone per callback, named after its script and function, so the profiler tells which script costs what
				ZoneScopedN("Lua Callback");
				if (TracyIsConnected)
				{
					const auto zoneName = p_behaviour.name + ":" + p_functionName;
					ZoneName(zoneName.c_str(), zoneName.size());
				}

				sol::protected_function pfr = function;
				auto pfrResult = pfr.call(table, std::forward<Args>(p_args)...);
				if (!pfrResult.valid())
				{
					sol::error err = pfrResult;
					OVLOG_ERROR(err.what());
				}
			}
		}
		catch (const sol::error& p_error)
		{
			OVLOG_ERROR(p_error.what());
		}
	}

	sol::table LoadScript(sol::state& p_luaState, const std::string& p_scriptName)
	{
		using namespace OvCore::Scripting;

		// A behaviour script is read and compiled once per Lua state, but run for each behaviour, which so gets a table
		// and functions of its own
		sol::table compiledBehaviours = p_luaState.registry()[kCompiledBehavioursKey];
		const sol::object compiled = compiledBehaviours[p_scriptName];
		sol::protected_function chunk;

		if (compiled.get_type() == sol::type::function)
		{
			chunk = compiled.as<sol::protected_function>();
		}
		else
		{
			sol::load_result loaded = p_luaState.load_file(p_scriptName);

			if (!loaded.valid())
			{
				sol::error err = loaded;
				OVLOG_ERROR(err.what());
				return {};
			}

			chunk = loaded.get<sol::protected_function>();
			compiledBehaviours[p_scriptName] = chunk;
		}

		const sol::protected_function_result result = chunk();

		if (!result.valid())
		{
			sol::error err = result;
			OVLOG_ERROR(err.what());
			return {};
		}
		else
		{
			if (result.return_count() == 1 && result[0].is<sol::table>())
			{
				return result[0];
			}
			else
			{
				OVLOG_ERROR("'" + p_scriptName + "' missing return expression");
				return {};
			}
		}
	}

	bool IsWithinRoot(const std::filesystem::path& p_path, const std::filesystem::path& p_root)
	{
		const auto relativePath = p_path.lexically_relative(p_root);
		return !relativePath.empty() && relativePath.begin()->string() != "..";
	}

	bool RegisterBehaviour(sol::state& p_luaState, OvCore::ECS::Components::Behaviour& p_behaviour, const std::string& p_scriptName)
	{
		auto table = LoadScript(p_luaState, p_scriptName);

		p_behaviour.SetScript(std::make_unique<OvCore::Scripting::LuaScript>(table));

		// Update the script context to add the owner reference
		if (auto context = p_behaviour.GetScript(); context.has_value() && context->IsValid())
		{
			auto& luaScript = static_cast<OvCore::Scripting::LuaScript&>(context.value());
			luaScript.SetOwner(p_behaviour.owner);
			return true;
		}

		return false;
	}

	std::string GetLuarcFileContent(
		const std::filesystem::path& p_engineResourcesFolder
	)
	{
		const auto absolutePath = std::filesystem::absolute(p_engineResourcesFolder);

		return std::format(
			"{{\n"
			"  \"workspace.library\": [\"{}\"],\n"
			"  \"runtime.version\": \"Lua {}.{}\",\n"
			"  \"runtime.builtin\": {{\n"
			"    \"basic\": \"enable\",\n"
			"    \"math\": \"enable\",\n"
			"    \"string\": \"enable\",\n"
			"    \"table\": \"enable\",\n"
			"    \"coroutine\": \"enable\",\n"
			"    \"utf8\": \"enable\",\n"
			"    \"io\": \"disable\",\n"
			"    \"os\": \"disable\",\n"
			"    \"package\": \"disable\",\n"
			"    \"debug\": \"disable\"\n"
			"  }}\n"
			"}}\n",
			absolutePath.string(),
			LUA_VERSION_MAJOR,
			LUA_VERSION_MINOR
		);
	}
}

template<>
OvCore::Scripting::LuaScriptEngineBase::TScriptEngine(
	const std::filesystem::path& p_projectAssetsPath,
	const std::filesystem::path& p_engineAssetsPath
)
{
	m_context.projectAssetsPath = p_projectAssetsPath;
	m_context.engineAssetsPath = p_engineAssetsPath;
}

template<>
OvCore::Scripting::LuaScriptEngineBase::~TScriptEngine() {}

template<>
bool OvCore::Scripting::LuaScriptEngineBase::CreateProjectFiles(const std::filesystem::path& p_projectFolder, bool p_force)
{
	// Create a .luarc.json file at the root of the user's project.
	// This file will allow Lua LSPs to properly discover Lua symbols exposed by Overload.
	const std::filesystem::path luarcPath = p_projectFolder / ".luarc.json";

	// Prevent the .luarc.json from being overrided UNLESS p_force is used
	if (!p_force && std::filesystem::exists(luarcPath))
	{
		return true;
	}

	std::ofstream luarc(luarcPath);
	luarc << GetLuarcFileContent(m_context.engineAssetsPath);
	return true;
}

template<>
std::string OvCore::Scripting::LuaScriptEngineBase::GetDefaultExtension()
{
	return ".lua";
}

template<>
std::unordered_set<std::string> OvCore::Scripting::LuaScriptEngineBase::GetValidExtensions()
{
	return { GetDefaultExtension() };
}

template<>
std::string OvCore::Scripting::LuaScriptEngineBase::GetDefaultScriptContent(const std::string& p_name)
{
	std::string scriptTableName = p_name;
	OvTools::Utils::String::ReplaceAll(scriptTableName, " ", "_");

	return
		"---@class " + scriptTableName + " : Behaviour\n"
		"local " + scriptTableName + " =\n"
		"{\n"
		"}\n"
		"\n"
		"function " + scriptTableName + ":OnStart()\n"
		"end\n"
		"\n"
		"function " + scriptTableName + ":OnUpdate(deltaTime)\n"
		"end\n"
		"\n"
		"return " + scriptTableName;
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::AddBehaviour(OvCore::ECS::Components::Behaviour& p_toAdd)
{
	OVASSERT(m_context.luaState != nullptr, "No valid Lua context");

	m_context.behaviours.push_back(std::ref(p_toAdd));

	// A Behaviour of the project's assets, or of the engine's with ":"
	const auto scriptPath = OvTools::Utils::PathParser::GetRealPath(p_toAdd.name, m_context.engineAssetsPath, m_context.projectAssetsPath);

	if (!RegisterBehaviour(*m_context.luaState, p_toAdd, scriptPath.string()))
	{
		++m_context.errorCount;
	}
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::Reload()
{
	static_cast<LuaScriptEngine&>(*this).DestroyContext();
	static_cast<LuaScriptEngine&>(*this).CreateContext();
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::RemoveBehaviour(OvCore::ECS::Components::Behaviour& p_toRemove)
{
	if (m_context.luaState)
	{
		p_toRemove.RemoveScript();
	}

	m_context.behaviours.erase(
		std::remove_if(m_context.behaviours.begin(), m_context.behaviours.end(),
			[&p_toRemove](std::reference_wrapper< OvCore::ECS::Components::Behaviour> behaviour) {
				return &p_toRemove == &behaviour.get();
			}
		)
	);

	// Every behaviour holds a table of its own, released by RemoveScript, so there is nothing left to reparse.
	// Rebuilding the state here would reset the other behaviours and the shared scripts, and a scene change,
	// which removes every behaviour of the old scene, would lose what the game keeps for the whole session.
}

template<>
bool OvCore::Scripting::LuaScriptEngineBase::IsOk() const
{
	return m_context.luaState && m_context.errorCount == 0;
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::OnAwake(OvCore::ECS::Components::Behaviour& p_target)
{
	ExecuteLuaFunction(p_target, "OnAwake");
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::OnStart(OvCore::ECS::Components::Behaviour& p_target)
{
	ExecuteLuaFunction(p_target, "OnStart");
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::OnEnable(OvCore::ECS::Components::Behaviour& p_target)
{
	ExecuteLuaFunction(p_target, "OnEnable");
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::OnDisable(OvCore::ECS::Components::Behaviour& p_target)
{
	ExecuteLuaFunction(p_target, "OnDisable");
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::OnDestroy(OvCore::ECS::Components::Behaviour& p_target)
{
	ExecuteLuaFunction(p_target, "OnDestroy");
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::OnUpdate(OvCore::ECS::Components::Behaviour& p_target, float p_deltaTime)
{
	ExecuteLuaFunction(p_target, "OnUpdate", p_deltaTime);
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::OnFixedUpdate(OvCore::ECS::Components::Behaviour& p_target, float p_deltaTime)
{
	ExecuteLuaFunction(p_target, "OnFixedUpdate", p_deltaTime);
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::OnLateUpdate(OvCore::ECS::Components::Behaviour& p_target, float p_deltaTime)
{
	ExecuteLuaFunction(p_target, "OnLateUpdate", p_deltaTime);
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::OnCollisionEnter(OvCore::ECS::Components::Behaviour& p_target, OvCore::ECS::Components::CPhysicalObject& p_otherObject)
{
	ExecuteLuaFunction(p_target, "OnCollisionEnter", p_otherObject);
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::OnCollisionStay(OvCore::ECS::Components::Behaviour& p_target, OvCore::ECS::Components::CPhysicalObject& p_otherObject)
{
	ExecuteLuaFunction(p_target, "OnCollisionStay", p_otherObject);
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::OnCollisionExit(OvCore::ECS::Components::Behaviour& p_target, OvCore::ECS::Components::CPhysicalObject& p_otherObject)
{
	ExecuteLuaFunction(p_target, "OnCollisionExit", p_otherObject);
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::OnTriggerEnter(OvCore::ECS::Components::Behaviour& p_target, OvCore::ECS::Components::CPhysicalObject& p_otherObject)
{
	ExecuteLuaFunction(p_target, "OnTriggerEnter", p_otherObject);
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::OnTriggerStay(OvCore::ECS::Components::Behaviour& p_target, OvCore::ECS::Components::CPhysicalObject& p_otherObject)
{
	ExecuteLuaFunction(p_target, "OnTriggerStay", p_otherObject);
}

template<>
void OvCore::Scripting::LuaScriptEngineBase::OnTriggerExit(OvCore::ECS::Components::Behaviour& p_target, OvCore::ECS::Components::CPhysicalObject& p_otherObject)
{
	ExecuteLuaFunction(p_target, "OnTriggerExit", p_otherObject);
}

OvCore::Scripting::LuaScriptEngine::LuaScriptEngine(
	const std::filesystem::path& p_projectAssetsPath,
	const std::filesystem::path& p_engineAssetsPath
) : OvCore::Scripting::LuaScriptEngineBase(
	p_projectAssetsPath,
	p_engineAssetsPath
)
{
	CreateContext();
}

OvCore::Scripting::LuaScriptEngine::~LuaScriptEngine()
{
	DestroyContext();
}

void OvCore::Scripting::LuaScriptEngine::CreateContext()
{
	OVASSERT(m_context.luaState == nullptr, "A Lua context already exists!");

	m_context.luaState = std::make_unique<sol::state>();
	LuaApi::OpenLibraries(*m_context.luaState);

	// Scripts allocate mostly short-lived values every frame (vectors, temporary tables): the
	// generational collector reclaims them for less than the incremental one
	lua_gc(m_context.luaState->lua_state(), LUA_GCGEN, 0, 0);

	m_context.luaState->registry()[kLoadedScriptsKey] = m_context.luaState->create_table();
	m_context.luaState->registry()[kLoadingScriptsKey] = m_context.luaState->create_table();
	m_context.luaState->registry()[kCompiledBehavioursKey] = m_context.luaState->create_table();

	LuaApi::Bind(*m_context.luaState);

	m_context.frameEvents = std::make_unique<LuaFrameEvents>();
	m_context.frameEvents->updateListener = SceneSystem::Scene::UpdateEvent += [this](float p_deltaTime)
	{
		RunFrameCallbacks(m_context.frameEvents->update, p_deltaTime, "AddUpdate");
	};
	m_context.frameEvents->lateUpdateListener = SceneSystem::Scene::LateUpdateEvent += [this](float p_deltaTime)
	{
		RunFrameCallbacks(m_context.frameEvents->lateUpdate, p_deltaTime, "AddLateUpdate");
	};

	m_context.errorCount = 0;

	std::for_each(m_context.behaviours.begin(), m_context.behaviours.end(),
		[this](std::reference_wrapper<OvCore::ECS::Components::Behaviour> behaviour) {
			const auto scriptPath = OvTools::Utils::PathParser::GetRealPath(behaviour.get().name, m_context.engineAssetsPath, m_context.projectAssetsPath);
			if (!RegisterBehaviour(*m_context.luaState, behaviour.get(), scriptPath.string()))
			{
				++m_context.errorCount;
			}
		}
	);

	if (m_context.errorCount > 0)
	{
		const std::string message = std::to_string(m_context.errorCount) + " script(s) failed to register";
		OVLOG_ERROR(message);
	}
}

void OvCore::Scripting::LuaScriptEngine::DestroyContext()
{
	OVASSERT(m_context.luaState != nullptr, "No valid Lua context");

	std::for_each(m_context.behaviours.begin(), m_context.behaviours.end(),
		[this](std::reference_wrapper<OvCore::ECS::Components::Behaviour> behaviour)
		{
			behaviour.get().RemoveScript();
		}
	);

	// The callbacks hold Lua functions: they go before the state
	if (m_context.frameEvents)
	{
		SceneSystem::Scene::UpdateEvent -= m_context.frameEvents->updateListener;
		SceneSystem::Scene::LateUpdateEvent -= m_context.frameEvents->lateUpdateListener;
		m_context.frameEvents.reset();
	}

	m_context.luaState.reset();
}

sol::object OvCore::Scripting::LuaScriptEngine::GetScript(const std::string& p_path)
{
	std::string error;
	sol::object script = LoadModule(p_path, error);

	if (!error.empty())
	{
		OVLOG_ERROR(error);
	}

	return script;
}

sol::object OvCore::Scripting::LuaScriptEngine::RequireScript(const std::string& p_path)
{
	std::string error;
	sol::object script = LoadModule(p_path, error);

	// sol turns the exception into a Lua error, raised where require was called
	if (!error.empty())
	{
		throw std::runtime_error("require: " + error);
	}

	return script;
}

sol::object OvCore::Scripting::LuaScriptEngine::LoadModule(const std::string& p_path, std::string& p_error)
{
	OVASSERT(m_context.luaState != nullptr, "No valid Lua context");

	auto& luaState = *m_context.luaState;
	const auto nil = sol::make_object(luaState, sol::lua_nil);

	// require names a module with or without its extension
	std::string path = p_path;
	if (!std::filesystem::path(path).has_extension())
	{
		path += GetDefaultExtension();
	}

	const auto realPath = OvTools::Utils::PathParser::GetRealPath(
		path,
		m_context.engineAssetsPath,
		m_context.projectAssetsPath
	);

	if (!IsWithinRoot(realPath, m_context.engineAssetsPath) && !IsWithinRoot(realPath, m_context.projectAssetsPath))
	{
		p_error = "'" + p_path + "' is outside of the assets folders";
		return nil;
	}

	if (!GetValidExtensions().contains(realPath.extension().string()))
	{
		p_error = "'" + p_path + "' isn't a script";
		return nil;
	}

	const auto key = realPath.generic_string();
	sol::table loadedScripts = luaState.registry()[kLoadedScriptsKey];

	if (const sol::object loaded = loadedScripts[key]; loaded.valid())
	{
		return loaded;
	}

	if (std::error_code error; !std::filesystem::is_regular_file(realPath, error))
	{
		p_error = "'" + p_path + "' not found";
		return nil;
	}

	sol::table loadingScripts = luaState.registry()[kLoadingScriptsKey];

	if (loadingScripts[key].valid())
	{
		p_error = "'" + p_path + "' is part of a circular script dependency";
		return nil;
	}

	loadingScripts[key] = true;
	const auto result = luaState.safe_script_file(key, &sol::script_pass_on_error, sol::load_mode::text);
	loadingScripts[key] = sol::lua_nil;

	if (!result.valid())
	{
		sol::error err = result;
		p_error = err.what();
		return nil;
	}

	if (result.return_count() == 0)
	{
		p_error = "'" + p_path + "' missing return expression";
		return nil;
	}

	const sol::object script = result[0];
	loadedScripts[key] = script;

	return script;
}

uint64_t OvCore::Scripting::LuaScriptEngine::AddFrameCallback(EFrameEvent p_event, const sol::object& p_callback, std::optional<ActorRef> p_owner)
{
	OVASSERT(m_context.frameEvents != nullptr, "No valid Lua context");

	auto entry = std::make_shared<LuaFrameCallback>();
	entry->id = m_context.frameEvents->nextId++;
	entry->callback = p_callback.as<sol::protected_function>();
	entry->owner = p_owner;

	auto& callbacks = p_event == EFrameEvent::UPDATE ? m_context.frameEvents->update : m_context.frameEvents->lateUpdate;
	callbacks.push_back(entry);

	return entry->id;
}

bool OvCore::Scripting::LuaScriptEngine::RemoveFrameCallback(uint64_t p_id)
{
	if (!m_context.frameEvents)
		return false;

	for (auto* callbacks : { &m_context.frameEvents->update, &m_context.frameEvents->lateUpdate })
	{
		const auto found = std::find_if(callbacks->begin(), callbacks->end(), [p_id](const auto& p_entry)
		{
			return p_entry->id == p_id && !p_entry->removed;
		});

		if (found != callbacks->end())
		{
			// Marked first: an event that is running skips it at once
			(*found)->removed = true;
			callbacks->erase(found);
			return true;
		}
	}

	return false;
}
