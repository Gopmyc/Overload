/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <OvRendering/Entities/Camera.h>
#include <OvRendering/Features/DebugShapeRenderFeature.h>
#include <OvRendering/Features/LightingRenderFeature.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/SceneSystem/SceneManager.h>
#include <OvCore/ECS/Components/CModelRenderer.h>
#include <OvCore/Resources/Material.h>
#include <OvCore/ECS/Components/CAmbientBoxLight.h>
#include <OvCore/ECS/Components/CAmbientSphereLight.h>
#include <OvCore/Rendering/SceneRenderer.h>

namespace OvCore::Rendering
{
	/**
	* Ensures drawable are properly setup to receive shadows by
	* providing them with the shadow map and light-space matrix.
	*/
	class ShadowRenderFeature : public OvRendering::Features::ARenderFeature
	{
	public:
		/**
		* Constructor
		* @param p_renderer
		* @param p_executionPolicy
		*/
		ShadowRenderFeature(
			OvRendering::Core::CompositeRenderer& p_renderer,
			OvRendering::Features::EFeatureExecutionPolicy p_executionPolicy
		);

		/**
		* Returns the light used for shadow rendering (the first directional light casting shadows).
		* Only one shadow map is currently supported.
		* @param p_lights
		*/
		static OvTools::Utils::OptRef<OvRendering::Entities::Light> FindShadowCastingLight(
			const OvRendering::Features::LightingRenderFeature::LightSet& p_lights
		);

	protected:
		virtual void OnBeginFrame(const OvRendering::Data::FrameDescriptor& p_frameDescriptor) override;
		virtual void OnEndFrame() override;
		virtual void OnBeforeDraw(OvRendering::Data::PipelineState& p_pso, const OvRendering::Entities::Drawable& p_drawable) override;

	private:
		OvTools::Utils::OptRef<const OvRendering::Entities::Light> m_shadowCastingLight;
		bool m_multipleShadowCastersWarned = false;
	};
}