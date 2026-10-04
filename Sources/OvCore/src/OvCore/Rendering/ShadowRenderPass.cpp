/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <string>
#include <type_traits>
#include <variant>

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Rendering/EngineBufferRenderFeature.h>
#include <OvCore/Rendering/EngineDrawableDescriptor.h>
#include <OvCore/Rendering/FrameBuilder.h>
#include <OvCore/Rendering/ShadowRenderFeature.h>
#include <OvCore/Rendering/ShadowRenderPass.h>
#include <OvCore/Rendering/SkinningDrawableDescriptor.h>
#include <OvCore/ResourceManagement/MaterialManager.h>
#include <OvCore/ResourceManagement/ModelManager.h>
#include <OvCore/ResourceManagement/ShaderManager.h>
#include <OvCore/ResourceManagement/TextureManager.h>

#include <OvRendering/Features/LightingRenderFeature.h>
#include <OvRendering/Utils/Profiling.h>

namespace
{
	const std::string kShadowPassName = "SHADOW_PASS";

	/**
	* Appends the bytes of the given value to the signature
	*/
	template<typename T>
	void Append(std::vector<std::byte>& p_signature, const T& p_value)
	{
		static_assert(std::is_standard_layout_v<T>, "Only plain data can be part of a signature");
		const auto bytes = reinterpret_cast<const std::byte*>(&p_value);
		p_signature.insert(p_signature.end(), bytes, bytes + sizeof(T));
	}

	void AppendShader(std::vector<std::byte>& p_signature, const OvRendering::Resources::Shader* p_shader)
	{
		Append(p_signature, p_shader);
		Append(p_signature, p_shader ? p_shader->GetVariantsVersion() : uint64_t{ 0 });
	}
}

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

	// Only one shadow map is supported: rendering the other shadow casting lights would be wasted work.
	if (auto light = ShadowRenderFeature::FindShadowCastingLight(lightingDescriptor.lights))
	{
		light->PrepareForShadowRendering(frameDescriptor);

		const auto casters = _FilterShadowCasters(light->shadowCamera.value());

		// The shadow map keeps its content between frames: it only needs to be rendered again when
		// the light, or something drawn into the shadow map, changed.
		const bool cached = light->shadowMapUpdateMode == OvRendering::Settings::EShadowMapUpdateMode::ON_CHANGE;
		bool upToDate = false;

		if (cached)
		{
			_BuildShadowMapSignature(light.value(), casters);
			upToDate = m_shadowMapSignature == light->shadowMapSignature;
		}
		else
		{
			light->InvalidateShadowMap();
		}

		if (!upToDate)
		{
			ZoneScopedN("Shadow Map Update");

			engineBufferRenderFeature.SetCamera(light->shadowCamera.value());

			light->shadowBuffer->Bind();
			m_renderer.SetViewport(0, 0, light->shadowMapResolution, light->shadowMapResolution);
			m_renderer.Clear(false, true, false);
			_DrawShadows(pso, casters);
			light->shadowBuffer->Unbind();

			engineBufferRenderFeature.SetCamera(frameDescriptor.camera.value());

			if (cached)
			{
				light->shadowMapSignature = m_shadowMapSignature;
			}
		}
	}

	if (auto output = frameDescriptor.outputBuffer)
	{
		output.value().Bind();
	}

	m_renderer.SetViewport(0, 0, frameDescriptor.renderWidth, frameDescriptor.renderHeight);
}

OvCore::Rendering::FrameBuilder::FilteringResult OvCore::Rendering::ShadowRenderPass::_FilterShadowCasters(
	const OvRendering::Entities::Camera& p_camera
)
{
	// Reuse the drawables parsed for this frame instead of walking the scene again.
	// Casters outside of the light camera frustum are culled, as they cannot contribute to the shadow map.
	return FrameBuilder::Filter(
		m_renderer.GetDescriptor<FrameBuilder::ParsingResult>(),
		FrameBuilder::FilteringInput{
			.camera = p_camera,
			.requiredVisibilityFlags = EVisibilityFlags::SHADOW,
			.filter = [](const OvRendering::Entities::Drawable&, const OvRendering::Data::Material& p_material) {
				return p_material.IsShadowCaster();
			}
		}
	);
}

