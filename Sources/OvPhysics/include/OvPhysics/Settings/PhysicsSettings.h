/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <OvMaths/FVector3.h>

namespace OvPhysics::Settings
{
	/**
	* Data structure to give to the PhysicsEngine constructor to setup its settings
	*/
	struct PhysicsSettings
	{
		/**
		* The number of collision layers a physical object can be on (0 to kLayerCount - 1)
		*/
		static constexpr uint16_t kLayerCount = 16;

		OvMaths::FVector3 gravity = { 0.0f, -9.81f, 0.f };

		/**
		* The name of each layer, empty when it has none
		*/
		std::array<std::string, kLayerCount> layerNames = { "Default", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "Ragdoll" };

		/**
		* For each layer, the layers it collides with: bit j of collisionMasks[i]. Two layers collide only if each
		* names the other
		*/
		std::array<uint16_t, kLayerCount> collisionMasks = MakeFullMasks();

		/**
		* The layer of the ragdolls' bodies
		*/
		uint16_t ragdollLayer = kLayerCount - 1;

		/**
		* Whether ragdolls go through dynamic bodies (they still fall on static and kinematic ones), so that a falling
		* body never pushes nor trips a living one
		*/
		bool ragdollIgnoresDynamicBodies = false;

		static constexpr std::array<uint16_t, kLayerCount> MakeFullMasks()
		{
			std::array<uint16_t, kLayerCount> masks{};
			masks.fill(0xFFFF);
			return masks;
		}
	};
}
