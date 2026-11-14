project "soloud"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	targetdir (outputdir .. "%{cfg.buildcfg}/%{prj.name}")
	objdir (objoutdir .. "%{cfg.buildcfg}/%{prj.name}")
	warnings "Off"

	files {
		"**.h",
		"**.cpp",
		"**.c",
		"**.lua"
	}

	includedirs {
		"include"
	}

	defines {
		"WITH_MINIAUDIO",
		"WITH_NULL" -- Mixed by AudioEngine when there is no audio device, and in the tests
	}

	filter { "configurations:Debug" }
		defines { "DEBUG", "_DEBUG" }
		symbols "On"

	filter { "configurations:Release or configurations:Publish" }
		defines { "NDEBUG" }
		optimize "On"
