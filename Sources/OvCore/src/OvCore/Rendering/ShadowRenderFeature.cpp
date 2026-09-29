/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>

#include <tracy/Tracy.hpp>

#include <OvCore/ECS/Components/CMaterialRenderer.h>
#include <OvCore/Rendering/ShadowRenderFeature.h>
#include <OvDebug/Logger.h>

OvCore::Rendering::ShadowRenderFeature::ShadowRenderFeature(
	OvRendering::Core::CompositeRenderer& p_renderer,
	OvRendering::Features::EFeatureExecutionPolicy p_executionPolicy
) :
	ARenderFeature(p_renderer, p_executionPolicy)
{
}

OvTools::Utils::OptRef<OvRendering::Entities::Light> OvCore::Rendering::ShadowRenderFeature::FindShadowCastingLight(
	const OvRendering::Features::LightingRenderFeature::LightSet& p_lights
)
{
	for (auto lightReference : p_lights)
	{
		auto& light = lightReference.get();

		if (light.castShadows && light.type == OvRendering::Settings::ELightType::DIRECTIONAL)
		{
			return light;
		}
	}

	return std::nullopt;
}

void OvCore::Rendering::ShadowRenderFeature::OnBeginFrame(const OvRendering::Data::FrameDescriptor& p_frameDescriptor)
{
	OVASSERT(m_renderer.HasDescriptor<OvRendering::Features::LightingRenderFeature::LightingDescriptor>(), "Cannot find LightingDescriptor attached to this renderer");

	const auto& lights = m_renderer.GetDescriptor<OvRendering::Features::LightingRenderFeature::LightingDescriptor>().lights;

	if (auto light = FindShadowCastingLight(lights))
	{
		m_shadowCastingLight = light.value();
	}
	else
	{
		m_shadowCastingLight.reset();
	}

	if (!m_multipleShadowCastersWarned)
	{
		const auto shadowCasterCount = std::ranges::count_if(lights, [](const auto& p_light) {
			return p_light.get().castShadows && p_light.get().type == OvRendering::Settings::ELightType::DIRECTIONAL;
		});

		if (shadowCasterCount > 1)
		{
			OVLOG_WARNING("ShadowRenderFeature does not support more than one shadow casting directional light at the moment");
			m_multipleShadowCastersWarned = true;
		}
	}
}

void OvCore::Rendering::ShadowRenderFeature::OnEndFrame()
{
	m_shadowCastingLight.reset();
}

void OvCore::Rendering::ShadowRenderFeature::OnBeforeDraw(OvRendering::Data::PipelineState& p_pso, const OvRendering::Entities::Drawable& p_drawable)
{
	ZoneScoped;

	if (!m_shadowCastingLight || !m_shadowCastingLight->IsSetupForShadowRendering())
	{
		return;
	}

	auto& material = p_drawable.material.value();

	// Skip materials that aren't properly set to receive shadows.
	if (!material.IsShadowReceiver() || !material.HasProperty("_ShadowMap") || !material.HasProperty("_LightSpaceMatrix"))
	{
		return;
	}

	const auto shadowTex = m_shadowCastingLight->shadowBuffer->GetAttachment<baregl::Texture>(
		baregl::types::EFramebufferAttachment::DEPTH
	);

	material.SetProperty("_ShadowMap", &shadowTex.value().get(), true);
	material.SetProperty("_LightSpaceMatrix", m_shadowCastingLight->lightSpaceMatrix.value(), true);
}
