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
#include <OvCore/ECS/Components/UI/CCanvas.h>
#include <OvCore/ECS/Components/UI/CImage.h>
#include <OvCore/ECS/Components/UI/CText.h>
#include <OvCore/Rendering/EngineDrawableDescriptor.h>
#include <OvCore/Rendering/FrameBuilder.h>
#include <OvCore/Rendering/SceneDrawableDescriptor.h>
#include <OvCore/Rendering/SkinningDrawableDescriptor.h>
#include <OvCore/Rendering/SkinningUtils.h>
#include <OvCore/Rendering/UIRenderingUtils.h>

namespace
{
	using namespace OvCore::Rendering;

	const std::string kSkinningFeatureName{ OvCore::Rendering::SkinningUtils::kFeatureName };

	EngineDrawableDescriptor CreateUIDrawableDescriptor(
		OvCore::ECS::Actor& p_owner,
		const OvCore::Rendering::UIRenderingUtils::UIFrameResolver& p_uiFrameResolver,
		const OvMaths::FMatrix4& p_uiProjectionMatrix,
		const OvMaths::FVector2& p_elementSize,
		bool& p_outDepthTested,
		bool p_preserveAspect = false
	)
	{
		p_outDepthTested = false;

		EngineDrawableDescriptor descriptor{
			.modelMatrix = p_owner.transform.GetFTransform().GetWorldMatrix(),
			.userMatrix = OvMaths::FMatrix4::Identity
		};

		OvCore::Rendering::UIRenderingUtils::ResolvedUIElement resolvedElement;
		if (p_uiFrameResolver.ResolveElement(
			p_owner,
			p_elementSize,
			resolvedElement
		))
		{
			if (
				p_preserveAspect &&
				p_elementSize.x > 0.0f &&
				p_elementSize.y > 0.0f &&
				resolvedElement.effectiveSize.x > 0.0f &&
				resolvedElement.effectiveSize.y > 0.0f
			)
			{
				const float fitScale = std::min(
					resolvedElement.effectiveSize.x / p_elementSize.x,
					resolvedElement.effectiveSize.y / p_elementSize.y
				);
				descriptor.modelMatrix = resolvedElement.frameMatrix * OvMaths::FMatrix4::Scaling({
					fitScale,
					fitScale,
					1.0f
				});
			}
			else
			{
				descriptor.modelMatrix = resolvedElement.modelMatrix;
			}

			if (resolvedElement.screenSpace)
			{
				descriptor.viewMatrixOverride = OvMaths::FMatrix4::Identity;
				descriptor.projectionMatrixOverride = p_uiProjectionMatrix;
			}

			// A world space canvas is part of the scene: what stands in front of it hides it. A screen space
			// canvas previewed in the world stays drawn over the scene
			p_outDepthTested =
				resolvedElement.canvas &&
				resolvedElement.canvas->GetRenderMode() == OvCore::ECS::Components::UI::CCanvas::ERenderMode::WORLD_SPACE;
		}

		return descriptor;
	}

	void AppendImageDrawable(
		OvCore::Rendering::FrameBuilder::ParsingResult& p_result,
		OvCore::ECS::Components::UI::CImage& p_image,
		const OvCore::Rendering::UIRenderingUtils::UIFrameResolver& p_uiFrameResolver,
		const OvMaths::FMatrix4& p_uiProjectionMatrix,
		int p_drawOrder
	)
	{
		auto& owner = p_image.owner;
		auto* material = p_image.GetMaterial();
		if (!material) return;

		OvRendering::Entities::Drawable drawable{
			.mesh = p_image.GetMesh(),
			.material = *material,
			.stateMask = material->GenerateStateMask()
		};

		drawable.AddDescriptor<SceneDrawableDescriptor>({
			.actor = owner,
			.visibilityFlags = EVisibilityFlags::GEOMETRY,
			.bounds = std::nullopt,
			.drawOrderOverride = p_drawOrder,
			.isUserInterface = true
		});

		bool depthTested = false;

		drawable.AddDescriptor<EngineDrawableDescriptor>(
			CreateUIDrawableDescriptor(
				owner,
				p_uiFrameResolver,
				p_uiProjectionMatrix,
				p_image.GetIntrinsicSize(),
				depthTested,
				p_image.GetPreserveAspect()
			)
		);

		drawable.stateMask.depthTest = drawable.stateMask.depthTest || depthTested;

		p_result.drawables.push_back(std::move(drawable));
	}

