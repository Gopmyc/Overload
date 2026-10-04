/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <string>
#include <type_traits>
#include <tracy/Tracy.hpp>

#include <OvCore/ECS/Components/CMaterialRenderer.h>
#include <OvCore/ECS/Components/CModelRenderer.h>
#include <OvCore/ECS/Components/CSkinnedMeshRenderer.h>
#include <OvCore/Rendering/EngineDrawableDescriptor.h>
#include <OvCore/Rendering/FrameBuilder.h>
#include <OvCore/Rendering/SceneDrawableDescriptor.h>
#include <OvCore/Rendering/SkinningDrawableDescriptor.h>
#include <OvCore/Rendering/SkinningUtils.h>

namespace
{
	const std::string kSkinningFeatureName{ OvCore::Rendering::SkinningUtils::kFeatureName };
}

OvCore::Rendering::FrameBuilder::ParsingResult OvCore::Rendering::FrameBuilder::Parse(const ParsingInput& p_input)
{
	ZoneScoped;

	using namespace OvCore::ECS::Components;

	// Containers for the parsed drawables.
	ParsingResult result;

	const auto& scene = p_input.scene;
	const auto& modelRenderers = scene.GetFastAccessComponents().modelRenderers;

	// At least one drawable per model renderer
	result.drawables.reserve(modelRenderers.size());

	for (const auto modelRenderer : modelRenderers)
	{
		auto& owner = modelRenderer->owner;
		if (!owner.IsActive()) continue;
		const auto model = modelRenderer->GetModel();
		if (!model) continue;
		const auto materialRenderer = modelRenderer->owner.GetComponent<CMaterialRenderer>();
		if (!materialRenderer) continue;
		const auto* skinnedRenderer = owner.GetComponent<CSkinnedMeshRenderer>();
		const bool hasSkinning = SkinningUtils::IsSkinningActive(skinnedRenderer);

		const auto& transform = owner.transform.GetFTransform();
		const auto& materials = materialRenderer->GetMaterials();

		for (auto& mesh : model->GetMeshes())
		{
			OvTools::Utils::OptRef<OvRendering::Data::Material> material;

			if (mesh->GetMaterialIndex() < kMaxMaterialCount)
			{
				material = materials.at(mesh->GetMaterialIndex());
			}

			OvRendering::Entities::Drawable drawable{
				.mesh = *mesh,
				.material = material,
				.stateMask = material.has_value() ? material->GenerateStateMask() : OvRendering::Data::StateMask{},
			};

			auto bounds = [&]() -> std::optional<OvRendering::Geometry::BoundingSphere> {
				using enum CModelRenderer::EFrustumBehaviour;
				switch (modelRenderer->GetFrustumBehaviour())
				{
				case MESH_BOUNDS: return mesh->GetBoundingSphere();
				case DEPRECATED_MODEL_BOUNDS: return model->GetBoundingSphere();
				case CUSTOM_BOUNDS: return modelRenderer->GetCustomBoundingSphere();
				default: return std::nullopt;
				}
				return std::nullopt;
			}();

			drawable.AddDescriptor<SceneDrawableDescriptor>({
				.actor = modelRenderer->owner,
				.visibilityFlags = materialRenderer->GetVisibilityFlags(),
				.bounds = bounds
			});

			drawable.AddDescriptor<EngineDrawableDescriptor>({
				transform.GetWorldMatrix(),
				materialRenderer->GetUserMatrix()
			});

			if (hasSkinning && mesh->HasSkinningData())
			{
				SkinningUtils::ApplyDescriptor(drawable, *skinnedRenderer);
			}

			result.drawables.push_back(std::move(drawable));
		}
	}

	return result;
}

