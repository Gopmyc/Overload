/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <tracy/Tracy.hpp>

#include <OvCore/ECS/Components/CParticleSystem.h>
#include <OvCore/Rendering/ParticleDrawableDescriptor.h>
#include <OvCore/Rendering/ParticleRenderFeature.h>

OvCore::Rendering::ParticleRenderFeature::ParticleRenderFeature(
	OvRendering::Core::CompositeRenderer& p_renderer,
	OvRendering::Features::EFeatureExecutionPolicy p_executionPolicy,
	uint32_t p_bufferBindingPoint
) :
	ARenderFeature(p_renderer, p_executionPolicy),
	m_bufferBindingPoint(p_bufferBindingPoint)
{
}

void OvCore::Rendering::ParticleRenderFeature::OnBeforeDraw(OvRendering::Data::PipelineState& p_pso, const OvRendering::Entities::Drawable& p_drawable)
{
	(void)p_pso;

	OvTools::Utils::OptRef<const ParticleDrawableDescriptor> descriptor;

	if (p_drawable.TryGetDescriptor<ParticleDrawableDescriptor>(descriptor) && descriptor->system)
	{
		ZoneScoped;

		// Uploads what changed since the last draw of the system: once a frame, whatever the views drawing it
		descriptor->system->BindBuffer(m_bufferBindingPoint);
	}
}
