/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <string>

#include <doctest/doctest.h>

#include <OvCore/Helpers/VersionedSave.h>
#include <OvCore/Input/GameInput.h>
#include <OvCore/Input/InputActions.h>
#include <OvTests/TestUtils.h>
#include <OvTools/Filesystem/IniFile.h>
#include <OvTools/Filesystem/SaveStorage.h>
#include <OvTools/Utils/Json.h>

namespace VersionedSave = OvCore::Helpers::VersionedSave;
using OvTools::Utils::Json::Value;

namespace
{
	Value Parse(std::string_view p_text)
	{
		std::string error;
		const auto value = OvTools::Utils::Json::Parse(p_text, error);
		REQUIRE_MESSAGE(value.has_value(), error);
		return *value;
	}

	std::string Read(OvTools::Filesystem::SaveStorage& p_storage, std::string_view p_name)
	{
		std::string error;
		return p_storage.Read(p_name, error).value_or("");
	}

	void Write(OvTools::Filesystem::SaveStorage& p_storage, std::string_view p_name, std::string_view p_text)
	{
		std::string error;
		REQUIRE_MESSAGE(p_storage.Write(p_name, p_text, error), error);
	}

	/**
	* A project whose actions file declares Jump (SPACE) and Interact (E), with its save folder
	*/
	struct ActionsProject
	{
		OvTests::TempFolder folder;
		OvTools::Filesystem::IniFile ini;
		OvTools::Filesystem::SaveStorage storage;

		ActionsProject() :
			ini(folder.WriteFile("Test.ovproject", "input_actions=Settings/Actions.json\n").string()),
			storage(folder.GetPath() / "Saves")
		{
			folder.WriteFile("Assets/Settings/Actions.json", R"({ "actions": [
				{ "name": "Jump", "inputs": ["SPACE"] },
				{ "name": "Interact", "inputs": ["E"] }
			] })");
		}

		/**
		* The second slot of Jump, once the project's actions and the player's bindings are loaded
		*/
		std::string JumpSecond()
		{
			OvCore::Input::InputActions actions;
			OvCore::Input::LoadProjectActions(ini, folder.GetPath() / "Assets", storage, actions);
			const auto bindings = actions.GetBindings("Jump");
			REQUIRE(bindings.has_value());
			return (*bindings)[1] ? OvCore::Input::GetInputName(*(*bindings)[1]) : "";
		}
	};
}