OvCore::Rendering::FrameBuilder::FilteringResult OvCore::Rendering::FrameBuilder::Filter(
	const ParsingResult& p_parsingResult,
	const FilteringInput& p_input
)
{
	ZoneScoped;

	FilteringResult result;

	// Most drawables are opaque
	result.opaques.reserve(p_parsingResult.drawables.size());

	const auto& camera = p_input.camera;
	const auto& frustumOverride = p_input.frustumOverride;

	// Determine if we should use frustum culling
	OvTools::Utils::OptRef<const OvRendering::Data::Frustum> frustum;
	if (camera.HasFrustumGeometryCulling())
	{
		frustum = frustumOverride ? frustumOverride : camera.GetFrustum();
	}

	// Process each drawable
	for (const auto& drawable : p_parsingResult.drawables)
	{
		const auto& desc = drawable.GetDescriptor<SceneDrawableDescriptor>();

		// Skip drawables that do not satisfy the required visibility flags
		if (!SatisfiesVisibility(desc.visibilityFlags, p_input.requiredVisibilityFlags))
		{
			continue;
		}

		const auto targetMaterial =
			p_input.overrideMaterial.has_value() ?
			p_input.overrideMaterial.value() :
			(drawable.material.has_value() ? drawable.material.value() : p_input.fallbackMaterial);

		// Skip if material is invalid
		if (!targetMaterial || !targetMaterial->IsValid()) continue;

		// Filter drawables based on the type (UI, opaque, transparent)
		// Except for the fallback material, which is always included.
		if (!p_input.fallbackMaterial || &p_input.fallbackMaterial.value() != &targetMaterial.value())
		{
			const bool isUI = targetMaterial->IsUserInterface();
			if (isUI && !p_input.includeUI) continue;
			if (!isUI && !targetMaterial->IsBlendable() && !p_input.includeOpaque) continue;
			if (!isUI && targetMaterial->IsBlendable() && !p_input.includeTransparent) continue;
		}

		// Skip drawables rejected by the custom filter
		if (p_input.filter && !p_input.filter(drawable, targetMaterial.value()))
		{
			continue;
		}

		// Perform frustum culling if enabled
		if (frustum && desc.bounds.has_value())
		{
			ZoneScopedN("Frustum Culling");

			if (!IsInFrustum(drawable, frustum.value()))
			{
				continue; // Skip this drawable as it's outside the frustum
			}
		}

		// Calculate distance to camera for sorting
		const float distanceToCamera = OvMaths::FVector3::Distance(
			desc.actor.transform.GetWorldPosition(),
			camera.GetPosition()
		);

		auto& material = targetMaterial.value();

		auto insert = [&](auto& p_filteredDrawables) {
			using DrawOrderKey = typename std::remove_reference_t<decltype(p_filteredDrawables)>::value_type::first_type;

			p_filteredDrawables.emplace_back(
				DrawOrderKey{
					.order = material.GetDrawOrder(),
					.materialKey = reinterpret_cast<uintptr_t>(&material),
					.distance = distanceToCamera
				},
				FilteredDrawable{
					.drawable = drawable,
					.material = material
				}
			);
		};

		// Categorize drawable based on their type.
		// Sorting happens once all the drawables are gathered.
		if (material.IsUserInterface())
		{
			insert(result.ui);
		}
		else if (material.IsBlendable())
		{
			insert(result.transparents);
		}
		else
		{
			insert(result.opaques);
		}
	}

	const auto sortByDrawOrder = [](auto& p_filteredDrawables) {
		std::stable_sort(p_filteredDrawables.begin(), p_filteredDrawables.end(), [](const auto& p_lhs, const auto& p_rhs) {
			return p_lhs.first < p_rhs.first;
		});
	};

	sortByDrawOrder(result.opaques);
	sortByDrawOrder(result.transparents);
	sortByDrawOrder(result.ui);

	return result;
}

bool OvCore::Rendering::FrameBuilder::IsInFrustum(
	const OvRendering::Entities::Drawable& p_drawable,
	const OvRendering::Data::Frustum& p_frustum
)
{
	const auto& desc = p_drawable.GetDescriptor<SceneDrawableDescriptor>();

	if (!desc.bounds.has_value())
	{
		return true;
	}

	auto cullingBounds = desc.bounds.value();

	OvTools::Utils::OptRef<const SkinningDrawableDescriptor> skinningDescriptor;
	if (p_drawable.TryGetDescriptor<SkinningDrawableDescriptor>(skinningDescriptor))
	{
		cullingBounds.radius *= skinningDescriptor->boundsScale;
	}

	return p_frustum.BoundingSphereInFrustum(cullingBounds, desc.actor.transform.GetFTransform());
}

OvRendering::Entities::Drawable OvCore::Rendering::FrameBuilder::Prepare(
	const FilteredDrawable& p_filteredDrawable,
	const PreparationInput& p_input
)
{
	auto& material = p_filteredDrawable.material.get();

	// If a pass is requested and the material doesn't have it, use the pass fallback material (if any).
	auto& targetMaterial =
		p_input.pass && p_input.passFallbackMaterial && !material.HasPass(p_input.pass.value()) ?
		p_input.passFallbackMaterial.value() :
		material;

	// Copy the parsed drawable to avoid modifying the original one.
	auto drawable = p_filteredDrawable.drawable.get();
	drawable.material = targetMaterial;
	drawable.stateMask = targetMaterial.GenerateStateMask();
	drawable.pass = p_input.pass;

	// Skinning is only applied if both the resolved and target materials support it.
	if (
		drawable.HasDescriptor<SkinningDrawableDescriptor>() &&
		material.SupportsFeature(kSkinningFeatureName) &&
		targetMaterial.SupportsFeature(kSkinningFeatureName)
	)
	{
		drawable.featureSetOverride = SkinningUtils::BuildFeatureSet(&targetMaterial.GetFeatures());
	}
	else
	{
		drawable.featureSetOverride = std::nullopt;
	}

	if (p_input.customPreparation)
	{
		p_input.customPreparation(drawable);
	}

	return drawable;
}

void OvCore::Rendering::FrameBuilder::Draw(
	OvRendering::Core::CompositeRenderer& p_renderer,
	OvRendering::Data::PipelineState p_pso,
	const FilteringResult& p_filteringResult,
	const PreparationInput& p_input
)
{
	Draw(p_renderer, p_pso, p_filteringResult.opaques, p_input);
	Draw(p_renderer, p_pso, p_filteringResult.transparents, p_input);
	Draw(p_renderer, p_pso, p_filteringResult.ui, p_input);
}
