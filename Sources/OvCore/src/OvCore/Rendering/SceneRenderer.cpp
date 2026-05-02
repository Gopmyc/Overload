/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <ranges>
#include <string>
#include <tracy/Tracy.hpp>

#include <OvCore/ECS/Components/CModelRenderer.h>
#include <OvCore/ECS/Components/CMaterialRenderer.h>
#include <OvCore/ECS/Components/CSkinnedMeshRenderer.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Rendering/EngineBufferRenderFeature.h>
#include <OvCore/Rendering/EngineDrawableDescriptor.h>
#include <OvCore/Rendering/PostProcessRenderPass.h>
#include <OvCore/Rendering/ReflectionRenderFeature.h>
#include <OvCore/Rendering/ReflectionRenderPass.h>
#include <OvCore/Rendering/SceneRenderer.h>
#include <OvCore/Rendering/ShadowRenderFeature.h>
#include <OvCore/Rendering/ShadowRenderPass.h>
#include <OvCore/Rendering/SkinningRenderFeature.h>
#include <OvCore/Rendering/SkinningUtils.h>
#include <OvCore/ResourceManagement/ShaderManager.h>
#include <OvRendering/Data/Frustum.h>
#include <OvRendering/Features/LightingRenderFeature.h>
#include <OvRendering/HAL/Profiling.h>
#include <OvRendering/Resources/Loaders/ShaderLoader.h>

namespace
{
	using namespace OvCore::Rendering;
	const std::string kSkinningFeatureName{ SkinningUtils::kFeatureName };

	bool AreBoundingSpheresEqual(
		const OvRendering::Geometry::BoundingSphere& p_left,
		const OvRendering::Geometry::BoundingSphere& p_right
	)
	{
		return
			p_left.position.x == p_right.position.x &&
			p_left.position.y == p_right.position.y &&
			p_left.position.z == p_right.position.z &&
			p_left.radius == p_right.radius;
	}

	std::optional<OvRendering::Geometry::BoundingSphere> ResolveBounds(
		OvCore::ECS::Components::CModelRenderer::EFrustumBehaviour p_frustumBehaviour,
		const OvRendering::Resources::Model& p_model,
		const OvRendering::Resources::Mesh& p_mesh,
		const OvRendering::Geometry::BoundingSphere& p_customBounds
	)
	{
		using enum OvCore::ECS::Components::CModelRenderer::EFrustumBehaviour;

		switch (p_frustumBehaviour)
		{
		case MESH_BOUNDS:
			return p_mesh.GetBoundingSphere();
		case DEPRECATED_MODEL_BOUNDS:
			return p_model.GetBoundingSphere();
		case CUSTOM_BOUNDS:
			return p_customBounds;
		default:
			return std::nullopt;
		}
	}

