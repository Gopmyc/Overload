/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <ranges>
#include <tracy/Tracy.hpp>

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Rendering/EngineBufferRenderFeature.h>
#include <OvCore/Rendering/EngineDrawableDescriptor.h>
#include <OvCore/Rendering/FrameBuilder.h>
#include <OvCore/Rendering/PostProcessRenderPass.h>
#include <OvCore/Rendering/ReflectionRenderFeature.h>
#include <OvCore/Rendering/ReflectionRenderPass.h>
#include <OvCore/Rendering/SceneRenderer.h>
#include <OvCore/Rendering/ShadowRenderFeature.h>
#include <OvCore/Rendering/ShadowRenderPass.h>
#include <OvCore/Rendering/SkinningRenderFeature.h>
#include <OvCore/ResourceManagement/ShaderManager.h>
#include <OvRendering/Data/Frustum.h>
#include <OvRendering/Features/LightingRenderFeature.h>
#include <OvRendering/Resources/Loaders/ShaderLoader.h>
#include <OvRendering/Utils/Profiling.h>

namespace
{
	using namespace OvCore::Rendering;

	class SceneRenderPass : public OvRendering::Core::ARenderPass
	{
	public:
		SceneRenderPass(OvRendering::Core::CompositeRenderer& p_renderer, bool stencilWrite = false) :
			OvRendering::Core::ARenderPass(p_renderer),
			m_stencilWrite(stencilWrite)
		{
		}

	protected:
		void PrepareStencilBuffer(OvRendering::Data::PipelineState& p_pso)
		{
			p_pso.stencilTest = true;
			p_pso.stencilWriteMask = 0xFF;
			p_pso.stencilFuncRef = 1;
			p_pso.stencilFuncMask = 0xFF;
			p_pso.stencilOpFail = baregl::types::EOperation::REPLACE;
			p_pso.depthOpFail = baregl::types::EOperation::REPLACE;
			p_pso.bothOpFail = baregl::types::EOperation::REPLACE;
			p_pso.colorWriting.mask = 0x00;
		}

	private:
		bool m_stencilWrite;
	};

	/**
	* Opaque drawables written to the depth buffer by the depth pre-pass
	*/
	bool IsDepthPrePassCandidate(const OvRendering::Entities::Drawable& p_drawable)
	{
		return p_drawable.stateMask.depthTest && p_drawable.stateMask.depthWriting;
	}

	/**
	* Optional pass (see Camera::SetDepthPrePass) writing the depth of opaque drawables before they get shaded,
	* so that the opaque pass only shades the visible fragments (no overdraw of expensive fragment shaders).
	* The same shader programs are used in both passes, so the depth values match exactly.
	*/
	class DepthPrePassRenderPass : public OvRendering::Core::ARenderPass
	{
	public:
		DepthPrePassRenderPass(OvRendering::Core::CompositeRenderer& p_renderer) :
			OvRendering::Core::ARenderPass(p_renderer)
		{
		}

	protected:
		virtual void Draw(OvRendering::Data::PipelineState p_pso) override
		{
			ZoneScoped;
			TracyGpuZone("DepthPrePassRenderPass");

			if (!m_renderer.GetFrameDescriptor().camera->HasDepthPrePass())
			{
				return;
			}

			auto& engineBufferRenderFeature = m_renderer.GetFeature<EngineBufferRenderFeature>();
			const auto& filteringResult = m_renderer.GetDescriptor<FrameBuilder::FilteringResult>();

			// Lets shaders skip their shading once alpha testing is done (see ubo_DepthOnly)
			engineBufferRenderFeature.SetDepthOnly(true);

			for (const auto& filteredDrawable : filteringResult.opaques | std::views::values)
			{
				auto drawable = FrameBuilder::Prepare(filteredDrawable, {});

				if (IsDepthPrePassCandidate(drawable))
				{
					drawable.stateMask.colorWriting = false;
					m_renderer.DrawEntity(p_pso, drawable);
				}
			}

			engineBufferRenderFeature.SetDepthOnly(false);
		}
	};

	class OpaqueRenderPass : public SceneRenderPass
	{
	public:
		OpaqueRenderPass(OvRendering::Core::CompositeRenderer& p_renderer, bool p_stencilWrite = false) :
			SceneRenderPass(p_renderer, p_stencilWrite)
		{
		}

