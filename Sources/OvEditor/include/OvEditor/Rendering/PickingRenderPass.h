/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CAmbientBoxLight.h>
#include <OvCore/ECS/Components/CAmbientSphereLight.h>
#include <OvCore/ECS/Components/CModelRenderer.h>
#include <OvCore/Resources/Material.h>
#include <OvCore/Rendering/SceneRenderer.h>
#include <OvCore/SceneSystem/SceneManager.h>

#include <OvEditor/Core/Context.h>
#include <OvEditor/Core/GizmoBehaviour.h>

#include <array>
#include <optional>
#include <utility>

#include <baregl/Buffer.h>
#include <baregl/Fence.h>

#include <OvRendering/Entities/Camera.h>
#include <OvRendering/Features/DebugShapeRenderFeature.h>

namespace OvEditor::Rendering
{
	/**
	* Draw the scene for actor picking
	*/
	class PickingRenderPass : public OvRendering::Core::ARenderPass
	{
	public:
		using PickingResult =
			std::optional<
			std::variant<OvTools::Utils::OptRef<OvCore::ECS::Actor>,
			OvEditor::Core::GizmoBehaviour::EDirection>
		>;

		/**
		* Constructor
		* @param p_renderer
		*/
		PickingRenderPass(OvRendering::Core::CompositeRenderer& p_renderer);

		/**
		* Sets the pixel to pick (framebuffer coordinates, origin at the bottom left).
		* Only this pixel is rendered by the picking pass.
		* @param p_x
		* @param p_y
		*/
		void SetPickingPosition(uint32_t p_x, uint32_t p_y);

		/**
		* Discards the pending and previous picking results (e.g. when the picking pass gets disabled)
		*/
		void ResetPickingResult();

		/**
		* Returns the result of the most recent picking readback completed by the GPU.
		* The GPU is never waited for, so the result usually comes from the previous frame.
		* Returns std::nullopt if nothing has been picked (or if no result is available yet).
		* @param p_scene
		*/
		PickingResult GetPickingResult(const OvCore::SceneSystem::Scene& p_scene);

	private:
		virtual void Draw(OvRendering::Data::PipelineState p_pso) override;
		void DrawPickableModels(OvRendering::Data::PipelineState p_pso, OvCore::SceneSystem::Scene& p_scene);
		void DrawPickableCameras(OvRendering::Data::PipelineState p_pso, OvCore::SceneSystem::Scene& p_scene);
		void DrawPickableReflectionProbes(OvRendering::Data::PipelineState p_pso, OvCore::SceneSystem::Scene& p_scene);
		void DrawPickableLights(OvRendering::Data::PipelineState p_pso, OvCore::SceneSystem::Scene& p_scene);
		void DrawPickableGizmo(
			OvRendering::Data::PipelineState p_pso,
			const OvMaths::FVector3& p_position,
			const OvMaths::FQuaternion& p_rotation,
			OvEditor::Core::EGizmoOperation p_operation
		);

	private:
		/**
		* Asynchronous readback of the picked pixel: the pixel is copied to a buffer on the GPU,
		* and only downloaded once a fence tells the copy is done (no CPU/GPU synchronization).
		*/
		struct PickingReadback
		{
			baregl::Buffer buffer;
			baregl::Fence fence;
			bool pending = false;
		};

		static constexpr uint32_t kReadbackCount = 3;

		baregl::Framebuffer m_actorPickingFramebuffer;
		std::array<PickingReadback, kReadbackCount> m_readbacks;
		uint32_t m_nextReadback = 0;
		std::optional<std::array<uint8_t, 3>> m_lastPickedPixel;
		std::pair<uint32_t, uint32_t> m_pickingPosition = { 0, 0 };
		OvCore::Resources::Material m_actorPickingFallbackMaterial;
		OvCore::Resources::Material m_reflectionProbeMaterial;
		OvCore::Resources::Material m_lightMaterial;
		OvCore::Resources::Material m_gizmoPickingMaterial;
	};
}
