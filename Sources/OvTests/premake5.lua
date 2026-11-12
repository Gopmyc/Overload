project "OvTests"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	targetdir (outputdir .. "%{cfg.buildcfg}/%{prj.name}")
	objdir (objoutdir .. "%{cfg.buildcfg}/%{prj.name}")
	debugdir (outputdir .. "%{cfg.buildcfg}/%{prj.name}")
	fatalwarnings { "All" }

	-- If MSVC, set big obj flag
	filter { "toolset:msc" }
		buildoptions { "/bigobj" }
	filter {}

	files {
		"**.h",
		"**.cpp",
		"**.lua",

		-- The game's code that has no window to need: its command line and its image writer
		"%{wks.location}/Sources/OvGame/src/OvGame/Core/LaunchOptions.cpp",
		"%{wks.location}/Sources/OvGame/src/OvGame/Utils/PngWriter.cpp",
	}

	includedirs {
		-- Dependencies
		dependdir .. "baregl/include",
		dependdir .. "doctest/include",
		dependdir .. "ImGui/include",
		dependdir .. "lua/include",
		dependdir .. "sol/include",
		dependdir .. "tinyxml2/include",
		dependdir .. "tracy",

		-- Overload SDK
		"%{wks.location}/Sources/OvAudio/include",
		"%{wks.location}/Sources/OvCore/include",
		"%{wks.location}/Sources/OvDebug/include",
		"%{wks.location}/Sources/OvGame/include",
		"%{wks.location}/Sources/OvMaths/include",
		"%{wks.location}/Sources/OvPhysics/include",
		"%{wks.location}/Sources/OvRendering/include",
		"%{wks.location}/Sources/OvTools/include",
		"%{wks.location}/Sources/OvUI/include",
		"%{wks.location}/Sources/OvWindowing/include",

		-- Current project
		"include"
	}

	links {
		-- Dependencies
		"assimp",
		"baregl",
		"freetype",
		"glfw",
		"ImGui",
		"jolt",
		"lua",
		"recastnavigation",
		"soloud",
		"tinyxml2",
		"tracy",

		-- Overload SDK
		"OvAudio",
		"OvCore",
		"OvDebug",
		"OvMaths",
		"OvPhysics",
		"OvRendering",
		"OvTools",
		"OvUI",
		"OvWindowing"
	}

	filter { "configurations:Debug" }
		defines { "DEBUG", "_DEBUG" }
		symbols "On"

	filter { "configurations:Release or configurations:Publish" }
		defines { "NDEBUG" }
		optimize "Speed"

	filter { "system:windows" }
		-- forces post-build commands to trigger even if nothing changed
		fastuptodate "Off"

		links {
			-- Precompiled Libraries
			"dbghelp.lib",
			"opengl32.lib",
		}

		postbuildcommands {
			"for /f \"delims=|\" %%i in ('dir /B /S \"%{dependdir}\\*.dll\"') do xcopy /Q /Y \"%%i\" \"%{outputdir}%{cfg.buildcfg}\\%{prj.name}\"",
			"EXIT /B 0"
		}

	filter { "system:linux" }
		-- doctest includes <ciso646> under clang, which recent libstdc++ releases deprecate with a #warning in C++20 (-Wcpp is clang's name for #warning without a "#", which a Makefile would take for a comment)
		buildoptions { "-Wno-cpp" }

		links {
			"dl",
			"pthread",
			"GL",
			"X11",
		}

		-- Force inclusion of all symbols from these libraries
		linkoptions {
			"-Wl,--whole-archive",
			outputdir .. "%{cfg.buildcfg}/baregl/libbaregl.a",
			outputdir .. "%{cfg.buildcfg}/ImGui/libImGui.a",
			outputdir .. "%{cfg.buildcfg}/jolt/libjolt.a",
			outputdir .. "%{cfg.buildcfg}/lua/liblua.a",
			outputdir .. "%{cfg.buildcfg}/recastnavigation/librecastnavigation.a",
			outputdir .. "%{cfg.buildcfg}/soloud/libsoloud.a",
			outputdir .. "%{cfg.buildcfg}/OvAudio/libOvAudio.a",
			outputdir .. "%{cfg.buildcfg}/assimp/libassimp.a",
			outputdir .. "%{cfg.buildcfg}/tinyxml2/libtinyxml2.a",
			"-Wl,--no-whole-archive",
			"-Wl,--allow-multiple-definition",  -- Tracy has some duplicate symbols
		}
