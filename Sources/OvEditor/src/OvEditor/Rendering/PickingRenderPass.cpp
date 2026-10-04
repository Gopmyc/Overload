/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <ranges>
#include <string>

#include <OvCore/ECS/Components/CMaterialRenderer.h>
#include <OvCore/Rendering/EngineDrawableDescriptor.h>
#include <OvCore/Rendering/FrameBuilder.h>
#include <OvCore/Rendering/FramebufferUtil.h>
#include <OvCore/Rendering/SceneDrawableDescriptor.h>
#include <OvCore/Rendering/UIRenderingUtils.h>

#include <OvEditor/Core/EditorActions.h>
#include <OvEditor/Rendering/DebugModelRenderFeature.h>
#include <OvEditor/Rendering/DebugSceneRenderer.h>
#include <OvEditor/Rendering/PickingRenderPass.h>
#include <OvEditor/Settings/EditorSettings.h>

#include <OvRendering/Utils/Profiling.h>

namespace
{
	const std::string kPickingPassName = "PICKING_PASS";
	constexpr float kDistanceBasedGizmoScale = -1.0f;
	constexpr float kUIScreenSpaceGizmoScale = 80.0f;
	constexpr float kUIScreenSpaceGizmoDepth = 1000.0f;
	constexpr const char* kGizmoScaleUniform = "u_GizmoScale";
	constexpr const char* kVisibleAxesUniform = "u_VisibleAxes";

	/**
	* Returns a frustum only covering a small region around the given pixel (similar to gluPickMatrix)
	*/
	OvRendering::Data::Frustum CalculatePickingFrustum(
		const OvRendering::Entities::Camera& p_camera,
		uint32_t p_x,
		uint32_t p_y,
		uint32_t p_width,
		uint32_t p_height
	)
	{
		constexpr float kRegionSize = 3.0f; // In pixels, a bit larger than the picked pixel to stay conservative

		const float width = static_cast<float>(std::max(p_width, 1u));
		const float height = static_cast<float>(std::max(p_height, 1u));
		const float scaleX = kRegionSize / width;
		const float scaleY = kRegionSize / height;
		const float centerX = 2.0f * (static_cast<float>(p_x) + 0.5f) / width - 1.0f;
		const float centerY = 2.0f * (static_cast<float>(p_y) + 0.5f) / height - 1.0f;

		// Remaps the region around the pixel to the whole clip space
		OvMaths::FMatrix4 pickMatrix = OvMaths::FMatrix4::Identity;
		pickMatrix.data[0] = 1.0f / scaleX;
		pickMatrix.data[3] = -centerX / scaleX;
		pickMatrix.data[5] = 1.0f / scaleY;
		pickMatrix.data[7] = -centerY / scaleY;

		OvRendering::Data::Frustum frustum;
		frustum.CalculateFrustum(pickMatrix * p_camera.GetProjectionMatrix() * p_camera.GetViewMatrix());
		return frustum;
	}

	void PreparePickingMaterial(
		const OvCore::ECS::Actor& p_actor,
		OvRendering::Data::Material& p_material,
		const std::string& p_uniformName = "_PickingColor"
	)
	{
		uint32_t actorID = static_cast<uint32_t>(p_actor.GetID());

		auto bytes = reinterpret_cast<uint8_t*>(&actorID);
		auto color = OvMaths::FVector4{ bytes[0] / 255.0f, bytes[1] / 255.0f, bytes[2] / 255.0f, 1.0f };

		// Set the picking color property if it exists
		if (p_material.GetProperty(p_uniformName))
		{
			p_material.SetProperty(p_uniformName, color, true);
		}
	}

	bool TryGetUIActorGizmoTransform(
		const bool p_includeUI,
		const OvCore::Rendering::UIRenderingUtils::UIFrameResolver& p_uiFrameResolver,
		OvCore::ECS::Actor& p_actor,
		OvMaths::FVector3& p_position,
		OvMaths::FQuaternion& p_rotation,
		bool& p_screenSpace
	)
	{
		if (!p_includeUI)
		{
			return false;
		}

		OvCore::Rendering::UIRenderingUtils::ResolvedUIGizmoTransform resolvedTransform;
		if (!OvCore::Rendering::UIRenderingUtils::ResolveUIGizmoTransform(
			p_uiFrameResolver,
			p_actor,
			resolvedTransform
		))
		{
			return false;
		}

		p_position = resolvedTransform.position;
		p_rotation = resolvedTransform.rotation;
		p_screenSpace = resolvedTransform.screenSpace;
		return true;
	}

