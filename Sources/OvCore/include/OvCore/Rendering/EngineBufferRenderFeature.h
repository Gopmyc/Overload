/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <chrono>
#include <vector>

#include <OvMaths/FMatrix4.h>
#include <OvMaths/FVector3.h>
#include <OvMaths/FVector4.h>
#include <OvCore/Rendering/FogSettings.h>
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
		// Shader storage binding point of the instances buffer (see Common/Buffers/InstancesSSBO.ovfxh)
		static constexpr uint32_t kInstancesBufferBindingPoint = 2;

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

		/**
		* Defines the distance fog of the next draws, exposed to shaders as ubo_Fog*. Draws with their own camera
		* (e.g. screen space UI) are never fogged
		* @param p_settings
		*/
		void SetFog(const FogSettings& p_settings);

	protected:
		virtual void OnBeginFrame(const OvRendering::Data::FrameDescriptor& p_frameDescriptor) override;
		virtual void OnEndFrame() override;
		virtual void OnBeforeDraw(OvRendering::Data::PipelineState& p_pso, const OvRendering::Entities::Drawable& p_drawable) override;

	protected:
		/**
		* Camera matrices, as provided by the camera (not transposed)
		*/
		struct CameraData
		{
			OvMaths::FMatrix4 view;
			OvMaths::FMatrix4 projection;
			OvMaths::FVector3 position;
		};

		/**
		* Writes the given camera data to the engine UBO
		* @param p_camera
		*/
		void ApplyCamera(const CameraData& p_camera);

		/**
		* Turns the fog on in the engine UBO while it is enabled and no camera override is active
		*/
		void ApplyFogState();

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
			float fogStart;
			float fogDensity;
			float fogMax;
			OvMaths::FVector4 fogColor; // w: 1 while the fog is on
			float gameTime;
			int32_t instanceCount; // Instances of the current instanced draw, 0 for any other draw
			float padding[2]; // std140 rounds the block up to a multiple of 16 bytes
		};

		/**
		* Matrices of one instance of an instanced draw (std430 layout, see InstancesSSBO.ovfxh)
		*/
		struct InstanceData
		{
			OvMaths::FMatrix4 model;
			OvMaths::FMatrix4 normalMatrix;
			OvMaths::FMatrix4 userMatrix;
		};

		std::chrono::high_resolution_clock::time_point m_startTime;

		// Each draw gets its own copy of the UBO in a streaming buffer, so updating
		// the data never has to wait for (or stall on) previous draws still in flight.
		OvRendering::Utils::StreamingBuffer m_engineBuffer;
		EngineUBO m_data{};
		bool m_dirty = true;

		// Instances of the instanced draws (see InstancingDrawableDescriptor), streamed like the UBO
		OvRendering::Utils::StreamingBuffer m_instancesBuffer;
		std::vector<InstanceData> m_instances;

		// Camera set for the frame (or with SetCamera), restored after draws overriding the camera
		// (see EngineDrawableDescriptor::viewMatrixOverride and projectionMatrixOverride)
		CameraData m_frameCamera{};
		bool m_cameraOverrideActive = false;

		bool m_fogEnabled = false;
	};
}