	template<SceneRenderer::EOrderingMode OrderingMode>
	void SortDrawableList(SceneRenderer::DrawableList<OrderingMode>& p_drawables)
	{
		std::stable_sort(
			p_drawables.begin(),
			p_drawables.end(),
			[](const auto& p_left, const auto& p_right)
			{
				return p_left.first < p_right.first;
			}
		);
	}

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
			p_pso.stencilOpFail = OvRendering::Settings::EOperation::REPLACE;
			p_pso.depthOpFail = OvRendering::Settings::EOperation::REPLACE;
			p_pso.bothOpFail = OvRendering::Settings::EOperation::REPLACE;
			p_pso.colorWriting.mask = 0x00;
		}

	private:
		bool m_stencilWrite;
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

			const auto& drawables = m_renderer.GetDescriptor<SceneRenderer::SceneFilteredDrawablesDescriptor>();

			for (const auto& drawable : drawables.opaques | std::views::values)
			{
				m_renderer.DrawEntity(p_pso, drawable);
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

			const auto& drawables = m_renderer.GetDescriptor<SceneRenderer::SceneFilteredDrawablesDescriptor>();

			for (const auto& drawable : drawables.transparents | std::views::values)
			{
				m_renderer.DrawEntity(p_pso, drawable);
			}
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

			const auto& drawables = m_renderer.GetDescriptor<SceneRenderer::SceneFilteredDrawablesDescriptor>();

			for (const auto& drawable : drawables.ui | std::views::values)
			{
				m_renderer.DrawEntity(p_pso, drawable);
			}
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

	AddDescriptor<SceneDrawablesDescriptor>({
		ParseScene(SceneParsingInput{
			.scene = sceneDescriptor.scene
		})
	});

	// Default filtered drawables descriptor using the main camera (used by most render passes).
	// Some other render passes can decide to filter the drawables themselves, using the 
	// SceneDrawablesDescriptor instead of the SceneFilteredDrawablesDescriptor one.
	AddDescriptor<SceneFilteredDrawablesDescriptor>({
		FilterDrawables(
			GetDescriptor<SceneDrawablesDescriptor>(),
			SceneDrawablesFilteringInput{
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

SceneRenderer::SceneDrawablesDescriptor OvCore::Rendering::SceneRenderer::ParseScene(const SceneParsingInput& p_input)
{
	ZoneScoped;

	using namespace OvCore::ECS::Components;

	const auto& scene = p_input.scene;
	std::vector<ParseCacheEntry> currentEntries;
	currentEntries.reserve(scene.GetFastAccessComponents().modelRenderers.size());

	for (const auto* modelRenderer : scene.GetFastAccessComponents().modelRenderers)
	{
		const auto* materialRenderer = modelRenderer->owner.GetComponent<CMaterialRenderer>();
		const auto* skinnedRenderer = modelRenderer->owner.GetComponent<CSkinnedMeshRenderer>();

		currentEntries.push_back({
			.modelRenderer = modelRenderer,
			.model = modelRenderer->GetModel(),
			.materialRenderer = materialRenderer,
			.skinnedRenderer = skinnedRenderer,
			.frustumBehaviour = modelRenderer->GetFrustumBehaviour(),
			.customBounds = modelRenderer->GetCustomBoundingSphere()
		});
	}

	if (ShouldRebuildParseCache(scene, currentEntries))
	{
		RebuildParseCache(scene, currentEntries);
	}

	return SceneDrawablesDescriptor{
		.drawables = m_parsedDrawablesCache.data(),
		.count = m_parsedDrawablesCache.size()
	};
}

bool OvCore::Rendering::SceneRenderer::ShouldRebuildParseCache(
	const OvCore::SceneSystem::Scene& p_scene,
	const std::vector<ParseCacheEntry>& p_currentEntries
) const
{
	using enum OvCore::ECS::Components::CModelRenderer::EFrustumBehaviour;

	if (m_cachedScene != &p_scene)
	{
		return true;
	}

	if (m_parseCacheEntries.size() != p_currentEntries.size())
	{
		return true;
	}

	for (size_t i = 0; i < p_currentEntries.size(); ++i)
	{
		const auto& current = p_currentEntries[i];
		const auto& cached = m_parseCacheEntries[i];

		if (
			current.modelRenderer != cached.modelRenderer ||
			current.model != cached.model ||
			current.materialRenderer != cached.materialRenderer ||
			current.skinnedRenderer != cached.skinnedRenderer ||
			current.frustumBehaviour != cached.frustumBehaviour
			)
		{
			return true;
		}

		if (
			current.frustumBehaviour == CUSTOM_BOUNDS &&
			!AreBoundingSpheresEqual(current.customBounds, cached.customBounds)
			)
		{
			return true;
		}
	}

	return false;
}

void OvCore::Rendering::SceneRenderer::RebuildParseCache(
	const OvCore::SceneSystem::Scene& p_scene,
	const std::vector<ParseCacheEntry>& p_currentEntries
)
{
	m_cachedScene = &p_scene;
	m_parseCacheEntries = p_currentEntries;
	m_parsedDrawablesCache.clear();

	size_t drawableCount = 0;
	for (const auto& entry : p_currentEntries)
	{
		if (entry.model && entry.materialRenderer)
		{
			drawableCount += entry.model->GetMeshes().size();
		}
	}

	m_parsedDrawablesCache.reserve(drawableCount);

	for (const auto& entry : p_currentEntries)
	{
		if (!entry.model || !entry.materialRenderer)
		{
			continue;
		}

		for (auto* mesh : entry.model->GetMeshes())
		{
			if (!mesh)
			{
				continue;
			}

			m_parsedDrawablesCache.push_back({
				.actor = &entry.modelRenderer->owner,
				.materialRenderer = entry.materialRenderer,
				.skinnedRenderer = entry.skinnedRenderer,
				.mesh = mesh,
				.materialIndex = mesh->GetMaterialIndex(),
				.meshHasSkinningData = mesh->HasSkinningData(),
				.bounds = ResolveBounds(entry.frustumBehaviour, *entry.model, *mesh, entry.customBounds)
			});
		}
	}
}

SceneRenderer::SceneFilteredDrawablesDescriptor OvCore::Rendering::SceneRenderer::FilterDrawables(
	const SceneDrawablesDescriptor& p_drawables,
	const SceneDrawablesFilteringInput& p_filteringInput
)
{
	ZoneScoped;

	using namespace OvCore::ECS::Components;

	SceneFilteredDrawablesDescriptor output;

	const auto& camera = p_filteringInput.camera;
	const auto& frustumOverride = p_filteringInput.frustumOverride;

	// Determine if we should use frustum culling
	OvTools::Utils::OptRef<const OvRendering::Data::Frustum> frustum;
	if (camera.HasFrustumGeometryCulling())
	{
		frustum = frustumOverride ? frustumOverride : camera.GetFrustum();
	}

	output.opaques.reserve(p_drawables.count);
	output.transparents.reserve(p_drawables.count);
	output.ui.reserve(p_drawables.count);

	// Process each parsed drawable
	for (size_t i = 0; i < p_drawables.count; ++i)
	{
		const auto& parsedDrawable = p_drawables.drawables[i];
		if (!parsedDrawable.actor || !parsedDrawable.materialRenderer || !parsedDrawable.mesh)
		{
			continue;
		}

		auto& actor = *parsedDrawable.actor;
		if (!actor.IsActive())
		{
			continue;
		}

		const auto visibilityFlags = parsedDrawable.materialRenderer->GetVisibilityFlags();

		// Skip drawables that do not satisfy the required visibility flags
		if (!SatisfiesVisibility(visibilityFlags, p_filteringInput.requiredVisibilityFlags))
		{
			continue;
		}

		const auto& materials = parsedDrawable.materialRenderer->GetMaterials();

		OvTools::Utils::OptRef<OvRendering::Data::Material> parsedMaterial;
		if (parsedDrawable.materialIndex < kMaxMaterialCount)
		{
			parsedMaterial = materials.at(parsedDrawable.materialIndex);
		}

		const auto targetMaterial =
			p_filteringInput.overrideMaterial.has_value() ?
			p_filteringInput.overrideMaterial.value() :
			(parsedMaterial.has_value() ? parsedMaterial.value() : p_filteringInput.fallbackMaterial);

		// Skip if material is invalid
		if (!targetMaterial || !targetMaterial->IsValid())
		{
			continue;
		}

		// Filter drawables based on the type (UI, opaque, transparent)
		// Except for the fallback material, which is always included.
		if (!p_filteringInput.fallbackMaterial || &p_filteringInput.fallbackMaterial.value() != &targetMaterial.value())
		{
			const bool isUI = targetMaterial->IsUserInterface();
			if (isUI && !p_filteringInput.includeUI)
			{
				continue;
			}
			if (!isUI && !targetMaterial->IsBlendable() && !p_filteringInput.includeOpaque)
			{
				continue;
			}
			if (!isUI && targetMaterial->IsBlendable() && !p_filteringInput.includeTransparent)
			{
				continue;
			}
		}

		const bool hasSkinning =
			parsedDrawable.skinnedRenderer &&
			parsedDrawable.meshHasSkinningData &&
			SkinningUtils::IsSkinningActive(parsedDrawable.skinnedRenderer);

		// Perform frustum culling if enabled
		if (frustum && parsedDrawable.bounds.has_value())
		{
			ZoneScopedN("Frustum Culling");

			auto cullingBounds = parsedDrawable.bounds.value();
			if (hasSkinning)
			{
				cullingBounds.radius *= parsedDrawable.skinnedRenderer->GetMeshBoundsScale();
			}

			if (!frustum->BoundingSphereInFrustum(cullingBounds, actor.transform.GetFTransform()))
			{
				continue; // Skip this drawable as it's outside the frustum
			}
		}

		// Calculate distance to camera for sorting
		const float distanceToCamera = OvMaths::FVector3::Distance(
			actor.transform.GetWorldPosition(),
			camera.GetPosition()
		);

		// Build the filtered drawable once all checks passed.
		OvRendering::Entities::Drawable drawable{
			.mesh = *parsedDrawable.mesh,
			.material = targetMaterial,
			.stateMask = targetMaterial->GenerateStateMask(),
		};

		drawable.AddDescriptor<SceneDrawableDescriptor>({
			.actor = actor,
			.visibilityFlags = visibilityFlags,
			.bounds = parsedDrawable.bounds
		});

		drawable.AddDescriptor<EngineDrawableDescriptor>({
			actor.transform.GetWorldMatrix(),
			parsedDrawable.materialRenderer->GetUserMatrix()
		});

		if (hasSkinning)
		{
			SkinningUtils::ApplyDescriptor(drawable, *parsedDrawable.skinnedRenderer);
		}

		if (
			hasSkinning &&
			targetMaterial->HasShader() &&
			targetMaterial->SupportsFeature(kSkinningFeatureName)
		)
		{
			drawable.featureSetOverride = SkinningUtils::BuildFeatureSet(&targetMaterial->GetFeatures());
		}
		else
		{
			drawable.featureSetOverride = std::nullopt;
		}

		// Categorize drawable based on their type.
		// This is also where sort keys are built.
		if (targetMaterial->IsUserInterface())
		{
			output.ui.emplace_back(decltype(decltype(output.ui)::value_type::first){
				.order = targetMaterial->GetDrawOrder(),
				.distance = distanceToCamera
			}, std::move(drawable));
		}
		else if (targetMaterial->IsBlendable())
		{
			output.transparents.emplace_back(decltype(decltype(output.transparents)::value_type::first){
				.order = targetMaterial->GetDrawOrder(),
				.distance = distanceToCamera
			}, std::move(drawable));
		}
		else
		{
			output.opaques.emplace_back(decltype(decltype(output.opaques)::value_type::first){
				.order = targetMaterial->GetDrawOrder(),
				.distance = distanceToCamera
			}, std::move(drawable));
		}
	}

	SortDrawableList(output.opaques);
	SortDrawableList(output.transparents);
	SortDrawableList(output.ui);

	return output;
}