	bool ShouldPickWorldDebugElements(const OvRendering::Core::CompositeRenderer& p_renderer)
	{
		if (!p_renderer.HasDescriptor<OvCore::Rendering::SceneRenderer::SceneDescriptor>())
		{
			return true;
		}

		const auto& sceneDescriptor = p_renderer.GetDescriptor<OvCore::Rendering::SceneRenderer::SceneDescriptor>();
		return !sceneDescriptor.renderUIInScreenSpace;
	}
}

OvEditor::Rendering::PickingRenderPass::PickingRenderPass(OvRendering::Core::CompositeRenderer& p_renderer) :
	OvRendering::Core::ARenderPass(p_renderer),
	m_actorPickingFramebuffer("ActorPicking")
{
	// Actor IDs are encoded as 8-bit colors: RGBA8 stores them exactly
	OvCore::Rendering::FramebufferUtil::SetupFramebuffer(
		m_actorPickingFramebuffer, 1, 1, true, false, false,
		baregl::types::EInternalFormat::RGBA8
	);

	for (auto& readback : m_readbacks)
	{
		readback.buffer.Allocate(sizeof(uint32_t), baregl::types::EAccessSpecifier::STREAM_READ);
	}

	/* Light Material */
	m_lightMaterial.SetShader(EDITOR_CONTEXT(editorResources)->GetShader("Billboard"));
	m_lightMaterial.SetDepthTest(false);

	/* Gizmo Pickable Material */
	m_gizmoPickingMaterial.SetShader(EDITOR_CONTEXT(editorResources)->GetShader("Gizmo"));
	m_gizmoPickingMaterial.SetGPUInstances(3);
	m_gizmoPickingMaterial.SetProperty("u_IsBall", false);
	m_gizmoPickingMaterial.SetProperty("u_IsPickable", true);
	m_gizmoPickingMaterial.TrySetProperty(kGizmoScaleUniform, kDistanceBasedGizmoScale);
	m_gizmoPickingMaterial.TrySetProperty(kVisibleAxesUniform, OvEditor::Core::kGizmoAxisAll);
	m_gizmoPickingMaterial.SetDepthTest(true);

	m_reflectionProbeMaterial.SetShader(EDITOR_CONTEXT(editorResources)->GetShader("PickingFallback"));
	m_reflectionProbeMaterial.SetDepthTest(false);

	/* Picking Material */
	m_actorPickingFallbackMaterial.SetShader(EDITOR_CONTEXT(editorResources)->GetShader("PickingFallback"));
}

void OvEditor::Rendering::PickingRenderPass::SetPickingPosition(uint32_t p_x, uint32_t p_y)
{
	m_pickingPosition = { p_x, p_y };
}

void OvEditor::Rendering::PickingRenderPass::ResetPickingResult()
{
	for (auto& readback : m_readbacks)
	{
		readback.fence.Reset();
		readback.pending = false;
	}

	m_lastPickedPixel.reset();
}

OvEditor::Rendering::PickingRenderPass::PickingResult OvEditor::Rendering::PickingRenderPass::GetPickingResult(
	const OvCore::SceneSystem::Scene& p_scene
)
{
	// Consume the most recent readback the GPU is done with (from newest to oldest), without waiting.
	// Older readbacks are discarded.
	for (uint32_t i = 1; i <= kReadbackCount; ++i)
	{
		auto& readback = m_readbacks[(m_nextReadback + kReadbackCount - i) % kReadbackCount];

		if (readback.pending && readback.fence.IsSignaled())
		{
			uint8_t pixel[4] = {};
			readback.buffer.Download(pixel, baregl::data::BufferMemoryRange{ .offset = 0, .size = 3 });
			m_lastPickedPixel = std::array<uint8_t, 3>{ pixel[0], pixel[1], pixel[2] };

			for (auto& olderReadback : m_readbacks)
			{
				if (&olderReadback != &readback && olderReadback.pending && olderReadback.fence.IsSignaled())
				{
					olderReadback.fence.Reset();
					olderReadback.pending = false;
				}
			}

			readback.fence.Reset();
			readback.pending = false;
			break;
		}
	}

	if (!m_lastPickedPixel)
	{
		return std::nullopt;
	}

	const auto& pixel = m_lastPickedPixel.value();

	uint32_t actorID = (0 << 24) | (pixel[2] << 16) | (pixel[1] << 8) | (pixel[0] << 0);
	auto actorUnderMouse = p_scene.FindActorByID(actorID);

	if (actorUnderMouse)
	{
		return OvTools::Utils::OptRef(*actorUnderMouse);
	}
	else if (
		pixel[0] == 255 &&
		pixel[1] == 255 &&
		pixel[2] >= 252 &&
		pixel[2] <= 254
		)
	{
		return static_cast<OvEditor::Core::GizmoBehaviour::EDirection>(pixel[2] - 252);
	}

	return std::nullopt;
}

