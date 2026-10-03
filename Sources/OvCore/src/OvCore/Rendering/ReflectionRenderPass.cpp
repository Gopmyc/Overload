/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <OvCore/ECS/Components/CMaterialRenderer.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Rendering/EngineBufferRenderFeature.h>
#include <OvCore/Rendering/EngineDrawableDescriptor.h>
#include <OvCore/Rendering/FrameBuilder.h>
#include <OvCore/Rendering/ReflectionRenderFeature.h>
#include <OvCore/Rendering/ReflectionRenderPass.h>
#include <OvCore/Rendering/SceneRenderer.h>
#include <OvCore/ResourceManagement/ShaderManager.h>

#include <OvRendering/Utils/Profiling.h>

namespace
{
	constexpr uint32_t kProbeFaceCount = 6;
	const OvMaths::FVector3 kCubeFaceRotations[kProbeFaceCount] = {
		{ 0.0f, -90.0f, 180.0f },	// (Right)
		{ 0.0f, 90.0f, 180.0f },	// (Left)
		{ 90.0f, 0.0f, 180.0f },	// (Top)
		{ -90.0f, 0.0f, 180.0f },	// (Bottom)
		{ 0.0f, 0.0f, 180.0f },		// (Front)
		{ 0.0f, -180.0f, 180.0f }	// (Back)
	};
}

OvCore::Rendering::ReflectionRenderPass::ReflectionRenderPass(OvRendering::Core::CompositeRenderer& p_renderer) :
	OvRendering::Core::ARenderPass(p_renderer)
{
}

void OvCore::Rendering::ReflectionRenderPass::Draw(OvRendering::Data::PipelineState p_pso)
{
	ZoneScoped;
	TracyGpuZone("ReflectionRenderPass");

	using namespace OvCore::Rendering;

	auto& sceneDescriptor = m_renderer.GetDescriptor<SceneRenderer::SceneDescriptor>();
	auto& engineBufferRenderFeature = m_renderer.GetFeature<OvCore::Rendering::EngineBufferRenderFeature>();
	auto& reflectionDescriptor = m_renderer.GetDescriptor<OvCore::Rendering::ReflectionRenderFeature::ReflectionDescriptor>();
	auto& frameDescriptor = m_renderer.GetFrameDescriptor();

	for (auto reflectionProbeReference : reflectionDescriptor.reflectionProbes)
	{
		auto& reflectionProbe = reflectionProbeReference.get();

		const auto faceIndices = reflectionProbe._GetCaptureFaceIndices();

		// No faces to render, skip this probe.
		if (faceIndices.empty())
		{
			continue;
		}

		OvRendering::Entities::Camera reflectionCamera;

		reflectionCamera.SetPosition(
			reflectionProbe.owner.transform.GetWorldPosition() +
			reflectionProbe.GetCapturePosition()
		);

		auto& targetFramebuffer = reflectionProbe._GetTargetFramebuffer();

		reflectionCamera.SetFov(90.0f);
		const auto [width, height] = targetFramebuffer.GetSize();
		targetFramebuffer.Bind();
		m_renderer.SetViewport(0, 0, width, height);

		// Iterating over the given face indices, which determine if we 
		// are rendering progressively (less than 6 faces per frame) or immediately (6 faces at once).
		for (auto faceIndex : faceIndices)
		{
			reflectionCamera.SetRotation(OvMaths::FQuaternion{ kCubeFaceRotations[faceIndex] });
			reflectionCamera.CacheMatrices(width, height);
			engineBufferRenderFeature.SetCamera(reflectionCamera);
			targetFramebuffer.SetTargetDrawBuffer(faceIndex);
			m_renderer.Clear(true, true, true);
			_DrawReflections(p_pso, reflectionCamera);

			// If we just drew the last face, we notify the reflection probe that the cubemap is complete.
			if (faceIndex == 5)
			{
				reflectionProbe._NotifyCubemapComplete();
			}
		}

		targetFramebuffer.Unbind();
	}

	// Once we are done rendering all reflection probes,
	// we can restore the initial camera, unbind the framebuffer,
	// and reset the viewport.
	engineBufferRenderFeature.SetCamera(frameDescriptor.camera.value());

	if (auto output = frameDescriptor.outputBuffer)
	{
		output.value().Bind();
	}

	m_renderer.SetViewport(0, 0, frameDescriptor.renderWidth, frameDescriptor.renderHeight);
}

void OvCore::Rendering::ReflectionRenderPass::_DrawReflections(
	OvRendering::Data::PipelineState p_pso,
	const OvRendering::Entities::Camera& p_camera
)
{
	auto& parsingResult = m_renderer.GetDescriptor<FrameBuilder::ParsingResult>();

	const auto filteringResult = FrameBuilder::Filter(
		parsingResult,
		FrameBuilder::FilteringInput{
			.camera = p_camera,
			.frustumOverride = std::nullopt, // No frustum override for reflections
			.overrideMaterial = std::nullopt, // No override material for reflections
			.fallbackMaterial = std::nullopt, // No fallback material for reflections
			.requiredVisibilityFlags = EVisibilityFlags::REFLECTION,
			.includeUI = false, // Exclude UI elements from contribution
			.filter = [](const OvRendering::Entities::Drawable&, const OvRendering::Data::Material& p_material) {
				return p_material.IsCapturedByReflectionProbes();
			}
		}
	);

	FrameBuilder::Draw(m_renderer, p_pso, filteringResult, FrameBuilder::PreparationInput{
		.pass = "REFLECTION_PASS"
	});
}
