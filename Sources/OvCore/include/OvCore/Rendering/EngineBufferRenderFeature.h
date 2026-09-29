/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <chrono>

#include <OvMaths/FMatrix4.h>
#include <OvMaths/FVector3.h>
#include <OvRendering/Features/ARenderFeature.h>
#include <OvRendering/Entities/Camera.h>
#include <OvRendering/Utils/StreamingBuffer.h>

namespace OvCore::Rendering
{
	/**
	* Render feature handling engine buffer (UBO) updates
	*/
	class EngineBufferRenderFeature : public OvRendering::Features::ARenderFeature
	{
	public:
		/**
		* Constructor
		* @param p_renderer
		* @param p_executionPolicy
		*/
		EngineBufferRenderFeature(
			OvRendering::Core::CompositeRenderer& p_renderer,
			OvRendering::Features::EFeatureExecutionPolicy p_executionPolicy
		);

		/**
		* Replace the current camera data in the engine buffer by the provided camera
		* @param p_camera
		*/
		void SetCamera(const OvRendering::Entities::Camera& p_camera);

		/**
		* Defines if the next draws only write depth (depth pre-pass), exposed to shaders as ubo_DepthOnly
		* @param p_depthOnly
		*/
		void SetDepthOnly(bool p_depthOnly);

	protected:
		virtual void OnBeginFrame(const OvRendering::Data::FrameDescriptor& p_frameDescriptor) override;
		virtual void OnEndFrame() override;
		virtual void OnBeforeDraw(OvRendering::Data::PipelineState& p_pso, const OvRendering::Entities::Drawable& p_drawable) override;

	protected:
		/**
		* CPU copy of the engine UBO (std140 layout, see EngineUBO.ovfxh)
		*/
		struct EngineUBO
		{
			OvMaths::FMatrix4 model;
			OvMaths::FMatrix4 view;
			OvMaths::FMatrix4 projection;
			OvMaths::FVector3 viewPos;
			float time;
			OvMaths::FMatrix4 userMatrix;
			OvMaths::FMatrix4 viewProjection;
			OvMaths::FMatrix4 normalMatrix;
			int32_t depthOnly;
			int32_t padding[3]; // std140 blocks are padded to 16 bytes
		};

		std::chrono::high_resolution_clock::time_point m_startTime;

		// Each draw gets its own copy of the UBO in a streaming buffer, so updating
		// the data never has to wait for (or stall on) previous draws still in flight.
		OvRendering::Utils::StreamingBuffer m_engineBuffer;
		EngineUBO m_data{};
		bool m_dirty = true;
	};
}