	protected:
		virtual void Draw(OvRendering::Data::PipelineState p_pso) override
		{
			ZoneScoped;
			TracyGpuZone("OpaqueRenderPass");

			PrepareStencilBuffer(p_pso);

			const auto& filteringResult = m_renderer.GetDescriptor<FrameBuilder::FilteringResult>();

			if (!m_renderer.GetFrameDescriptor().camera->HasDepthPrePass())
			{
				FrameBuilder::Draw(m_renderer, p_pso, filteringResult.opaques, {});
				return;
			}

			// Drawables already written by the depth pre-pass only need to shade the fragments matching
			// the depth buffer: no depth writes, so fragments shaders with discard don't prevent early depth testing.
			auto prePassedPso = p_pso;
			prePassedPso.depthFunc = baregl::types::EComparaisonAlgorithm::LESS_EQUAL;

			for (const auto& filteredDrawable : filteringResult.opaques | std::views::values)
			{
				auto drawable = FrameBuilder::Prepare(filteredDrawable, {});

				if (IsDepthPrePassCandidate(drawable))
				{
					drawable.stateMask.depthWriting = false;
					m_renderer.DrawEntity(prePassedPso, drawable);
				}
				else
				{
					m_renderer.DrawEntity(p_pso, drawable);
				}
			}
		}
	};

	class TransparentRenderPass : public SceneRenderPass
	{
	public:
		TransparentRenderPass(OvRendering::Core::CompositeRenderer& p_renderer, bool p_stencilWrite = false) :
			SceneRenderPass(p_renderer, p_stencilWrite) {
		}

	protected:
		virtual void Draw(OvRendering::Data::PipelineState p_pso) override
		{
			ZoneScoped;
			TracyGpuZone("TransparentRenderPass");

			PrepareStencilBuffer(p_pso);

			const auto& filteringResult = m_renderer.GetDescriptor<FrameBuilder::FilteringResult>();

			FrameBuilder::Draw(m_renderer, p_pso, filteringResult.transparents, {});
		}
	};

	class UIRenderPass : public SceneRenderPass
	{
	public:
		UIRenderPass(OvRendering::Core::CompositeRenderer& p_renderer, bool p_stencilWrite = false) :
			SceneRenderPass(p_renderer, p_stencilWrite) {
		}

	protected:
		virtual void Draw(OvRendering::Data::PipelineState p_pso) override
		{
			ZoneScoped;
			TracyGpuZone("UIRenderPass");

			PrepareStencilBuffer(p_pso);

			const auto& filteringResult = m_renderer.GetDescriptor<FrameBuilder::FilteringResult>();

			FrameBuilder::Draw(m_renderer, p_pso, filteringResult.ui, {});
		}
	};

	OvRendering::Features::LightingRenderFeature::LightSet FindActiveLights(const OvCore::SceneSystem::Scene& p_scene)
	{
		OvRendering::Features::LightingRenderFeature::LightSet lights;

		const auto& facs = p_scene.GetFastAccessComponents();

		for (auto light : facs.lights)
		{
			if (light->owner.IsActive())
			{
				lights.push_back(std::ref(light->GetData()));
			}
		}

		return lights;
	}

	std::vector<std::reference_wrapper<OvCore::ECS::Components::CReflectionProbe>> FindActiveReflectionProbes(const OvCore::SceneSystem::Scene& p_scene)
	{
		std::vector<std::reference_wrapper<OvCore::ECS::Components::CReflectionProbe>> probes;
		const auto& facs = p_scene.GetFastAccessComponents();
		for (auto probe : facs.reflectionProbes)
		{
			if (probe->owner.IsActive())
			{
				probes.push_back(*probe);
			}
		}
		return probes;
	}
}

