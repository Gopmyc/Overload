/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>

#include <OvRendering/Features/ARenderFeature.h>

namespace OvCore::Rendering
{
	/**
	* Render feature binding a particle system's buffer before each of its draws (Common/Particles.ovfxh reads it)
	*/
	class ParticleRenderFeature : public OvRendering::Features::ARenderFeature
	{
	public:
		static constexpr uint32_t kDefaultBufferBindingPoint = 3;

		/**
		* Constructor
		* @param p_renderer
		* @param p_executionPolicy
		* @param p_bufferBindingPoint
		*/
		ParticleRenderFeature(
			OvRendering::Core::CompositeRenderer& p_renderer,
			OvRendering::Features::EFeatureExecutionPolicy p_executionPolicy,
			uint32_t p_bufferBindingPoint = kDefaultBufferBindingPoint
		);

	protected:
		virtual void OnBeforeDraw(OvRendering::Data::PipelineState& p_pso, const OvRendering::Entities::Drawable& p_drawable) override;

	private:
		uint32_t m_bufferBindingPoint;
	};
}
