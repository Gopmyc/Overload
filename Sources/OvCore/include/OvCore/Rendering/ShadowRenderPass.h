/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstddef>
#include <vector>

#include <OvRendering/Entities/Camera.h>
#include <OvRendering/Entities/Light.h>
#include <OvRendering/Features/DebugShapeRenderFeature.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/SceneSystem/SceneManager.h>
#include <OvCore/ECS/Components/CModelRenderer.h>
#include <OvCore/Resources/Material.h>
#include <OvCore/ECS/Components/CAmbientBoxLight.h>
#include <OvCore/ECS/Components/CAmbientSphereLight.h>
#include <OvCore/Rendering/FrameBuilder.h>
#include <OvCore/Rendering/SceneRenderer.h>

namespace OvCore::Rendering
{
	/**
	* Draw the scene to a depth buffer from the point of view of each light source
	*/
	class ShadowRenderPass : public OvRendering::Core::ARenderPass
	{
	public:
		/**
		* Constructor
		* @param p_renderer
		*/
		ShadowRenderPass(OvRendering::Core::CompositeRenderer& p_renderer);

	private:
		virtual void Draw(OvRendering::Data::PipelineState p_pso) override;

		FrameBuilder::FilteringResult _FilterShadowCasters(const OvRendering::Entities::Camera& p_camera);

		/**
		* Describes everything the shadow map content depends on, in m_shadowMapSignature.
		* The shadow map is only rendered again when this description changes (see EShadowMapUpdateMode::ON_CHANGE).
		*/
		void _BuildShadowMapSignature(const OvRendering::Entities::Light& p_light, const FrameBuilder::FilteringResult& p_casters);

		void _DrawShadows(OvRendering::Data::PipelineState p_pso, const FrameBuilder::FilteringResult& p_casters);

	private:
		OvCore::Resources::Material m_shadowMaterial;

		// Reused every frame to avoid allocations
		std::vector<std::byte> m_shadowMapSignature;
		std::vector<OvRendering::Data::Material*> m_signatureMaterials;
	};
}