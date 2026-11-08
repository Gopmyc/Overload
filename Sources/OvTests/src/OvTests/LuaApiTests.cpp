/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>

#include <doctest/doctest.h>
#include <sol/sol.hpp>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/Behaviour.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvCore/Scripting/Lua/LuaApi.h>
#include <OvCore/Scripting/ScriptEngine.h>
#include <OvTests/TestUtils.h>

namespace
{
	/**
	* The names the definitions of Resources/Engine/Lua declare, as LuaApi::Describe names them
	*/
	struct Definitions
	{
		std::set<std::string> names;
		std::map<std::string, std::string> baseClasses; // a class and the class it derives from
		std::set<std::string> globals; // the names assigned at the top level ("Physics = {}")
	};

	Definitions ReadDefinitions(const std::filesystem::path& p_folder)
	{
		static const std::regex kClass(R"(^---@class\s+([A-Za-z_]\w*)(?:\s*:\s*([A-Za-z_]\w*))?)");
		static const std::regex kField(R"(^---@field\s+([A-Za-z_]\w*))");
		static const std::regex kEnum(R"(^---@enum\s+([A-Za-z_]\w*))");
		static const std::regex kMember(R"(^function\s+([A-Za-z_]\w*)[.:]([A-Za-z_]\w*)\s*\()");
		static const std::regex kFunction(R"(^function\s+([A-Za-z_]\w*)\s*\()");
		static const std::regex kGlobal(R"(^([A-Za-z_]\w*)\s*=\s*(.*)$)");
		static const std::regex kEnumValue(R"(^\s*([A-Za-z_]\w*)\s*=)");

		Definitions definitions;

		for (const auto& entry : std::filesystem::recursive_directory_iterator(p_folder))
		{
			if (entry.path().extension() != ".lua")
				continue;

			std::ifstream file(entry.path());
			std::string line, currentClass, currentEnum, openEnum;
			std::smatch match;

			while (std::getline(file, line))
			{
				if (!line.empty() && line.back() == '\r')
					line.pop_back();

				if (!openEnum.empty())
				{
					if (line.find('}') != std::string::npos)
						openEnum.clear();
					else if (std::regex_search(line, match, kEnumValue))
						definitions.names.insert(openEnum + "." + match[1].str());
				}
				else if (std::regex_search(line, match, kClass))
				{
					currentClass = match[1];
					if (match[2].matched)
						definitions.baseClasses[currentClass] = match[2];
				}
				else if (std::regex_search(line, match, kField))
				{
					if (!currentClass.empty())
						definitions.names.insert(currentClass + "." + match[1].str());
				}
				else if (std::regex_search(line, match, kEnum))
				{
					currentEnum = match[1];
				}
				else if (std::regex_search(line, match, kMember))
				{
					definitions.names.insert(match[1].str() + "." + match[2].str());
				}
				else if (std::regex_search(line, match, kFunction))
				{
					definitions.names.insert(match[1]);
					definitions.globals.insert(match[1]);
				}
				else if (std::regex_search(line, match, kGlobal))
				{
					definitions.names.insert(match[1]);
					definitions.globals.insert(match[1]);

					const std::string value = match[2];
					if (match[1] == currentEnum && value.starts_with("{") && value.find('}') == std::string::npos)
						openEnum = currentEnum;
				}
			}
		}

		return definitions;
	}

	/**
	* Gives each class the members of the classes it derives from, as Lua finds them through its base classes
	*/
	std::set<std::string> WithInheritedMembers(const std::set<std::string>& p_names, const std::map<std::string, std::string>& p_baseClasses)
	{
		std::set<std::string> names = p_names;

		for (const auto& [derived, directBase] : p_baseClasses)
		{
			for (auto base = p_baseClasses.find(derived); base != p_baseClasses.end(); base = p_baseClasses.find(base->second))
			{
				const std::string prefix = base->second + ".";
				for (auto name = p_names.lower_bound(prefix); name != p_names.end() && name->starts_with(prefix); ++name)
					names.insert(derived + "." + name->substr(prefix.size()));
			}
		}

		return names;
	}

	std::string Join(const std::vector<std::string>& p_names)
	{
		std::ostringstream text;
		for (const auto& name : p_names)
			text << "\n  " << name;
		return text.str();
	}
}

