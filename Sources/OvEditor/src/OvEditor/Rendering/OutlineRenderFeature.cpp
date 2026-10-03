/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <functional>
#include <string>

#include <OvCore/Rendering/EngineDrawableDescriptor.h>
#include <OvCore/Rendering/FrameBuilder.h>
#include <OvCore/Rendering/SceneDrawableDescriptor.h>
#include <OvCore/Rendering/SceneRenderer.h>
#include <OvEditor/Core/EditorActions.h>
#include <OvEditor/Rendering/DebugModelRenderFeature.h>
#include <OvEditor/Rendering/OutlineRenderFeature.h>
#include <OvEditor/Settings/EditorSettings.h>
#include <OvRendering/Utils/Conversions.h>

namespace
{
	constexpr uint32_t kStencilMask = 0xFF;
	constexpr int32_t kStencilReference = 1;
	constexpr std::string_view kOutlinePassName = "OUTLINE_PASS";

	void DrawActorModels(
		OvRendering::Core::CompositeRenderer& p_renderer,
		OvRendering::Data::PipelineState p_pso,
		const OvCore::ECS::Actor& p_actor,
		OvCore::Resources::Material& p_material,
		const std::function<void(OvRendering::Entities::Drawable&)>& p_customPreparation
	)
	{
		using namespace OvCore::Rendering;

		const auto& sceneDescriptor = p_renderer.GetDescriptor<SceneRenderer::SceneDescriptor>();

		const auto filteringResult = FrameBuilder::Filter(
			p_renderer.GetDescriptor<FrameBuilder::ParsingResult>(),
			FrameBuilder::FilteringInput{
				.camera = p_renderer.GetFrameDescriptor().camera.value(),
				.frustumOverride = sceneDescriptor.frustumOverride,
				.fallbackMaterial = p_material, // Drawables without material are outlined using the given material
				.filter = [&p_actor](const OvRendering::Entities::Drawable& p_drawable, const OvRendering::Data::Material&) {
					// Only keep the drawables of the given actor and its descendants
					const auto& actor = p_drawable.GetDescriptor<SceneDrawableDescriptor>().actor;
					return &actor == &p_actor || actor.IsDescendantOf(&p_actor);
				}
			}
		);

		FrameBuilder::Draw(p_renderer, p_pso, filteringResult, FrameBuilder::PreparationInput{
			.pass = std::string{ kOutlinePassName },
			.passFallbackMaterial = p_material,
			.customPreparation = p_customPreparation
		});
	}
}

OvEditor::Rendering::OutlineRenderFeature::OutlineRenderFeature(
	OvRendering::Core::CompositeRenderer& p_renderer,
	OvRendering::Features::EFeatureExecutionPolicy p_executionPolicy
) :
	OvRendering::Features::ARenderFeature(p_renderer, p_executionPolicy)
{
	/* Stencil Fill Material */
	m_stencilFillMaterial.SetShader(EDITOR_CONTEXT(editorResources)->GetShader("OutlineFallback"));

	/* Outline Material */
	m_outlineMaterial.SetShader(EDITOR_CONTEXT(editorResources)->GetShader("OutlineFallback"));
}

void OvEditor::Rendering::OutlineRenderFeature::DrawOutline(
	OvCore::ECS::Actor& p_actor,
	const OvMaths::FVector4& p_color,
	float p_thickness
)
{
	DrawStencilPass(p_actor);
	DrawOutlinePass(p_actor, p_color, p_thickness);
}

void OvEditor::Rendering::OutlineRenderFeature::DrawStencilPass(OvCore::ECS::Actor& p_actor)
{
	auto pso = m_renderer.CreatePipelineState();

	pso.stencilTest = true;
	pso.stencilWriteMask = kStencilMask;
	pso.stencilFuncRef = kStencilReference;
	pso.stencilFuncMask = kStencilMask;
	pso.stencilOpFail = baregl::types::EOperation::REPLACE;
	pso.depthOpFail = baregl::types::EOperation::REPLACE;
	pso.bothOpFail = baregl::types::EOperation::REPLACE;
	pso.colorWriting.mask = 0x00;

	DrawActorModels(m_renderer, pso, p_actor, m_stencilFillMaterial, [](OvRendering::Entities::Drawable& p_drawable) {
		p_drawable.stateMask.depthTest = false;
		p_drawable.stateMask.colorWriting = false;
	});

	DrawActorToStencil(pso, p_actor);
}

void OvEditor::Rendering::OutlineRenderFeature::DrawOutlinePass(OvCore::ECS::Actor& p_actor, const OvMaths::FVector4& p_color, float p_thickness)
{
	auto pso = m_renderer.CreatePipelineState();

	pso.stencilTest = true;
	pso.stencilOpFail = baregl::types::EOperation::KEEP;
	pso.depthOpFail = baregl::types::EOperation::KEEP;
	pso.bothOpFail = baregl::types::EOperation::REPLACE;
	pso.stencilFuncOp = baregl::types::EComparaisonAlgorithm::NOTEQUAL;
	pso.stencilFuncRef = kStencilReference;
	pso.stencilFuncMask = kStencilMask;
	pso.rasterizationMode = baregl::types::ERasterizationMode::LINE;
	pso.lineWidthPow2 = OvRendering::Utils::Conversions::FloatToPow2(p_thickness);

	DrawActorModels(m_renderer, pso, p_actor, m_outlineMaterial, [&p_color](OvRendering::Entities::Drawable& p_drawable) {
		p_drawable.stateMask.depthTest = false;

		// Set the outline color property if it exists
		auto& material = p_drawable.material.value();
		if (material.GetProperty("_OutlineColor"))
		{
			material.SetProperty("_OutlineColor", p_color, true);
		}
	});

	DrawActorOutline(pso, p_actor, p_color);
}

