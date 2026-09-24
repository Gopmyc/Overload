/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

#include <OvMaths/FMatrix4.h>
#include <OvMaths/FVector3.h>

#include <OvRendering/Features/ARenderFeature.h>
#include <OvRendering/Entities/Camera.h>

namespace OvCore::Rendering
{
	/**
	* Render feature handling engine buffer (UBO) updates.
	* Each draw gets its own slot in a persistently mapped ring buffer (bound with a range),
	* so updating per-draw data never has to wait for previous draws to complete.
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
		* Destructor
		*/
		virtual ~EngineBufferRenderFeature();

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
		// Mirrors the std140 "EngineUBO" block declared in EngineUBO.ovfxh
		struct EngineUBOData
		{
			OvMaths::FMatrix4 model;
			OvMaths::FMatrix4 view;
			OvMaths::FMatrix4 projection;
			OvMaths::FVector3 cameraPosition;
			float elapsedTime;
			OvMaths::FMatrix4 userMatrix;
		};

		static constexpr uint32_t kSegmentCount = 16;
		static constexpr uint32_t kSlotsPerSegment = 1024;

		void WriteAndBindCurrentData();
		void AdvanceSegment();

	protected:
		std::chrono::high_resolution_clock::time_point m_startTime;

	private:
		EngineUBOData m_data{};
		uint32_t m_bufferID = 0;
		std::byte* m_mappedMemory = nullptr;
		size_t m_slotStride = 0;
		uint32_t m_segment = 0;
		uint32_t m_slotInSegment = 0;
		std::array<void*, kSegmentCount> m_segmentFences{};
	};
}
