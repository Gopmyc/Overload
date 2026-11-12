/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

namespace OvCore::ECS::Components { class CParticleSystem; }

namespace OvCore::Rendering
{
	/**
	* Descriptor attached to the drawables of a particle system: one instance per particle, which the shader places
	* from the system's buffer (see ParticleRenderFeature)
	*/
	struct ParticleDrawableDescriptor
	{
		OvCore::ECS::Components::CParticleSystem* system = nullptr;
	};
}
