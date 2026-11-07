/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <OvCore/Scripting/Lua/LuaApi.h>
#include <OvDebug/Logger.h>
#include <OvGame/Core/Application.h>
#include <OvGame/Core/LaunchOptions.h>
#include <OvRendering/Utils/Defines.h>
#include <OvTools/Profiling/TracyAllocators.h>

FORCE_DEDICATED_GPU

namespace
{
	int RunGame(int p_argc, char** p_argv)
	{
		std::vector<std::string> arguments;
		for (int index = 1; index < p_argc; ++index)
			arguments.emplace_back(p_argv[index]);

		const auto result = OvGame::Core::LaunchOptions::Parse(arguments);

		if (result.help)
		{
			std::cout << OvGame::Core::LaunchOptions::GetUsage();
			return EXIT_SUCCESS;
		}

		if (!result.options)
		{
			std::cerr << "OvGame: " << result.error << "\n\n" << OvGame::Core::LaunchOptions::GetUsage();
			return EXIT_FAILURE;
		}

		if (result.options->dumpLuaApi)
		{
			for (const auto& name : OvCore::Scripting::LuaApi::Describe())
				std::cout << name << '\n';
			return EXIT_SUCCESS;
		}

		// Errors are counted over the whole run, from the start scene's loading to the game's destruction
		std::atomic<uint32_t> errorCount = 0;
		const auto logListener = OvDebug::Logger::LogEvent += [&errorCount](const OvDebug::LogData& p_log)
		{
			if (p_log.logLevel == OvDebug::ELogLevel::LOG_ERROR)
				++errorCount;
		};

		int exitCode = EXIT_SUCCESS;
		{
			OvGame::Core::Application app(*result.options);
			exitCode = app.Run();
		}

		OvDebug::Logger::LogEvent -= logListener;

		if (exitCode == EXIT_SUCCESS && result.options->failOnError && errorCount > 0)
			exitCode = 2;

		return exitCode;
	}
}

#ifdef _DEBUG
int main(int argc, char** argv)
{
	return RunGame(argc, argv);
}
#else
	#ifdef _WIN32
		#undef APIENTRY
		#include "Windows.h"
		INT WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
		{
			// A windowed build has no console: started from a terminal with options, it writes to that terminal
			if (__argc > 1 && AttachConsole(ATTACH_PARENT_PROCESS))
			{
				FILE* stream = nullptr;
				freopen_s(&stream, "CONOUT$", "w", stdout);
				freopen_s(&stream, "CONOUT$", "w", stderr);
			}

			return RunGame(__argc, __argv);
		}
	#else
		int main(int argc, char** argv)
		{
			return RunGame(argc, argv);
		}
	#endif
#endif
