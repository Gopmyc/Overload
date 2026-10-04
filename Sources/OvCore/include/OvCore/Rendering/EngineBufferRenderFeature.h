/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <chrono>
#include <map>
#include <stack>

#include <OvMaths/FMatrix4.h>
#include <OvMaths/FVector3.h>

#include <OvRendering/Features/ARenderFeature.h>
#include <OvRendering/Entities/Camera.h>
#include <OvRendering/Utils/UniformStreamingBuffer.h>

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

	protected:
		virtual void OnBeginFrame(const OvRendering::Data::FrameDescriptor& p_frameDescriptor) override;
		virtual void OnEndFrame() override;
		virtual void OnBeforeDraw(OvRendering::Data::PipelineState& p_pso, const OvRendering::Entities::Drawable& p_drawable) override;

	private:
		/**
		* Writes the current engine data into a new block of the streaming buffer, and binds it
		*/
		void UploadAndBind();

	protected:
		/**
		* CPU-side copy of the engine UBO (std140 layout, see EngineUBO.ovfxh)
		*/
		struct EngineUBO
		{
			OvMaths::FMatrix4 modelMatrix;
			OvMaths::FMatrix4 viewMatrix;
			OvMaths::FMatrix4 projectionMatrix;
			OvMaths::FVector3 cameraPosition;
			float elapsedTime;
			OvMaths::FMatrix4 userMatrix;
		};

		std::chrono::high_resolution_clock::time_point m_startTime;
		OvRendering::Utils::UniformStreamingBuffer m_engineBuffer;
		EngineUBO m_engineData;
	};
}