TEST_SUITE("Versioned saves")
{
	TEST_CASE("the envelope holds a whole version and an object or a list; anything else is version 0, whole")
	{
		const Value data = Parse(R"({ "volume": 0.5 })");
		const Value wrapped = VersionedSave::Wrap(3, data);
		CHECK(Parse(OvTools::Utils::Json::Write(wrapped)) == Parse(R"({ "version": 3, "data": { "volume": 0.5 } })"));
		CHECK(OvTools::Utils::Json::Write(wrapped).find("\"version\": 3") != std::string::npos);

		const auto unwrapped = VersionedSave::Unwrap(wrapped);
		CHECK(unwrapped.version == 3);
		CHECK(unwrapped.data == data);

		CHECK(VersionedSave::Unwrap(Parse(R"({ "version": 1, "data": [1, 2] })")).version == 1);

		for (const char* notEnvelope : {
			R"({ "language": "fr" })",
			R"({ "version": 1, "data": {}, "extra": true })",
			R"({ "version": 1.5, "data": {} })",
			R"({ "version": 0, "data": {} })",
			R"({ "version": "1", "data": {} })",
			R"({ "version": 1, "data": "text" })",
			R"([1, 2])" })
		{
			CAPTURE(notEnvelope);
			const Value value = Parse(notEnvelope);
			const auto result = VersionedSave::Unwrap(value);
			CHECK(result.version == 0);
			CHECK(result.data == value);
		}
	}

	TEST_CASE("a file is set aside beside itself, its suffix before its extension")
	{
		CHECK(VersionedSave::AsideName("settings.json", "broken") == "settings.broken.json");
		CHECK(VersionedSave::AsideName("slots/1.json", "v3") == "slots/1.v3.json");
		CHECK(VersionedSave::AsideName("a.b.json", "broken") == "a.b.broken.json");
		CHECK(VersionedSave::AsideName("save", "broken") == "save.broken");
		CHECK(VersionedSave::AsideName("slots/save", "v2") == "slots/save.v2");
	}

	TEST_CASE("a file is read at its version or an older one; a newer or a broken one is set aside")
	{
		OvTests::TempFolder folder;
		OvTools::Filesystem::SaveStorage storage(folder.GetPath());
		OvTests::LogCapture logs;

		// No file: nothing, and nothing to tell
		CHECK_FALSE(VersionedSave::Read(storage, "prefs.json", 1).has_value());
		CHECK(logs.Count(OvDebug::ELogLevel::LOG_WARNING) == 0);

		// Written in its envelope, read back
		REQUIRE(VersionedSave::Write(storage, "prefs.json", 1, Parse(R"({ "language": "fr" })")));
		CHECK(Parse(Read(storage, "prefs.json")) == Parse(R"({ "version": 1, "data": { "language": "fr" } })"));
		const auto read = VersionedSave::Read(storage, "prefs.json", 1);
		REQUIRE(read.has_value());
		CHECK(*read == Parse(R"({ "language": "fr" })"));

		// From before the envelope: version 0, the same shape
		Write(storage, "prefs.json", R"({ "language": "ru" })");
		CHECK(*VersionedSave::Read(storage, "prefs.json", 1) == Parse(R"({ "language": "ru" })"));
		CHECK(logs.Count(OvDebug::ELogLevel::LOG_WARNING) == 0);

		// Newer: set aside, the original kept until the next write
		const std::string newer = R"({ "version": 2, "data": { "language": "de" } })";
		Write(storage, "prefs.json", newer);
		CHECK_FALSE(VersionedSave::Read(storage, "prefs.json", 1).has_value());
		CHECK(Read(storage, "prefs.v2.json") == newer);
		CHECK(Read(storage, "prefs.json") == newer);
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_WARNING, "prefs.json: version 2 is newer than the engine's, 1; set aside as prefs.v2.json, the defaults stand"));

		// Not JSON
		Write(storage, "prefs.json", "{ nope");
		CHECK_FALSE(VersionedSave::Read(storage, "prefs.json", 1).has_value());
		CHECK(Read(storage, "prefs.broken.json") == "{ nope");
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_WARNING, "set aside as prefs.broken.json"));

		// The next write replaces it
		REQUIRE(VersionedSave::Write(storage, "prefs.json", 1, Parse(R"({ "language": "en" })")));
		CHECK(*VersionedSave::Read(storage, "prefs.json", 1) == Parse(R"({ "language": "en" })"));
	}

	TEST_CASE("the engine's bindings are saved in the envelope, and read at version 0 or 1")
	{
		ActionsProject project;
		CHECK(project.JumpSecond() == "");

		// The player's binding, saved by the engine in its envelope
		{
			OvCore::Input::InputActions actions;
			OvCore::Input::LoadProjectActions(project.ini, project.folder.GetPath() / "Assets", project.storage, actions);
			std::string error;
			REQUIRE(actions.SetBinding("Jump", 1, OvCore::Input::ParseInput("F"), error));
			OvCore::Input::SaveBindings(project.storage, actions);
		}

		const auto saved = VersionedSave::Unwrap(Parse(Read(project.storage, OvCore::Input::kBindingsFile)));
		CHECK(saved.version == OvCore::Input::kBindingsVersion);
		CHECK(saved.data.Find("bindings") != nullptr);
		CHECK(project.JumpSecond() == "F");

		// A file from before the envelope still reads
		Write(project.storage, OvCore::Input::kBindingsFile, OvTools::Utils::Json::Write(saved.data));
		CHECK(project.JumpSecond() == "F");

		// One of a newer version keeps the defaults, set aside
		OvTests::LogCapture logs;
		Write(project.storage, OvCore::Input::kBindingsFile, OvTools::Utils::Json::Write(VersionedSave::Wrap(OvCore::Input::kBindingsVersion + 1, saved.data)));
		CHECK(project.JumpSecond() == "");
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_WARNING, "input_bindings.json: version 2 is newer than the engine's, 1; set aside as input_bindings.v2.json"));
	}
}