void OvEditor::Rendering::PickingRenderPass::Draw(OvRendering::Data::PipelineState p_pso)
{
	ZoneScoped;
	TracyGpuZone("PickingRenderPass");

	using namespace OvCore::Rendering;

	OVASSERT(m_renderer.HasDescriptor<SceneRenderer::SceneDescriptor>(), "Cannot find SceneDescriptor attached to this renderer");
	OVASSERT(m_renderer.HasDescriptor<DebugSceneRenderer::DebugSceneDescriptor>(), "Cannot find DebugSceneDescriptor attached to this renderer");

	auto& sceneDescriptor = m_renderer.GetDescriptor<SceneRenderer::SceneDescriptor>();
	auto& debugSceneDescriptor = m_renderer.GetDescriptor<DebugSceneRenderer::DebugSceneDescriptor>();
	auto& frameDescriptor = m_renderer.GetFrameDescriptor();
	auto& scene = sceneDescriptor.scene;
	const auto& uiFrameResolver = m_renderer.GetDescriptor<OvCore::Rendering::UIRenderingUtils::UIFrameResolver>();

	m_actorPickingFramebuffer.Resize(frameDescriptor.renderWidth, frameDescriptor.renderHeight);

	m_actorPickingFramebuffer.Bind();
	
	auto pso = m_renderer.CreatePipelineState();

	m_renderer.Clear(true, true, true);

	// Only the picked pixel is read back, so it is the only one that needs to be rendered
	const uint32_t pickingX = std::min(m_pickingPosition.first, std::max<uint32_t>(frameDescriptor.renderWidth, 1) - 1);
	const uint32_t pickingY = std::min(m_pickingPosition.second, std::max<uint32_t>(frameDescriptor.renderHeight, 1) - 1);
	pso.scissorTest = true;
	m_renderer.SetScissor(pickingX, pickingY, 1, 1);

	DrawPickableModels(pso, scene);

	if (ShouldPickWorldDebugElements(m_renderer))
	{
		DrawPickableCameras(pso, scene);
		DrawPickableReflectionProbes(pso, scene);
		DrawPickableLights(pso, scene);
	}

	// Clear depth, gizmos are rendered on top of everything else
	m_renderer.Clear(false, true, false);

	if (debugSceneDescriptor.selectedActor)
	{
		auto& selectedActor = debugSceneDescriptor.selectedActor.value();
		auto gizmoPosition = selectedActor.transform.GetWorldPosition();
		auto gizmoRotation = selectedActor.transform.GetWorldRotation();
		const bool pickWorldDebugElements = ShouldPickWorldDebugElements(m_renderer);
		bool uiGizmoScreenSpace = false;
		const bool hasUIGizmoTransform = TryGetUIActorGizmoTransform(
			sceneDescriptor.includeUI,
			uiFrameResolver,
			selectedActor,
			gizmoPosition,
			gizmoRotation,
			uiGizmoScreenSpace
		);
		std::optional<OvMaths::FMatrix4> gizmoViewMatrixOverride;
		std::optional<OvMaths::FMatrix4> gizmoProjectionMatrixOverride;
		std::optional<float> gizmoScaleOverride;
		int gizmoVisibleAxes = OvEditor::Core::kGizmoAxisAll;
		if (hasUIGizmoTransform)
		{
			gizmoVisibleAxes = OvEditor::Core::GetUIGizmoAxes(
				selectedActor,
				debugSceneDescriptor.gizmoOperation,
				uiGizmoScreenSpace
			);

			if (uiGizmoScreenSpace)
			{
				gizmoViewMatrixOverride = OvMaths::FMatrix4::Identity;
				gizmoProjectionMatrixOverride = uiFrameResolver.CreateProjectionMatrix(
					-kUIScreenSpaceGizmoDepth,
					kUIScreenSpaceGizmoDepth
				);
				gizmoScaleOverride = kUIScreenSpaceGizmoScale;
			}
		}

		if (pickWorldDebugElements || hasUIGizmoTransform)
		{
			DrawPickableGizmo(
				pso,
				gizmoPosition,
				gizmoRotation,
				debugSceneDescriptor.gizmoOperation,
				gizmoViewMatrixOverride,
				gizmoProjectionMatrixOverride,
				gizmoScaleOverride,
				gizmoVisibleAxes
			);
		}
	}

	// Copy the picked pixel to a buffer, to be downloaded once the GPU is done (see GetPickingResult)
	auto& readback = m_readbacks[m_nextReadback];
	m_actorPickingFramebuffer.ReadPixels(
		pickingX, pickingY, 1, 1,
		baregl::types::EPixelDataFormat::RGB,
		baregl::types::EPixelDataType::UNSIGNED_BYTE,
		readback.buffer
	);
	readback.fence.Insert();
	readback.pending = true;
	m_nextReadback = (m_nextReadback + 1) % kReadbackCount;

	m_actorPickingFramebuffer.Unbind();

	if (auto output = frameDescriptor.outputBuffer)
	{
		output.value().Bind();
	}
}