	void AppendTextDrawable(
		OvCore::Rendering::FrameBuilder::ParsingResult& p_result,
		OvCore::ECS::Components::UI::CText& p_text,
		const OvCore::Rendering::UIRenderingUtils::UIFrameResolver& p_uiFrameResolver,
		const OvMaths::FMatrix4& p_uiProjectionMatrix,
		int p_drawOrder
	)
	{
		auto& owner = p_text.owner;
		auto* material = p_text.GetMaterial();
		if (!material) return;

		const auto baseTextSize = p_text.GetSize();
		OvCore::Rendering::UIRenderingUtils::ResolvedUIElement resolvedElement;
		const bool hasResolvedElement = p_uiFrameResolver.ResolveElement(
			owner,
			baseTextSize,
			resolvedElement
		);
		const OvMaths::FVector2 textLayoutSize = hasResolvedElement ?
			OvMaths::FVector2{
				resolvedElement.widthDriven ? resolvedElement.effectiveSize.x : 0.0f,
				resolvedElement.heightDriven ? resolvedElement.effectiveSize.y : 0.0f
			} :
			owner.transform.GetUISize();

		auto* mesh = p_text.GetMesh(textLayoutSize);
		if (!mesh) return;
		const auto renderedTextSize = p_text.GetSize(textLayoutSize);

		OvRendering::Entities::Drawable drawable{
			.mesh = *mesh,
			.material = *material,
			.stateMask = material->GenerateStateMask()
		};

		drawable.AddDescriptor<SceneDrawableDescriptor>({
			.actor = owner,
			.visibilityFlags = EVisibilityFlags::GEOMETRY,
			.bounds = std::nullopt,
			.drawOrderOverride = p_drawOrder,
			.isUserInterface = true
		});

		bool depthTested = false;

		drawable.AddDescriptor<EngineDrawableDescriptor>(
			CreateUIDrawableDescriptor(
				owner,
				p_uiFrameResolver,
				p_uiProjectionMatrix,
				renderedTextSize,
				depthTested
			)
		);

		drawable.stateMask.depthTest = drawable.stateMask.depthTest || depthTested;

		p_result.drawables.push_back(std::move(drawable));
	}

	void AppendHierarchyUIDrawables(
		OvCore::Rendering::FrameBuilder::ParsingResult& p_result,
		OvCore::ECS::Actor& p_actor,
		const OvCore::Rendering::UIRenderingUtils::UIFrameResolver& p_uiFrameResolver,
		const OvCore::ECS::Components::UI::CCanvas* p_canvas,
		const OvMaths::FMatrix4& p_uiProjectionMatrix,
		int& p_drawOrder
	)
	{
		if (!p_actor.IsActive())
		{
			return;
		}

		if (auto* canvas = p_actor.GetComponent<OvCore::ECS::Components::UI::CCanvas>())
		{
			p_canvas = canvas;
		}

		if (p_canvas)
		{
			if (auto* image = p_actor.GetComponent<OvCore::ECS::Components::UI::CImage>())
			{
				AppendImageDrawable(
					p_result,
					*image,
					p_uiFrameResolver,
					p_uiProjectionMatrix,
					p_drawOrder++
				);
			}

			if (auto* text = p_actor.GetComponent<OvCore::ECS::Components::UI::CText>())
			{
				AppendTextDrawable(
					p_result,
					*text,
					p_uiFrameResolver,
					p_uiProjectionMatrix,
					p_drawOrder++
				);
			}
		}

		for (auto* child : p_actor.GetChildren())
		{
			if (child)
			{
				AppendHierarchyUIDrawables(
					p_result,
					*child,
					p_uiFrameResolver,
					p_canvas,
					p_uiProjectionMatrix,
					p_drawOrder
				);
			}
		}
	}

	void AppendHierarchyUIDrawables(
		OvCore::Rendering::FrameBuilder::ParsingResult& p_result,
		OvCore::SceneSystem::Scene& p_scene,
		const OvCore::Rendering::UIRenderingUtils::UIFrameResolver& p_uiFrameResolver
	)
	{
		int drawOrder = 0;
		const auto uiProjectionMatrix = p_uiFrameResolver.CreateProjectionMatrix();

		for (auto* actor : p_scene.GetActors())
		{
			if (actor && !actor->HasParent())
			{
				AppendHierarchyUIDrawables(
					p_result,
					*actor,
					p_uiFrameResolver,
					nullptr,
					uiProjectionMatrix,
					drawOrder
				);
			}
		}
	}
}

OvCore::Rendering::FrameBuilder::ParsingResult OvCore::Rendering::FrameBuilder::Parse(const ParsingInput& p_input)
{
	ZoneScoped;

	using namespace OvCore::ECS::Components;

	// Containers for the parsed drawables.
	ParsingResult result;

	auto& scene = p_input.scene;
	const auto& modelRenderers = scene.GetFastAccessComponents().modelRenderers;

	OvCore::Rendering::UIRenderingUtils::UIFrameResolver fallbackUIFrameResolver{
		p_input.renderSize,
		p_input.renderUIInScreenSpace
	};
	const auto& uiFrameResolver = p_input.uiFrameResolver ?
		*p_input.uiFrameResolver :
		fallbackUIFrameResolver;

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

	AppendHierarchyUIDrawables(result, scene, uiFrameResolver);

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

		if (desc.isUserInterface && !p_input.includeUI)
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
					.order = desc.drawOrderOverride.value_or(material.GetDrawOrder()),
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
