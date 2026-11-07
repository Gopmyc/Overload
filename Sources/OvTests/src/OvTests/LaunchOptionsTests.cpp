/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <doctest/doctest.h>

#include <OvGame/Core/LaunchOptions.h>
#include <OvTests/TestUtils.h>

using OvGame::Core::LaunchOptions;

TEST_SUITE("OvGame command line")
{
	TEST_CASE("no option runs the built game")
	{
		const auto result = LaunchOptions::Parse({});
		REQUIRE(result.options);
		CHECK_FALSE(result.help);
		CHECK_FALSE(result.options->projectFile);
		CHECK_FALSE(result.options->engineAssets);
		CHECK_FALSE(result.options->scene);
		CHECK_FALSE(result.options->frames);
		CHECK_FALSE(result.options->screenshot);
		CHECK_FALSE(result.options->hiddenWindow);
		CHECK_FALSE(result.options->failOnError);
		CHECK_FALSE(result.options->dumpLuaApi);
	}

	TEST_CASE("help stops the parsing")
	{
		CHECK(LaunchOptions::Parse({ "--help" }).help);
		CHECK(LaunchOptions::Parse({ "--frames", "5", "-h", "--unknown" }).help);
		CHECK(LaunchOptions::GetUsage().find("--project") != std::string::npos);
	}

	TEST_CASE("every option is read")
	{
		OvTests::TempFolder folder;
		const auto projectFile = folder.WriteFile("Game/Game.ovproject", "start_scene=Scenes/A.ovscene\n");
		std::filesystem::create_directories(folder.GetPath() / "Engine");

		const auto result = LaunchOptions::Parse({
			"--project", (folder.GetPath() / "Game").string(),
			"--engine", (folder.GetPath() / "Engine").string(),
			"--scene", "Scenes/B.ovscene",
			"--frames", "120",
			"--screenshot", (folder.GetPath() / "shot.png").string(),
			"--hidden-window",
			"--fail-on-error",
			"--dump-lua-api"
		});

		REQUIRE_MESSAGE(result.options, result.error);
		CHECK(std::filesystem::equivalent(*result.options->projectFile, projectFile));
		CHECK(std::filesystem::equivalent(*result.options->engineAssets, folder.GetPath() / "Engine"));
		CHECK(*result.options->scene == "Scenes/B.ovscene");
		CHECK(*result.options->frames == 120);
		CHECK(result.options->screenshot->filename() == "shot.png");
		CHECK(result.options->hiddenWindow);
		CHECK(result.options->failOnError);
		CHECK(result.options->dumpLuaApi);
	}

	TEST_CASE("a project is a .ovproject file or the folder that holds exactly one")
	{
		OvTests::TempFolder folder;
		const auto file = folder.WriteFile("One/One.ovproject", "");
		folder.WriteFile("Two/A.ovproject", "");
		folder.WriteFile("Two/B.ovproject", "");
		folder.WriteFile("None/readme.txt", "");

		const auto byFile = LaunchOptions::Parse({ "--project", file.string() });
		REQUIRE(byFile.options);
		CHECK(std::filesystem::equivalent(*byFile.options->projectFile, file));

		CHECK(LaunchOptions::Parse({ "--project", (folder.GetPath() / "Two").string() }).error.find("several") != std::string::npos);
		CHECK(LaunchOptions::Parse({ "--project", (folder.GetPath() / "None").string() }).error.find("no .ovproject") != std::string::npos);
		CHECK(LaunchOptions::Parse({ "--project", (folder.GetPath() / "None/readme.txt").string() }).error.find("not a .ovproject") != std::string::npos);
		CHECK(LaunchOptions::Parse({ "--project", (folder.GetPath() / "Missing").string() }).error.find("no file or folder") != std::string::npos);
	}

	TEST_CASE("bad options are refused with a message")
	{
		const auto check = [](const std::vector<std::string>& p_arguments, std::string_view p_expected)
		{
			const auto result = LaunchOptions::Parse(p_arguments);
			CHECK_FALSE(result.options);
			CHECK_FALSE(result.help);
			CHECK_MESSAGE(result.error.find(p_expected) != std::string::npos, result.error);
		};

		check({ "--bogus" }, "unknown option --bogus");
		check({ "game.ovproject" }, "unknown option");
		check({ "--frames" }, "needs a value");
		check({ "--frames", "--hidden-window" }, "needs a value");
		check({ "--frames", "0" }, "above 0");
		check({ "--frames", "-3" }, "above 0");
		check({ "--frames", "12x" }, "above 0");
		check({ "--frames", "99999999999" }, "above 0");
		check({ "--screenshot", "a.png" }, "needs --frames");
		check({ "--engine", "/no/such/folder/anywhere" }, "--engine: no folder");
	}
}