void OvEditor::Rendering::PickingRenderPass::DrawPickableModels(
	OvRendering::Data::PipelineState p_pso,
	OvCore::SceneSystem::Scene& p_scene
)
{
	using namespace OvCore::Rendering;

	const auto& filteringResult = m_renderer.GetDescriptor<FrameBuilder::FilteringResult>();
	const auto& frameDescriptor = m_renderer.GetFrameDescriptor();
	const auto pickingFrustum = CalculatePickingFrustum(
		frameDescriptor.camera.value(),
		m_pickingPosition.first,
		m_pickingPosition.second,
		frameDescriptor.renderWidth,
		frameDescriptor.renderHeight
	);

	const FrameBuilder::PreparationInput preparationInput{
		.pass = kPickingPassName,
		.passFallbackMaterial = m_actorPickingFallbackMaterial,
		.customPreparation = [](OvRendering::Entities::Drawable& p_drawable) {
			p_drawable.stateMask.frontfaceCulling = false;
			p_drawable.stateMask.backfaceCulling = false;

			const auto& actor = p_drawable.GetDescriptor<SceneDrawableDescriptor>().actor;
			PreparePickingMaterial(actor, p_drawable.material.value());
		}
	};

	auto drawPickableModels = [&](const auto& p_filteredDrawables) {
		for (const auto& filteredDrawable : p_filteredDrawables | std::views::values)
		{
			// Skip models that cannot cover the picked pixel
			if (FrameBuilder::IsInFrustum(filteredDrawable.drawable.get(), pickingFrustum))
			{
				m_renderer.DrawEntity(p_pso, FrameBuilder::Prepare(filteredDrawable, preparationInput));
			}
		}
	};

	drawPickableModels(filteringResult.opaques);
	drawPickableModels(filteringResult.transparents);
	drawPickableModels(filteringResult.ui);
}

void OvEditor::Rendering::PickingRenderPass::DrawPickableCameras(
	OvRendering::Data::PipelineState p_pso,
	OvCore::SceneSystem::Scene& p_scene
)
{
	for (auto camera : p_scene.GetFastAccessComponents().cameras)
	{
		auto& actor = camera->owner;

		if (actor.IsActive())
		{
			PreparePickingMaterial(actor, m_actorPickingFallbackMaterial);
			auto& cameraModel = *EDITOR_CONTEXT(editorResources)->GetModel("Camera");
			auto translation = OvMaths::FMatrix4::Translation(actor.transform.GetWorldPosition());
			auto rotation = OvMaths::FQuaternion::ToMatrix4(actor.transform.GetWorldRotation());
			auto modelMatrix = translation * rotation;

			m_renderer.GetFeature<DebugModelRenderFeature>()
				.DrawModelWithSingleMaterial(p_pso, cameraModel, m_actorPickingFallbackMaterial, modelMatrix);
		}
	}
}