TEST_SUITE("Lua API")
{
	TEST_CASE("the definitions describe the bindings, no more, no less")
	{
		const auto resources = OvTests::FindEngineResources();
		REQUIRE_MESSAGE(!resources.empty(), "Resources/Engine not found above the test program or the current folder");

		const auto definitions = ReadDefinitions(resources / "Lua");
		const auto described = OvCore::Scripting::LuaApi::Describe();

		// What a script implements, not what the engine binds: a Behaviour's callbacks and owner
		const std::set<std::string> kScriptSide = { "Behaviour" };

		const auto isCompared = [&](const std::string& p_name)
		{
			const std::string owner = p_name.substr(0, p_name.find('.'));
			// A class with no global is the shape of a value (a table given to a function): it has no binding
			return !kScriptSide.contains(owner) && definitions.globals.contains(owner);
		};

		std::set<std::string> defined;
		for (const auto& name : WithInheritedMembers(definitions.names, definitions.baseClasses))
			if (isCompared(name))
				defined.insert(name);

		const auto bound = WithInheritedMembers({ described.begin(), described.end() }, definitions.baseClasses);

		std::vector<std::string> undocumented, unbound;
		std::set_difference(bound.begin(), bound.end(), defined.begin(), defined.end(), std::back_inserter(undocumented));
		std::set_difference(defined.begin(), defined.end(), bound.begin(), bound.end(), std::back_inserter(unbound));

		CHECK_MESSAGE(undocumented.empty(), "bound but not in Resources/Engine/Lua:" << Join(undocumented));
		CHECK_MESSAGE(unbound.empty(), "in Resources/Engine/Lua but not bound:" << Join(unbound));
		CHECK(described.size() > 500);
	}

	TEST_CASE("the API is described as a script sees it")
	{
		const auto described = OvCore::Scripting::LuaApi::Describe();
		const std::set<std::string> names(described.begin(), described.end());

		CHECK(std::is_sorted(described.begin(), described.end()));
		for (const std::string name : { "Physics", "Physics.CastRay", "Vector3.x", "Vector3.new", "Vector3.__add", "Key.SPACE", "require", "Application.GetApiVersion" })
			CHECK_MESSAGE(names.contains(name), name);

		// The standard library, the code loaders the sandbox removes and sol's own machinery are not the engine's API
		for (const std::string name : { "print", "string", "load", "dofile", "Vector3.__eq", "Vector3.__gc", "Vector3.class_check", "Scene.new", "PhysicsHit.new", "AssetRef.new" })
			CHECK_FALSE_MESSAGE(names.contains(name), name);
	}
}

