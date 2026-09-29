/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cstdint>
#include <string>

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Rendering/EngineBufferRenderFeature.h>
#include <OvCore/Rendering/ShadowRenderFeature.h>
#include <OvCore/Rendering/ShadowRenderPass.h>
#include <OvCore/Rendering/SkinningDrawableDescriptor.h>
#include <OvCore/Rendering/SkinningUtils.h>
#include <OvCore/ResourceManagement/ShaderManager.h>

#include <OvRendering/Features/LightingRenderFeature.h>
#include <OvRendering/Utils/Profiling.h>

const std::string kShadowPassName = "SHADOW_PASS";
const std::string kSkinningFeatureName = std::string{ OvCore::Rendering::SkinningUtils::kFeatureName };

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

		engineBufferRenderFeature.SetCamera(light->shadowCamera.value());

		light->shadowBuffer->Bind();
		m_renderer.SetViewport(0, 0, light->shadowMapResolution, light->shadowMapResolution);
		m_renderer.Clear(false, true, false);
		_DrawShadows(pso, light->shadowCamera->GetFrustum());
		light->shadowBuffer->Unbind();

		engineBufferRenderFeature.SetCamera(frameDescriptor.camera.value());
	}

	if (auto output = frameDescriptor.outputBuffer)
	{
		output.value().Bind();
	}

	m_renderer.SetViewport(0, 0, frameDescriptor.renderWidth, frameDescriptor.renderHeight);
}

void OvCore::Rendering::ShadowRenderPass::_DrawShadows(
	OvRendering::Data::PipelineState p_pso,
	const OvRendering::Data::Frustum& p_lightFrustum
)
{
	using namespace OvCore::Rendering;

	OVASSERT(m_renderer.HasDescriptor<SceneRenderer::SceneDrawablesDescriptor>(), "Cannot find SceneDrawablesDescriptor attached to this renderer");

	// Reuse the drawables parsed for this frame instead of walking the scene again.
	const auto& sceneDrawables = m_renderer.GetDescriptor<SceneRenderer::SceneDrawablesDescriptor>();

	for (const auto& drawable : sceneDrawables.drawables)
	{
		const auto& desc = drawable.GetDescriptor<SceneRenderer::SceneDrawableDescriptor>();

		if (!SatisfiesVisibility(desc.visibilityFlags, EVisibilityFlags::SHADOW))
		{
			continue;
		}

		if (!drawable.material || !drawable.material->IsValid() || !drawable.material->IsShadowCaster())
		{
			continue;
		}

		auto& material = drawable.material.value();

		OvTools::Utils::OptRef<const SkinningDrawableDescriptor> skinningDescriptor;
		const bool hasSkinningDescriptor = drawable.TryGetDescriptor<SkinningDrawableDescriptor>(skinningDescriptor);

		// Casters outside of the light frustum cannot contribute to the shadow map.
		if (desc.bounds.has_value())
		{
			auto cullingBounds = desc.bounds.value();

			if (hasSkinningDescriptor)
			{
				cullingBounds.radius *= skinningDescriptor->boundsScale;
			}

			if (!p_lightFrustum.BoundingSphereInFrustum(cullingBounds, desc.actor.transform.GetFTransform()))
			{
				continue;
			}
		}

		// If the material has a shadow pass, use it. Otherwise, use the shadow fallback.
		auto& targetMaterial =
			material.HasPass(kShadowPassName) ?
			material :
			m_shadowMaterial;

		OvRendering::Entities::Drawable shadowDrawable = drawable;
		shadowDrawable.material = targetMaterial;

		// Generate the state mask for the target material, and override
		// its properties to ensure the shadow pass is rendered correctly.
		shadowDrawable.stateMask = targetMaterial.GenerateStateMask();
		shadowDrawable.stateMask.blendable = false; // The shadow pass should never use blending.
		shadowDrawable.stateMask.depthTest = true; // The shadow pass should always use depth test.
		shadowDrawable.stateMask.colorWriting = false; // The shadow pass should never write color.
		shadowDrawable.stateMask.depthWriting = true; // The shadow pass should always write depth.

		// No front/backface culling for shadow pass (aka: two-sided shadow pass).
		// A "two-sided" shadow pass setting could be added in the future to change this behavior.
		shadowDrawable.stateMask.frontfaceCulling = false;
		shadowDrawable.stateMask.backfaceCulling = false;

		shadowDrawable.pass = kShadowPassName;

		// Skinning is only applied if the original material explicitly supports it.
		const bool skinningEnabled =
			hasSkinningDescriptor &&
			material.SupportsFeature(kSkinningFeatureName) &&
			targetMaterial.SupportsFeature(kSkinningFeatureName);

		shadowDrawable.featureSetOverride =
			skinningEnabled ?
			std::make_optional(SkinningUtils::BuildFeatureSet(&targetMaterial.GetFeatures())) :
			std::nullopt;

		m_renderer.DrawEntity(p_pso, shadowDrawable);
	}
}
