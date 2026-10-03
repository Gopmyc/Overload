/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cstdint>
#include <string>

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Rendering/EngineBufferRenderFeature.h>
#include <OvCore/Rendering/FrameBuilder.h>
#include <OvCore/Rendering/ShadowRenderPass.h>
#include <OvCore/ResourceManagement/ShaderManager.h>

#include <OvRendering/Features/LightingRenderFeature.h>
#include <OvRendering/Utils/Profiling.h>

constexpr uint8_t kMaxShadowMaps = 1;
const std::string kShadowPassName = "SHADOW_PASS";

OvCore::Rendering::ShadowRenderPass::ShadowRenderPass(OvRendering::Core::CompositeRenderer& p_renderer) :
	OvRendering::Core::ARenderPass(p_renderer)
{
	const auto shadowShader = OVSERVICE(OvCore::ResourceManagement::ShaderManager).GetResource(":Shaders\\ShadowFallback.ovfx");
	OVASSERT(shadowShader, "Cannot find the shadow shader");

	m_shadowMaterial.SetShader(shadowShader);
	// No need to update the material settings, as its generated state mask will be overridden anyway.
}

void OvCore::Rendering::ShadowRenderPass::Draw(OvRendering::Data::PipelineState p_pso)
{
	ZoneScoped;
	TracyGpuZone("ShadowRenderPass");

	using namespace OvCore::Rendering;

	OVASSERT(m_renderer.HasDescriptor<FrameBuilder::ParsingResult>(), "Cannot find ParsingResult attached to this renderer");
	OVASSERT(m_renderer.HasFeature<OvCore::Rendering::EngineBufferRenderFeature>(), "Cannot find EngineBufferRenderFeature attached to this renderer");
	OVASSERT(m_renderer.HasDescriptor<OvRendering::Features::LightingRenderFeature::LightingDescriptor>(), "Cannot find LightingDescriptor attached to this renderer");

	auto& engineBufferRenderFeature = m_renderer.GetFeature<OvCore::Rendering::EngineBufferRenderFeature>();
	auto& lightingDescriptor = m_renderer.GetDescriptor<OvRendering::Features::LightingRenderFeature::LightingDescriptor>();

	auto& frameDescriptor = m_renderer.GetFrameDescriptor();

	auto pso = m_renderer.CreatePipelineState();

	uint8_t lightIndex = 0;

	for (auto lightReference : lightingDescriptor.lights)
	{
		auto& light = lightReference.get();

		if (light.castShadows)
		{
			if (lightIndex < kMaxShadowMaps)
			{
				if (light.type == OvRendering::Settings::ELightType::DIRECTIONAL)
				{
					light.PrepareForShadowRendering(frameDescriptor);

					engineBufferRenderFeature.SetCamera(light.shadowCamera.value());

					light.shadowBuffer->Bind();
					m_renderer.SetViewport(0, 0, light.shadowMapResolution, light.shadowMapResolution);
					m_renderer.Clear(true, true, true);
					_DrawShadows(pso, light.shadowCamera.value());
					light.shadowBuffer->Unbind();

					engineBufferRenderFeature.SetCamera(frameDescriptor.camera.value());
				}
				else
				{
					// Other light types not supported!
				}
			}
		}
	}

	if (auto output = frameDescriptor.outputBuffer)
	{
		output.value().Bind();
	}

	m_renderer.SetViewport(0, 0, frameDescriptor.renderWidth, frameDescriptor.renderHeight);
}

void OvCore::Rendering::ShadowRenderPass::_DrawShadows(
	OvRendering::Data::PipelineState p_pso,
	const OvRendering::Entities::Camera& p_camera
)
{
	using namespace OvCore::Rendering;

	const auto& parsingResult = m_renderer.GetDescriptor<FrameBuilder::ParsingResult>();

	const auto filteringResult = FrameBuilder::Filter(
		parsingResult,
		FrameBuilder::FilteringInput{
			.camera = p_camera,
			.requiredVisibilityFlags = EVisibilityFlags::SHADOW,
			.filter = [](const OvRendering::Entities::Drawable&, const OvRendering::Data::Material& p_material) {
				return p_material.IsShadowCaster();
			}
		}
	);

	const FrameBuilder::PreparationInput preparationInput{
		.pass = kShadowPassName,
		// If the material has a shadow pass, use it. Otherwise, use the shadow fallback.
		.passFallbackMaterial = m_shadowMaterial,
		// Override the state mask properties to ensure the shadow pass is rendered correctly.
		.customPreparation = [](OvRendering::Entities::Drawable& p_drawable) {
			auto& stateMask = p_drawable.stateMask;
			stateMask.blendable = false; // The shadow pass should never use blending.
			stateMask.depthTest = true; // The shadow pass should always use depth test.
			stateMask.colorWriting = false; // The shadow pass should never write color.
			stateMask.depthWriting = true; // The shadow pass should always write depth.

			// No front/backface culling for shadow pass (aka: two-sided shadow pass).
			// A "two-sided" shadow pass setting could be added in the future to change this behavior.
			stateMask.frontfaceCulling = false;
			stateMask.backfaceCulling = false;
		}
	};

	FrameBuilder::Draw(m_renderer, p_pso, filteringResult, preparationInput);
}
