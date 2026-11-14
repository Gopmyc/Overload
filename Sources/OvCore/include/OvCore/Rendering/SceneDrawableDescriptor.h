/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <optional>

#include <OvCore/ECS/Actor.h>
#include <OvCore/Rendering/EVisibilityFlags.h>
#include <OvMaths/FVector3.h>
#include <OvRendering/Geometry/BoundingSphere.h>

namespace OvCore::ECS::Components { class CSkinnedMeshRenderer; }

namespace OvCore::Rendering
{
	/**
	* Descriptor attached to drawables parsed from the scene.
	* Contains additional information required to filter them.
	*/
	struct SceneDrawableDescriptor
	{
		OvCore::ECS::Actor& actor;
		EVisibilityFlags visibilityFlags = EVisibilityFlags::NONE;
		std::optional<OvRendering::Geometry::BoundingSphere> bounds;
		bool boundsInWorld = false; // The bounds are in the world, instead of the actor's space
		std::optional<int> drawOrderOverride;
		bool isUserInterface = false;
		const ECS::Components::CSkinnedMeshRenderer* skinnedMeshRenderer = nullptr; // Told when a pass keeps the drawable
		std::optional<OvMaths::FVector3> sortPosition; // Where the drawable is sorted by distance from, instead of its actor's position
	};
}
