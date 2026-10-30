/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>

#include <OvCore/Rendering/EngineDrawableDescriptor.h>

namespace OvCore::Rendering
{
	/**
	* Descriptor attached to a drawable standing for several drawables of the same mesh and material, drawn as one
	* instanced draw (see FrameBuilder). The EngineBufferRenderFeature uploads the matrices of every instance, which
	* shaders declaring the instancing capability read by gl_InstanceID (see Common/Instancing.ovfxh).
	* The instances are only referenced: they must outlive the draw.
	*/
	struct InstancingDrawableDescriptor
	{
		const EngineDrawableDescriptor* const* instances = nullptr;
		uint32_t count = 0;
	};
}
