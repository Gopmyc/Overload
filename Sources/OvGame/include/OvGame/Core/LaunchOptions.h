/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace OvGame::Core
{
	/**
	* What the command line asks of a run. Without any option, the game runs as a built game: its project in
	* Data/User, the engine's resources in Data/Engine, until it quits
	*/
	struct LaunchOptions
	{
		/**
		* The project file (.ovproject) to run, its assets in the Assets folder next to it
		*/
		std::optional<std::filesystem::path> projectFile;

		/**
		* The engine's resources (an Overload checkout's Resources/Engine)
		*/
		std::optional<std::filesystem::path> engineAssets;

		/**
		* The scene to start on, relative to the project's assets, instead of the project's start_scene
		*/
		std::optional<std::string> scene;

		/**
		* The number of frames to run before quitting
		*/
		std::optional<uint32_t> frames;

		/**
		* Where to write the last frame, as a PNG image (with frames only)
		*/
		std::optional<std::filesystem::path> screenshot;

		bool hiddenWindow = false;

		/**
		* Whether the run ends with exit code 2 when an error was logged
		*/
		bool failOnError = false;

		struct ParseResult;

		/**
		* Reads the arguments that follow the program's name
		* @param p_arguments
		*/
		static ParseResult Parse(const std::vector<std::string>& p_arguments);

		/**
		* The options, as --help prints them
		*/
		static std::string GetUsage();
	};

	/**
	* The outcome of reading a command line: options, or an error message, or a request for the usage
	*/
	struct LaunchOptions::ParseResult
	{
		std::optional<LaunchOptions> options;
		std::string error;
		bool help = false;
	};
}
