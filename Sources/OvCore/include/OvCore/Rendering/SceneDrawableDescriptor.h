/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <optional>

#include <OvCore/ECS/Actor.h>
#include <OvCore/Rendering/EVisibilityFlags.h>
#include <OvRendering/Geometry/BoundingSphere.h>

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
	};
}
