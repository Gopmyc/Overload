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
	* The matrices of one instance as the GPU reads them (std430 layout, see InstancesSSBO.ovfxh): the model matrix
	* transposed, the normal matrix worked out from it, and the user matrix
	*/
	struct PreparedInstance
	{
		OvMaths::FMatrix4 model;
		OvMaths::FMatrix4 normalMatrix;
		OvMaths::FMatrix4 userMatrix;
	};

	/**
	* Returns an instance as the GPU reads it, for the drawables that prepare their instances once rather than at
	* every draw
	* @param p_modelMatrix
	* @param p_userMatrix
	*/
	PreparedInstance PrepareInstance(const OvMaths::FMatrix4& p_modelMatrix, const OvMaths::FMatrix4& p_userMatrix);

	/**
	* Descriptor attached to a drawable standing for several drawables of the same mesh and material, drawn as one
	* instanced draw (see FrameBuilder). The EngineBufferRenderFeature uploads the matrices of every instance, which
	* shaders declaring the instancing capability read by gl_InstanceID (see Common/Instancing.ovfxh).
	* The instances are only referenced: they must outlive the draw.
	* A drawable can give its instances already prepared (prepared, count) instead of their descriptors, as a vegetation
	* layer does for thousands of instances that seldom move.
	*/
	struct InstancingDrawableDescriptor
	{
		const EngineDrawableDescriptor* const* instances = nullptr;
		uint32_t count = 0;
		const PreparedInstance* prepared = nullptr;
	};
}
