/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Rendering/PostProcess/TonemappingEffect.h>
#include <OvCore/ResourceManagement/ShaderManager.h>
#include <OvRendering/Utils/Profiling.h>

OvCore::Rendering::PostProcess::TonemappingEffect::TonemappingEffect(OvRendering::Core::CompositeRenderer& p_renderer) : AEffect(p_renderer)
{
	m_tonemappingMaterial.SetShader(OVSERVICE(OvCore::ResourceManagement::ShaderManager)[":Shaders\\PostProcess\\Tonemapping.ovfx"]);
}

void OvCore::Rendering::PostProcess::TonemappingEffect::Draw(
	OvRendering::Data::PipelineState p_pso,
	baregl::Framebuffer& p_src,
	baregl::Framebuffer& p_dst,
	const EffectSettings& p_settings
)
{
	ZoneScoped;
	TracyGpuZone("TonemappingEffect");

	const auto& tonemappingSettings = static_cast<const TonemappingSettings&>(p_settings);

	// Tonemapping
	m_tonemappingMaterial.SetProperty("_Exposure", tonemappingSettings.exposure, true);
	m_tonemappingMaterial.SetProperty("_Mode", static_cast<int>(tonemappingSettings.mode), true);
	m_tonemappingMaterial.SetProperty("_GammaCorrection", static_cast<int>(tonemappingSettings.gammaCorrection), true);

	// Colour grading and vignette
	m_tonemappingMaterial.SetProperty("_Temperature", tonemappingSettings.temperature, true);
	m_tonemappingMaterial.SetProperty("_Tint", tonemappingSettings.tint, true);
	m_tonemappingMaterial.SetProperty("_Saturation", tonemappingSettings.saturation, true);
	m_tonemappingMaterial.SetProperty("_Contrast", tonemappingSettings.contrast, true);
	m_tonemappingMaterial.SetProperty("_VignetteIntensity", tonemappingSettings.vignetteIntensity, true);
	m_tonemappingMaterial.SetProperty("_VignetteSmoothness", tonemappingSettings.vignetteSmoothness, true);
	m_renderer.Blit(p_pso, p_src, p_dst, m_tonemappingMaterial);
}
