/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cstring>

#include <tracy/Tracy.hpp>

#include <OvCore/Rendering/EngineBufferRenderFeature.h>
#include <OvCore/Rendering/EngineDrawableDescriptor.h>
#include <OvRendering/Core/CompositeRenderer.h>

namespace
{
	constexpr size_t kUBOSize =
		sizeof(OvMaths::FMatrix4) +	// Model matrix
		sizeof(OvMaths::FMatrix4) +	// View matrix
		sizeof(OvMaths::FMatrix4) +	// Projection matrix
		sizeof(OvMaths::FVector3) +	// Camera position
		sizeof(float) +				// Elapsed time
		sizeof(OvMaths::FMatrix4);	// User matrix

	constexpr uint32_t kUBOBindingPoint = 0;
}

OvCore::Rendering::EngineBufferRenderFeature::EngineBufferRenderFeature(
	OvRendering::Core::CompositeRenderer& p_renderer,
	OvRendering::Features::EFeatureExecutionPolicy p_executionPolicy
) : 
	ARenderFeature(p_renderer, p_executionPolicy),
	m_engineData{}
{
	static_assert(sizeof(EngineUBO) == kUBOSize, "EngineUBO must match the std140 layout of the engine UBO");
	m_startTime = std::chrono::high_resolution_clock::now();
}

void OvCore::Rendering::EngineBufferRenderFeature::SetCamera(const OvRendering::Entities::Camera& p_camera)
{
	m_engineData.viewMatrix = OvMaths::FMatrix4::Transpose(p_camera.GetViewMatrix());
	m_engineData.projectionMatrix = OvMaths::FMatrix4::Transpose(p_camera.GetProjectionMatrix());
	m_engineData.cameraPosition = p_camera.GetPosition();

	UploadAndBind();
}

void OvCore::Rendering::EngineBufferRenderFeature::OnBeginFrame(const OvRendering::Data::FrameDescriptor& p_frameDescriptor)
{
	OVASSERT(p_frameDescriptor.camera.has_value(), "Camera is not set in the frame descriptor");

	auto currentTime = std::chrono::high_resolution_clock::now();
	auto elapsedTime = std::chrono::duration_cast<std::chrono::duration<float>>(currentTime - m_startTime);

	m_engineData.viewMatrix = OvMaths::FMatrix4::Transpose(p_frameDescriptor.camera->GetViewMatrix());
	m_engineData.projectionMatrix = OvMaths::FMatrix4::Transpose(p_frameDescriptor.camera->GetProjectionMatrix());
	m_engineData.cameraPosition = p_frameDescriptor.camera->GetPosition();
	m_engineData.elapsedTime = elapsedTime.count();

	UploadAndBind();
}

void OvCore::Rendering::EngineBufferRenderFeature::OnEndFrame()
{
}

void OvCore::Rendering::EngineBufferRenderFeature::OnBeforeDraw(OvRendering::Data::PipelineState& p_pso, const OvRendering::Entities::Drawable& p_drawable)
{
	ZoneScoped;

	OvTools::Utils::OptRef<const EngineDrawableDescriptor> descriptor;

	if (p_drawable.TryGetDescriptor<EngineDrawableDescriptor>(descriptor))
	{
		const auto modelMatrix = OvMaths::FMatrix4::Transpose(descriptor->modelMatrix);

		// Consecutive draws often share the same matrices (e.g. the meshes of a model),
		// in which case the block that is already bound can be reused as is.
		const bool isSameData =
			std::memcmp(&modelMatrix, &m_engineData.modelMatrix, sizeof(OvMaths::FMatrix4)) == 0 &&
			std::memcmp(&descriptor->userMatrix, &m_engineData.userMatrix, sizeof(OvMaths::FMatrix4)) == 0;

		if (!isSameData)
		{
			m_engineData.modelMatrix = modelMatrix;
			m_engineData.userMatrix = descriptor->userMatrix;
			UploadAndBind();
		}
	}
}

void OvCore::Rendering::EngineBufferRenderFeature::UploadAndBind()
{
	// Each change is written to a new block of a persistently mapped buffer, and bound as a range.
	// Rewriting a single buffer with glBufferSubData between draw calls would force the driver
	// to version (copy) the buffer for every draw call.
	const auto allocation = m_engineBuffer.Allocate(sizeof(EngineUBO));
	std::memcpy(allocation.data, &m_engineData, sizeof(EngineUBO));

	allocation.buffer.BindRange(
		baregl::types::EBufferType::UNIFORM,
		kUBOBindingPoint,
		baregl::data::BufferMemoryRange{
			.offset = allocation.offset,
			.size = sizeof(EngineUBO)
		}
	);
}
