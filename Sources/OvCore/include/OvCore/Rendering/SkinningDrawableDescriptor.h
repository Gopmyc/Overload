/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>

#include <OvMaths/FMatrix4.h>

namespace OvCore::ECS::Components { class CSkinnedMeshRenderer; }

namespace OvCore::Rendering
{
	/**
	* Descriptor attached to drawables that require skinning.
	*/
	struct SkinningDrawableDescriptor
	{
		const OvMaths::FMatrix4* matrices = nullptr;
		uint32_t count = 0;
		uint64_t poseVersion = 0;
		float boundsScale = 1.0f;
		const ECS::Components::CSkinnedMeshRenderer* renderer = nullptr; // Told when a pass keeps the drawable
	};
}