OvCore::Rendering::SceneRenderer::SceneRenderer(OvRendering::Context::Driver& p_driver, bool p_stencilWrite)
	: OvRendering::Core::CompositeRenderer(p_driver)
{
	using namespace OvRendering::Features;
	using namespace OvRendering::Settings;
	using enum OvRendering::Features::EFeatureExecutionPolicy;

	AddFeature<EngineBufferRenderFeature, ALWAYS>();
	AddFeature<LightingRenderFeature, ALWAYS>();
	AddFeature<SkinningRenderFeature, ALWAYS>();

	AddFeature<ReflectionRenderFeature, WHITELIST_ONLY>()
		.Include<OpaqueRenderPass>()
		.Include<TransparentRenderPass>();

	AddFeature<ShadowRenderFeature, WHITELIST_ONLY>()
		.Include<OpaqueRenderPass>()
		.Include<TransparentRenderPass>()
		.Include<UIRenderPass>();

	AddPass<ShadowRenderPass>("Shadows", ERenderPassOrder::Shadows);
	AddPass<ReflectionRenderPass>("ReflectionRenderPass", ERenderPassOrder::Reflections);
	AddPass<DepthPrePassRenderPass>("DepthPrePass", ERenderPassOrder::Opaque - 1);
	AddPass<OpaqueRenderPass>("Opaques", ERenderPassOrder::Opaque, p_stencilWrite);
	AddPass<TransparentRenderPass>("Transparents", ERenderPassOrder::Transparent, p_stencilWrite);
	AddPass<PostProcessRenderPass>("Post-Process", ERenderPassOrder::PostProcessing);
	AddPass<UIRenderPass>("UI", ERenderPassOrder::UI);
}

void OvCore::Rendering::SceneRenderer::BeginFrame(const OvRendering::Data::FrameDescriptor& p_frameDescriptor)
{
	ZoneScoped;

	OVASSERT(HasDescriptor<SceneDescriptor>(), "Cannot find SceneDescriptor attached to this renderer");

	auto& sceneDescriptor = GetDescriptor<SceneDescriptor>();

	const bool frustumLightCulling = p_frameDescriptor.camera.value().HasFrustumLightCulling();

	AddDescriptor<OvRendering::Features::LightingRenderFeature::LightingDescriptor>({
		FindActiveLights(sceneDescriptor.scene),
		frustumLightCulling ? sceneDescriptor.frustumOverride : std::nullopt
	});

	AddDescriptor<OvCore::Rendering::ReflectionRenderFeature::ReflectionDescriptor>({
		FindActiveReflectionProbes(sceneDescriptor.scene)
	});

	OvRendering::Core::CompositeRenderer::BeginFrame(p_frameDescriptor);

	AddDescriptor<FrameBuilder::ParsingResult>({
		FrameBuilder::Parse(FrameBuilder::ParsingInput{
			.scene = sceneDescriptor.scene
		})
	});

	// Default filtering result using the main camera (used by most render passes).
	// Some other render passes can decide to filter the drawables themselves, using the
	// FrameBuilder::ParsingResult instead of the FrameBuilder::FilteringResult one.
	AddDescriptor<FrameBuilder::FilteringResult>({
		FrameBuilder::Filter(
			GetDescriptor<FrameBuilder::ParsingResult>(),
			FrameBuilder::FilteringInput{
				.camera = p_frameDescriptor.camera.value(),
				.frustumOverride = sceneDescriptor.frustumOverride,
				.overrideMaterial = sceneDescriptor.overrideMaterial,
				.fallbackMaterial = sceneDescriptor.fallbackMaterial,
				.requiredVisibilityFlags = EVisibilityFlags::GEOMETRY
			}
		)
	});
}

void OvCore::Rendering::SceneRenderer::DrawModelWithSingleMaterial(OvRendering::Data::PipelineState p_pso, OvRendering::Resources::Model& p_model, OvRendering::Data::Material& p_material, const OvMaths::FMatrix4& p_modelMatrix)
{
	auto stateMask = p_material.GenerateStateMask();
	auto userMatrix = OvMaths::FMatrix4::Identity;

	auto engineDrawableDescriptor = EngineDrawableDescriptor{
		p_modelMatrix,
		userMatrix
	};

	for (auto mesh : p_model.GetMeshes())
	{
		OvRendering::Entities::Drawable element;
		element.mesh = *mesh;
		element.material = p_material;
		element.stateMask = stateMask;
		element.AddDescriptor(engineDrawableDescriptor);

		DrawEntity(p_pso, element);
	}
}
