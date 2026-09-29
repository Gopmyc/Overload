/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <utility>
#include <vector>

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
		auto& framebuffer = m_renderer.GetFrameDescriptor().outputBuffer.value();

		std::vector<std::pair<PostProcess::AEffect*, const PostProcess::EffectSettings*>> applicableEffects;

		for (auto& effect : m_effects)
		{
			if (effect)
			{
				auto& effectRef = *effect;
				const auto& settings = stack->Get(typeid(effectRef));

				if (effect->IsApplicable(settings))
				{
					applicableEffects.emplace_back(effect.get(), &settings);
				}
			}
		}

		// The first effect reads the output buffer directly, and the last one writes into it directly:
		// no need to copy the image to the ping-pong buffers, and back to the output buffer.
		// Effects can't read and write the same buffer, so a single effect still needs a final copy.
		baregl::Framebuffer* src = &framebuffer;

		for (size_t i = 0; i < applicableEffects.size(); ++i)
		{
			const auto [effect, settings] = applicableEffects[i];
			const bool isLastEffect = i == applicableEffects.size() - 1;

			baregl::Framebuffer* dst =
				isLastEffect && src != &framebuffer ?
				&framebuffer :
				(src == &m_pingPongBuffers[0] ? &m_pingPongBuffers[1] : &m_pingPongBuffers[0]);

			effect->Draw(p_pso, *src, *dst, *settings);
			src = dst;
		}

		if (src != &framebuffer)
		{
			m_renderer.Blit(p_pso, *src, framebuffer, m_blitMaterial);
		}
	}
}
