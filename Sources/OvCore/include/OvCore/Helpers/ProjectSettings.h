/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <string>

#include <OvCore/Scripting/JsonLimits.h>
#include <OvPhysics/Settings/PhysicsSettings.h>
#include <OvTools/Filesystem/IniFile.h>
#include <OvTools/Filesystem/SaveStorage.h>

namespace OvCore::Helpers::ProjectSettings
{
	/**
	* The limits a project sets on what its scripts may do
	*/
	struct RuntimeLimits
	{
		OvTools::Filesystem::SaveStorageLimits storage;
		OvCore::Scripting::JsonLimits json;

		/**
		* The longest step a frame may report, in seconds
		*/
		float maxFrameDelta = 0.1f;
	};

	/**
	* Reads the physics settings of a project file. A key the file lacks keeps its default value:
	* - gravity: the vertical gravity (-9.81)
	* - physics_layer_names: comma-separated names, the first for layer 0 (layers past the list keep their names)
	* - physics_ignored_layer_pairs: comma-separated pairs of layers that go through each other, as "a:b", each layer
	*   given by its name or its index (every layer collides with every other)
	* - physics_ragdoll_layer: the layer of the ragdolls' bodies, by name or index (15)
	* - physics_ragdoll_ignores_dynamic_bodies: ragdolls go through dynamic bodies (false)
	* An entry it cannot read is logged and skipped
	* @param p_projectSettings
	*/
	OvPhysics::Settings::PhysicsSettings ReadPhysicsSettings(const OvTools::Filesystem::IniFile& p_projectSettings);

	/**
	* Returns the name that identifies the project's saves: its project_id, or its executable_name when it has none,
	* or "Game"
	* @param p_projectSettings
	*/
	std::string GetSaveName(const OvTools::Filesystem::IniFile& p_projectSettings);

	/**
	* Reads the limits of a project file. A key the file lacks keeps its default value, and a value out of range is
	* logged and brought within it:
	* - storage_max_file_mib: the largest saved file (4, from 1 to 1024)
	* - storage_max_total_mib: the most the save folder holds (64, from storage_max_file_mib to 16384)
	* - storage_max_entries: the most files and folders it holds (1024, from 1 to 1000000)
	* - json_max_mib: the longest text Json encodes or decodes (4, from 1 to 1024)
	* - json_max_depth: the deepest nesting Json accepts (64, from 1 to 256)
	* - max_frame_delta: the longest step a frame reports, in seconds (0.1, from 0.001 to 1)
	* @param p_projectSettings
	*/
	RuntimeLimits ReadRuntimeLimits(const OvTools::Filesystem::IniFile& p_projectSettings);
}