TEST_SUITE("Lua modules")
{
	TEST_CASE("require loads a module once, from the project or the engine, and raises when it cannot")
	{
		OvTests::TempFolder project;
		OvTests::TempFolder engineAssets;

		project.WriteFile("Scripts/Shared.lua", "return { value = 42 }");
		project.WriteFile("Scripts/Broken.lua", "return {");
		project.WriteFile("Scripts/Empty.lua", "local unused = 1");
		project.WriteFile("Scripts/Loop.lua", "return { inner = require('Scripts/Loop') }");
		project.WriteFile("Scripts/data.json", "{}");
		engineAssets.WriteFile("Modules/Engine.lua", "return { name = 'engine' }");
		project.WriteFile("Scripts/Main.lua", R"(
			local results = {}
			local Shared = require("Scripts/Shared")
			results.value = Shared.value
			results.same = require("Scripts/Shared.lua") == Shared and Resources.GetScript("Scripts/Shared.lua") == Shared
			results.engine = require(":Modules/Engine").name
			local function failure(path)
				local ok, message = pcall(require, path)
				return ok and "loaded" or tostring(message)
			end
			results.missing = failure("Scripts/Missing")
			results.outside = failure("../Outside")
			results.broken = failure("Scripts/Broken")
			results.empty = failure("Scripts/Empty")
			results.loop = failure("Scripts/Loop")
			results.notScript = failure("Scripts/data.json")
			results.major, results.minor = Application.GetApiVersion()
			return results
		)");

		// The engine makes its Lua state when built, and closes it when destroyed
		OvCore::Scripting::ScriptEngine engine(project.GetPath(), engineAssets.GetPath());
		OvCore::Global::ServiceLocator::Provide(engine);

		// The references to Lua values go before the state they belong to, which the engine closes when destroyed
		{
			const sol::object loaded = engine.GetScript("Scripts/Main.lua");
			REQUIRE(loaded.get_type() == sol::type::table);
			const sol::table results = loaded.as<sol::table>();

			CHECK(results.get<int>("value") == 42);
			CHECK(results.get<bool>("same"));
			CHECK(results.get<std::string>("engine") == "engine");

			const auto message = [&](const char* p_key) { return results.get<std::string>(p_key); };
			CHECK_MESSAGE(message("missing").find("'Scripts/Missing' not found") != std::string::npos, message("missing"));
			CHECK_MESSAGE(message("outside").find("outside of the assets folders") != std::string::npos, message("outside"));
			CHECK_MESSAGE(message("broken").find("Broken.lua") != std::string::npos, message("broken"));
			CHECK_MESSAGE(message("empty").find("missing return expression") != std::string::npos, message("empty"));
			CHECK_MESSAGE(message("loop").find("circular") != std::string::npos, message("loop"));
			CHECK_MESSAGE(message("notScript").find("isn't a script") != std::string::npos, message("notScript"));
			for (const char* key : { "missing", "outside", "broken", "empty", "loop", "notScript" })
				CHECK_MESSAGE(message(key).find("require: ") != std::string::npos, message(key));

			CHECK(results.get<int>("major") == OvCore::Scripting::LuaApi::kVersionMajor);
			CHECK(results.get<int>("minor") == OvCore::Scripting::LuaApi::kVersionMinor);

		}
	}

	TEST_CASE("global frame events run outside any Behaviour, in order, and drop what fails")
	{
		OvTests::TempFolder project;
		OvTests::TempFolder engineAssets;

		project.WriteFile("Scripts/Frames.lua", R"(
			local Frames = { log = {} }
			local function add(entry) Frames.log[#Frames.log + 1] = entry end

			Events.AddUpdate(function(step) add("U" .. step) end)
			Events.AddLateUpdate(function(step) add("L" .. step) end)
			Events.AddUpdate(function() error("boom") end)

			local removed = Events.AddUpdate(function() add("never") end)
			Frames.removedOnce = Events.Remove(removed)
			Frames.removedTwice = Events.Remove(removed)

			local adder
			adder = Events.AddUpdate(function()
				add("A")
				Events.Remove(adder)
				Events.AddUpdate(function() add("N") end)
			end)

			local ok, message = pcall(Events.AddUpdate, 42)
			Frames.badArgument = ok and "accepted" or tostring(message)
			return Frames
		)");

		OvCore::Scripting::ScriptEngine engine(project.GetPath(), engineAssets.GetPath());
		OvCore::Global::ServiceLocator::Provide(engine);
		OvTests::LogCapture logs;

		{
			const sol::object loaded = engine.GetScript("Scripts/Frames.lua");
			REQUIRE(loaded.get_type() == sol::type::table);
			const sol::table frames = loaded.as<sol::table>();

			CHECK(frames.get<bool>("removedOnce"));
			CHECK_FALSE(frames.get<bool>("removedTwice"));
			CHECK(frames.get<std::string>("badArgument").find("Events.AddUpdate: a function is expected") != std::string::npos);

			// Two frames, as a scene runs them
			OvCore::SceneSystem::Scene::UpdateEvent.Invoke(0.5f);
			OvCore::SceneSystem::Scene::LateUpdateEvent.Invoke(0.5f);
			OvCore::SceneSystem::Scene::UpdateEvent.Invoke(0.25f);
			OvCore::SceneSystem::Scene::LateUpdateEvent.Invoke(0.25f);

			std::vector<std::string> log;
			for (const auto& [index, entry] : frames.get<sol::table>("log"))
				log.push_back(entry.as<std::string>());

			// The adder ran once and removed itself; what it added first ran at the next frame
			CHECK(log == std::vector<std::string>{ "U0.5", "A", "L0.5", "U0.25", "N", "L0.25" });
			CHECK(logs.Count(OvDebug::ELogLevel::LOG_ERROR) == 1);
			CHECK(logs.Contains(OvDebug::ELogLevel::LOG_ERROR, "Events.AddUpdate: a callback failed and is removed"));
			CHECK(logs.Contains(OvDebug::ELogLevel::LOG_ERROR, "boom"));
		}

		// Without a Lua state, the events reach no one
		engine.DestroyContext();
		OvCore::SceneSystem::Scene::UpdateEvent.Invoke(0.5f);
		CHECK_FALSE(engine.RemoveFrameCallback(1));
		engine.CreateContext();
	}

	TEST_CASE("a Behaviour whose path starts with a colon comes from the engine's resources")
	{
		OvTests::TempFolder project;
		OvTests::TempFolder engineAssets;
		engineAssets.WriteFile("Behaviours/Spin.lua", "return { nSpeed = 90 }");
		project.WriteFile("Scripts/Local.lua", "return { nSpeed = 10 }");

		OvCore::Scripting::ScriptEngine engine(project.GetPath(), engineAssets.GetPath());
		OvCore::Global::ServiceLocator::Provide(engine);

		// The actors go before the engine their Behaviours are registered with
		{
			OvCore::SceneSystem::Scene scene(project.GetPath(), engineAssets.GetPath());
			auto& actor = scene.CreateActor("Spinner");

			auto& engineBehaviour = actor.AddBehaviour(":Behaviours/Spin.lua");
			auto& projectBehaviour = actor.AddBehaviour("Scripts/Local.lua");

			for (auto* behaviour : { &engineBehaviour, &projectBehaviour })
			{
				CAPTURE(behaviour->name);
				const auto script = behaviour->GetScript();
				REQUIRE(script.has_value());
				REQUIRE(script->IsValid());
			}

			const auto speed = [](OvCore::ECS::Components::Behaviour& p_behaviour)
			{
				auto& script = static_cast<OvCore::Scripting::LuaScript&>(p_behaviour.GetScript().value());
				// A property reaches the script as a number of the inspector: a float
				return (*script.GetContext().table).get<double>("nSpeed");
			};

			CHECK(speed(engineBehaviour) == doctest::Approx(90.0));
			CHECK(speed(projectBehaviour) == doctest::Approx(10.0));
		}
	}
}
