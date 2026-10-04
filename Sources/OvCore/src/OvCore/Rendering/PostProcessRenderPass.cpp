/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <tracy/Tracy.hpp>

#include <OvCore/ECS/Components/CPostProcessStack.h>
#include <OvCore/Rendering/PostProcessRenderPass.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Rendering/FramebufferUtil.h>
#include <OvCore/Rendering/SceneRenderer.h>
#include <OvCore/ResourceManagement/ShaderManager.h>

#include <OvRendering/Core/CompositeRenderer.h>
#include <OvRendering/Utils/Profiling.h>

OvCore::Rendering::PostProcessRenderPass::PostProcessRenderPass(OvRendering::Core::CompositeRenderer& p_renderer) :
	OvRendering::Core::ARenderPass(p_renderer),
	m_pingPongBuffers{ "PostProcessBlit" }
{
	for (auto& buffer : m_pingPongBuffers.GetFramebuffers())
	{
		OvCore::Rendering::FramebufferUtil::SetupFramebuffer(
			buffer, 1, 1, false, false, false
		);
	}

	m_blitMaterial.SetShader(OVSERVICE(OvCore::ResourceManagement::ShaderManager)[":Shaders\\PostProcess\\Blit.ovfx"]);

	// Instantiate available effects
	m_effects.reserve(4);
	m_effects.push_back(std::make_unique<OvCore::Rendering::PostProcess::AutoExposureEffect>(p_renderer));
	m_effects.push_back(std::make_unique<OvCore::Rendering::PostProcess::BloomEffect>(p_renderer));
	m_effects.push_back(std::make_unique<OvCore::Rendering::PostProcess::TonemappingEffect>(p_renderer));
	m_effects.push_back(std::make_unique<OvCore::Rendering::PostProcess::FXAAEffect>(p_renderer));
}

OvTools::Utils::OptRef<const OvCore::Rendering::PostProcess::PostProcessStack> FindPostProcessStack(OvCore::SceneSystem::Scene& p_scene)
{
	auto& postProcessStacks = p_scene.GetFastAccessComponents().postProcessStacks;

	for (auto postProcessStack : postProcessStacks)
	{
		if (postProcessStack && postProcessStack->owner.IsActive())
		{
			return postProcessStack->GetStack();
		}
	}

	return std::nullopt;
}

void OvCore::Rendering::PostProcessRenderPass::Draw(OvRendering::Data::PipelineState p_pso)
{
	ZoneScoped;
	TracyGpuZone("PostProcessRenderPass");

	auto& sceneDescriptor = m_renderer.GetDescriptor<OvCore::Rendering::SceneRenderer::SceneDescriptor>();
	auto& scene = sceneDescriptor.scene;

	if (auto stack = FindPostProcessStack(scene))
	{
		auto& output = m_renderer.GetFrameDescriptor().outputBuffer.value();

		m_applicableEffects.clear();

		for (auto& effect : m_effects)
		{
			if (effect)
			{
				auto& effectRef = *effect;
				const auto& effectType = typeid(effectRef);
				const auto& settings = stack->Get(effectType);

				if (effect->IsApplicable(settings))
				{
					m_applicableEffects.emplace_back(effect.get(), &settings);
				}
			}
		}

		// The first effect reads directly from the output framebuffer, and the last one writes directly into it.
		// This avoids copying the whole image into the ping-pong buffers and back (2 full-screen passes).
		baregl::Framebuffer* source = &output;

		for (size_t i = 0; i < m_applicableEffects.size(); ++i)
		{
			const auto [effect, settings] = m_applicableEffects[i];
			const bool isLastEffect = i == m_applicableEffects.size() - 1;

			// An effect cannot write into the framebuffer it reads from (feedback loop), which
			// happens when a single effect is applied. In that case, the result is copied back below.
			const bool writeToOutput = isLastEffect && source != &output;
			auto& destination = writeToOutput ? output : m_pingPongBuffers[0];

			effect->Draw(p_pso, *source, destination, *settings);

			source = &destination;

			if (!writeToOutput)
			{
				++m_pingPongBuffers;
			}
		}

		if (source != &output)
		{
			m_renderer.Blit(p_pso, *source, output, m_blitMaterial);
		}
	}
}