void OvEditor::Rendering::OutlineRenderFeature::DrawActorToStencil(OvRendering::Data::PipelineState p_pso, OvCore::ECS::Actor& p_actor)
{
	if (p_actor.IsActive())
	{
		/* Render camera component outline */
		if (auto cameraComponent = p_actor.GetComponent<OvCore::ECS::Components::CCamera>(); cameraComponent)
		{
			auto translation = OvMaths::FMatrix4::Translation(p_actor.transform.GetWorldPosition());
			auto rotation = OvMaths::FQuaternion::ToMatrix4(p_actor.transform.GetWorldRotation());
			auto model = translation * rotation;
			DrawModelToStencil(p_pso, model, *EDITOR_CONTEXT(editorResources)->GetModel("Camera"));
		}

		if (auto reflectionProbeComponent = p_actor.GetComponent<OvCore::ECS::Components::CReflectionProbe>(); reflectionProbeComponent)
		{
			const auto translation = OvMaths::FMatrix4::Translation(
				p_actor.transform.GetWorldPosition() +
				reflectionProbeComponent->GetCapturePosition()
			);
			const auto rotation = OvMaths::FQuaternion::ToMatrix4(p_actor.transform.GetWorldRotation());
			const auto scale = OvMaths::FMatrix4::Scaling(
				OvMaths::FVector3::One * OvEditor::Settings::EditorSettings::ReflectionProbeScale
			);
			const auto model = translation * rotation * scale;
			DrawModelToStencil(p_pso, model, *EDITOR_CONTEXT(editorResources)->GetModel("Sphere"));
		}

		for (auto& child : p_actor.GetChildren())
		{
			DrawActorToStencil(p_pso, *child);
		}
	}
}

void OvEditor::Rendering::OutlineRenderFeature::DrawActorOutline(
	OvRendering::Data::PipelineState p_pso,
	OvCore::ECS::Actor& p_actor,
	const OvMaths::FVector4& p_color
)
{
	if (p_actor.IsActive())
	{
		if (auto cameraComponent = p_actor.GetComponent<OvCore::ECS::Components::CCamera>(); cameraComponent)
		{
			auto translation = OvMaths::FMatrix4::Translation(p_actor.transform.GetWorldPosition());
			auto rotation = OvMaths::FQuaternion::ToMatrix4(p_actor.transform.GetWorldRotation());
			auto model = translation * rotation;
			DrawModelOutline(p_pso, model, *EDITOR_CONTEXT(editorResources)->GetModel("Camera"), p_color);
		}

		if (auto reflectionProbeComponent = p_actor.GetComponent<OvCore::ECS::Components::CReflectionProbe>(); reflectionProbeComponent)
		{
			const auto translation = OvMaths::FMatrix4::Translation(
				p_actor.transform.GetWorldPosition() +
				reflectionProbeComponent->GetCapturePosition()
			);
			const auto rotation = OvMaths::FQuaternion::ToMatrix4(p_actor.transform.GetWorldRotation());
			const auto scale = OvMaths::FMatrix4::Scaling(
				OvMaths::FVector3::One * OvEditor::Settings::EditorSettings::ReflectionProbeScale
			);
			const auto model = translation * rotation * scale;
			DrawModelOutline(p_pso, model, *EDITOR_CONTEXT(editorResources)->GetModel("Sphere"), p_color);
		}

		for (auto& child : p_actor.GetChildren())
		{
			DrawActorOutline(p_pso, *child, p_color);
		}
	}
}

void OvEditor::Rendering::OutlineRenderFeature::DrawModelToStencil(
	OvRendering::Data::PipelineState p_pso,
	const OvMaths::FMatrix4& p_worldMatrix,
	OvRendering::Resources::Model& p_model
)
{
	const std::string outlinePassName{ kOutlinePassName };

	for (auto mesh : p_model.GetMeshes())
	{
		auto stateMask = m_stencilFillMaterial.GenerateStateMask();

		auto engineDrawableDescriptor = OvCore::Rendering::EngineDrawableDescriptor{
			p_worldMatrix,
			OvMaths::FMatrix4::Identity
		};

		OvRendering::Entities::Drawable element;
		element.mesh = *mesh;
		element.material = m_stencilFillMaterial;
		element.stateMask = stateMask;
		element.stateMask.depthTest = false;
		element.stateMask.colorWriting = false;
		element.pass = outlinePassName;

		element.AddDescriptor(engineDrawableDescriptor);

		m_renderer.DrawEntity(p_pso, element);
	}
}

void OvEditor::Rendering::OutlineRenderFeature::DrawModelOutline(
	OvRendering::Data::PipelineState p_pso,
	const OvMaths::FMatrix4& p_worldMatrix,
	OvRendering::Resources::Model& p_model,
	const OvMaths::FVector4& p_color
)
{
	const std::string outlinePassName{ kOutlinePassName };

	for (auto mesh : p_model.GetMeshes())
	{
		// Set the outline color property if it exists
		if (m_outlineMaterial.GetProperty("_OutlineColor"))
		{
			m_outlineMaterial.SetProperty("_OutlineColor", p_color, true);
		}

		auto stateMask = m_outlineMaterial.GenerateStateMask();

		auto engineDrawableDescriptor = OvCore::Rendering::EngineDrawableDescriptor{
			p_worldMatrix,
			OvMaths::FMatrix4::Identity
		};

		OvRendering::Entities::Drawable drawable;
		drawable.mesh = *mesh;
		drawable.material = m_outlineMaterial;
		drawable.stateMask = stateMask;
		drawable.stateMask.depthTest = false;
		drawable.pass = outlinePassName;

		drawable.AddDescriptor(engineDrawableDescriptor);

		m_renderer.DrawEntity(p_pso, drawable);
	}
}
