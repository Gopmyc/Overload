project "recastnavigation"
	kind "StaticLib"
	language "C++"
	cppdialect "C++20"
	targetdir (outputdir .. "%{cfg.buildcfg}/%{prj.name}")
	objdir (objoutdir .. "%{cfg.buildcfg}/%{prj.name}")
	warnings "Off"

	files {
		"Recast/**.h",
		"Recast/**.cpp",
		"Detour/**.h",
		"Detour/**.cpp",
		"**.lua"
	}

	includedirs {
		"Recast/Include",
		"Detour/Include"
	}

	filter { "configurations:Debug" }
		defines { "DEBUG", "_DEBUG" }
		symbols "On"

	filter { "configurations:Release or configurations:Publish" }
		defines { "NDEBUG" }
		optimize "On"