void OvEditor::Rendering::PickingRenderPass::DrawPickableReflectionProbes(OvRendering::Data::PipelineState p_pso, OvCore::SceneSystem::Scene& p_scene)
{
	for (auto reflectionProbe : p_scene.GetFastAccessComponents().reflectionProbes)
	{
		auto& actor = reflectionProbe->owner;

		if (actor.IsActive())
		{
			PreparePickingMaterial(actor, m_reflectionProbeMaterial);
			auto& reflectionProbeModel = *EDITOR_CONTEXT(editorResources)->GetModel("Sphere");
			const auto translation = OvMaths::FMatrix4::Translation(
				actor.transform.GetWorldPosition() +
				reflectionProbe->GetCapturePosition()
			);
			const auto rotation = OvMaths::FQuaternion::ToMatrix4(actor.transform.GetWorldRotation());
			const auto scaling = OvMaths::FMatrix4::Scaling(
				OvMaths::FVector3::One * OvEditor::Settings::EditorSettings::ReflectionProbeScale
			);
			auto modelMatrix = translation * rotation * scaling;

			m_renderer.GetFeature<DebugModelRenderFeature>()
				.DrawModelWithSingleMaterial(p_pso, reflectionProbeModel, m_reflectionProbeMaterial, modelMatrix);
		}
	}
}

void OvEditor::Rendering::PickingRenderPass::DrawPickableLights(
	OvRendering::Data::PipelineState p_pso,
	OvCore::SceneSystem::Scene& p_scene
)
{
	if (Settings::EditorSettings::LightBillboardScale > 0.001f)
	{
		m_renderer.Clear(false, true, false);

		m_lightMaterial.SetProperty("u_Scale", Settings::EditorSettings::LightBillboardScale * 0.1f);

		for (auto light : p_scene.GetFastAccessComponents().lights)
		{
			auto& actor = light->owner;

			if (actor.IsActive())
			{
				PreparePickingMaterial(actor, m_lightMaterial, "u_Diffuse");
				auto& lightModel = *EDITOR_CONTEXT(editorResources)->GetModel("Vertical_Plane");
				auto modelMatrix = OvMaths::FMatrix4::Translation(actor.transform.GetWorldPosition());

				m_renderer.GetFeature<DebugModelRenderFeature>()
					.DrawModelWithSingleMaterial(p_pso, lightModel, m_lightMaterial, modelMatrix);
			}
		}
	}
}

void OvEditor::Rendering::PickingRenderPass::DrawPickableGizmo(
	OvRendering::Data::PipelineState p_pso,
	const OvMaths::FVector3& p_position,
	const OvMaths::FQuaternion& p_rotation,
	OvEditor::Core::EGizmoOperation p_operation,
	std::optional<OvMaths::FMatrix4> p_viewMatrixOverride,
	std::optional<OvMaths::FMatrix4> p_projectionMatrixOverride,
	std::optional<float> p_scaleOverride,
	int p_visibleAxes
)
{
	m_gizmoPickingMaterial.TrySetProperty(kGizmoScaleUniform, p_scaleOverride.value_or(kDistanceBasedGizmoScale));
	m_gizmoPickingMaterial.TrySetProperty(kVisibleAxesUniform, p_visibleAxes);

	auto modelMatrix =
		OvMaths::FMatrix4::Translation(p_position) *
		OvMaths::FQuaternion::ToMatrix4(OvMaths::FQuaternion::Normalize(p_rotation));

	auto arrowModel = EDITOR_CONTEXT(editorResources)->GetModel("Arrow_Picking");

	m_renderer.GetFeature<DebugModelRenderFeature>()
		.DrawModelWithSingleMaterial(
			p_pso,
			*arrowModel,
			m_gizmoPickingMaterial,
			modelMatrix,
			p_viewMatrixOverride,
			p_projectionMatrixOverride
		);
}
