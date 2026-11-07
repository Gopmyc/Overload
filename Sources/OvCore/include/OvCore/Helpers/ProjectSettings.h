/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <OvPhysics/Settings/PhysicsSettings.h>
#include <OvTools/Filesystem/IniFile.h>

namespace OvCore::Helpers::ProjectSettings
{
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
}