void OvCore::Rendering::ShadowRenderPass::_BuildShadowMapSignature(
	const OvRendering::Entities::Light& p_light,
	const FrameBuilder::FilteringResult& p_casters
)
{
	ZoneScoped;

	using namespace OvCore::ResourceManagement;

	auto& signature = m_shadowMapSignature;
	signature.clear();
	m_signatureMaterials.clear();

	// Light (the light space matrix covers its position, rotation, shadow area size and camera following)
	Append(signature, p_light.shadowMapResolution);
	Append(signature, p_light.lightSpaceMatrix.value().data);

	// Reloaded resources keep their address, but not their content
	Append(signature, TextureManager::GetReloadCount());
	Append(signature, ModelManager::GetReloadCount());
	Append(signature, ShaderManager::GetReloadCount());
	Append(signature, MaterialManager::GetReloadCount());

	// Casters, in drawing order
	auto appendCasters = [&](const auto& p_filteredDrawables) {
		Append(signature, p_filteredDrawables.size());

		for (const auto& filteredDrawable : p_filteredDrawables | std::views::values)
		{
			const auto& drawable = filteredDrawable.drawable.get();
			auto& material = filteredDrawable.material.get();

			// Same material resolution as FrameBuilder::Prepare (see _DrawShadows)
			auto& targetMaterial = material.HasPass(kShadowPassName) ? material : m_shadowMaterial;

			Append(signature, &drawable.mesh.value());
			Append(signature, drawable.primitiveMode);
			Append(signature, &targetMaterial);

			// The resolved material shader decides if skinning is applied
			Append(signature, &material);
			AppendShader(signature, material.GetShader());

			const auto& engineDescriptor = drawable.template GetDescriptor<EngineDrawableDescriptor>();
			Append(signature, engineDescriptor.modelMatrix.data);
			Append(signature, engineDescriptor.userMatrix.data);

			OvTools::Utils::OptRef<const SkinningDrawableDescriptor> skinningDescriptor;
			const bool skinned = drawable.template TryGetDescriptor<SkinningDrawableDescriptor>(skinningDescriptor);
			Append(signature, skinned);

			if (skinned)
			{
				Append(signature, skinningDescriptor->matrices);
				Append(signature, skinningDescriptor->count);
				Append(signature, skinningDescriptor->poseVersion);
				Append(signature, skinningDescriptor->boundsScale);
			}

			m_signatureMaterials.push_back(&targetMaterial);
		}
	};

	appendCasters(p_casters.opaques);
	appendCasters(p_casters.transparents);
	appendCasters(p_casters.ui);

	// Materials used to draw the casters, each one once
	std::ranges::sort(m_signatureMaterials);
	const auto duplicates = std::ranges::unique(m_signatureMaterials);
	m_signatureMaterials.erase(duplicates.begin(), duplicates.end());

	for (auto material : m_signatureMaterials)
	{
		Append(signature, material);
		AppendShader(signature, material->GetShader());
		Append(signature, material->GetFeaturesVersion());
		Append(signature, material->GetGPUInstances());

		// Property values are compared (rather than a version number), as they can also be edited by reference.
		// Single use properties are set for a specific draw (e.g. by a render feature), not by the user.
		const auto& properties = material->GetProperties();
		Append(signature, properties.size());

		for (const auto& property : properties | std::views::values)
		{
			Append(signature, property.singleUse);

			if (!property.singleUse)
			{
				Append(signature, property.value.index());

				std::visit([&signature](const auto& p_value) {
					using T = std::decay_t<decltype(p_value)>;

					if constexpr (std::is_same_v<T, OvMaths::FMatrix3> || std::is_same_v<T, OvMaths::FMatrix4>)
					{
						Append(signature, p_value.data);
					}
					else if constexpr (!std::is_same_v<T, std::monostate>)
					{
						Append(signature, p_value);
					}
				}, property.value);
			}
		}
	}
}

void OvCore::Rendering::ShadowRenderPass::_DrawShadows(
	OvRendering::Data::PipelineState p_pso,
	const FrameBuilder::FilteringResult& p_casters
)
{
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

	FrameBuilder::Draw(m_renderer, p_pso, p_casters, preparationInput);
}
