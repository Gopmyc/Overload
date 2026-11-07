/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <doctest/doctest.h>

#include <OvCore/Helpers/ProjectSettings.h>
#include <OvTests/TestUtils.h>

namespace ProjectSettings = OvCore::Helpers::ProjectSettings;
using OvDebug::ELogLevel;

namespace
{
	/**
	* A project file of these lines, read as the engine reads it
	*/
	struct ProjectFile
	{
		OvTests::TempFolder folder;
		OvTools::Filesystem::IniFile ini;

		explicit ProjectFile(std::string_view p_lines) :
			ini(folder.WriteFile("Test.ovproject", p_lines).string())
		{
		}
	};

	bool Collide(const OvPhysics::Settings::PhysicsSettings& p_settings, uint16_t p_layer1, uint16_t p_layer2)
	{
		return (p_settings.collisionMasks[p_layer1] & (1u << p_layer2)) != 0;
	}
}

TEST_SUITE("Project settings")
{
	TEST_CASE("a project without physics keys keeps the defaults")
	{
		ProjectFile project("executable_name=Game\n");
		const auto settings = ProjectSettings::ReadPhysicsSettings(project.ini);

		CHECK(settings.gravity.y == doctest::Approx(-9.81f));
		CHECK(settings.layerNames[0] == "Default");
		CHECK(settings.layerNames[15] == "Ragdoll");
		CHECK(settings.ragdollLayer == 15);
		CHECK_FALSE(settings.ragdollIgnoresDynamicBodies);
		for (uint16_t layer = 0; layer < 16; ++layer)
			CHECK(settings.collisionMasks[layer] == 0xFFFF);
	}

	TEST_CASE("layers are named, and pairs named by name or index go through each other")
	{
		ProjectFile project(
			"gravity=-20\n"
			"physics_layer_names=Default, Player ,Enemy,,Debris\n"
			"physics_ignored_layer_pairs=Player:Enemy, 4:4 ,Debris:Player\n"
			"physics_ragdoll_layer=Debris\n"
			"physics_ragdoll_ignores_dynamic_bodies=true\n");
		const auto settings = ProjectSettings::ReadPhysicsSettings(project.ini);

		CHECK(settings.gravity.y == doctest::Approx(-20.0f));
		CHECK(settings.layerNames[1] == "Player");
		CHECK(settings.layerNames[2] == "Enemy");
		CHECK(settings.layerNames[3] == "");
		CHECK(settings.layerNames[4] == "Debris");
		CHECK(settings.layerNames[15] == "Ragdoll"); // past the list, a layer keeps its name

		CHECK_FALSE(Collide(settings, 1, 2));
		CHECK_FALSE(Collide(settings, 2, 1));
		CHECK_FALSE(Collide(settings, 4, 4));
		CHECK_FALSE(Collide(settings, 4, 1));
		CHECK(Collide(settings, 1, 1));
		CHECK(Collide(settings, 0, 2));

		CHECK(settings.ragdollLayer == 4);
		CHECK(settings.ragdollIgnoresDynamicBodies);
	}

	TEST_CASE("an entry that names no layer is logged and skipped")
	{
		OvTests::LogCapture logs;
		ProjectFile project(
			"physics_layer_names=Default,Player\n"
			"physics_ignored_layer_pairs=Player:Ghost,16:0,Player,Player:0\n"
			"physics_ragdoll_layer=Ghost\n");
		const auto settings = ProjectSettings::ReadPhysicsSettings(project.ini);

		CHECK(logs.Contains(ELogLevel::LOG_WARNING, "\"Player:Ghost\""));
		CHECK(logs.Contains(ELogLevel::LOG_WARNING, "\"16:0\""));
		CHECK(logs.Contains(ELogLevel::LOG_WARNING, "\"Player\""));
		CHECK(logs.Contains(ELogLevel::LOG_WARNING, "physics_ragdoll_layer"));
		CHECK(settings.ragdollLayer == 15);
		CHECK_FALSE(Collide(settings, 1, 0));
		CHECK(Collide(settings, 1, 2));
	}

	TEST_CASE("the save folder follows project_id, then executable_name")
	{
		CHECK(ProjectSettings::GetSaveName(ProjectFile("project_id=Quest\nexecutable_name=Quest 2\n").ini) == "Quest");
		CHECK(ProjectSettings::GetSaveName(ProjectFile("project_id=  \nexecutable_name= Quest 2 \n").ini) == "Quest 2");
		CHECK(ProjectSettings::GetSaveName(ProjectFile("vsync=true\n").ini) == "Game");
	}

	TEST_CASE("runtime limits default to the former fixed values")
	{
		const auto limits = ProjectSettings::ReadRuntimeLimits(ProjectFile("executable_name=Game\n").ini);
		CHECK(limits.storage.maxFileSize == 4u * 1024 * 1024);
		CHECK(limits.storage.maxTotalSize == 64u * 1024 * 1024);
		CHECK(limits.storage.maxEntryCount == 1024);
		CHECK(limits.json.maxSize == 4u * 1024 * 1024);
		CHECK(limits.json.maxDepth == 64);
		CHECK(limits.maxFrameDelta == doctest::Approx(0.1f));
	}

	TEST_CASE("runtime limits are read, and brought within range")
	{
		{
			const auto limits = ProjectSettings::ReadRuntimeLimits(ProjectFile(
				"storage_max_file_mib=2\nstorage_max_total_mib=8\nstorage_max_entries=50\n"
				"json_max_mib=1\njson_max_depth=10\nmax_frame_delta=0.05\n").ini);
			CHECK(limits.storage.maxFileSize == 2u * 1024 * 1024);
			CHECK(limits.storage.maxTotalSize == 8u * 1024 * 1024);
			CHECK(limits.storage.maxEntryCount == 50);
			CHECK(limits.json.maxSize == 1u * 1024 * 1024);
			CHECK(limits.json.maxDepth == 10);
			CHECK(limits.maxFrameDelta == doctest::Approx(0.05f));
		}

		{
			OvTests::LogCapture logs;
			const auto limits = ProjectSettings::ReadRuntimeLimits(ProjectFile(
				"storage_max_file_mib=100\nstorage_max_total_mib=10\nstorage_max_entries=0\n"
				"json_max_depth=1000\nmax_frame_delta=5\n").ini);
			CHECK(limits.storage.maxFileSize == 100u * 1024 * 1024);
			CHECK(limits.storage.maxTotalSize == 100u * 1024 * 1024); // never below the largest file
			CHECK(limits.storage.maxEntryCount == 1);
			CHECK(limits.json.maxDepth == 256);
			CHECK(limits.maxFrameDelta == doctest::Approx(1.0f));
			CHECK(logs.Count(ELogLevel::LOG_WARNING) == 4);
		}

		{
			// A total the project does not set grows with the largest file
			const auto limits = ProjectSettings::ReadRuntimeLimits(ProjectFile("storage_max_file_mib=100\n").ini);
			CHECK(limits.storage.maxTotalSize == 100u * 1024 * 1024);
		}
	}
}
