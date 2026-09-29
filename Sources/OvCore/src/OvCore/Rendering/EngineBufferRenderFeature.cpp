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
	constexpr uint32_t kEngineUBOBindingPoint = 0;
}

OvCore::Rendering::EngineBufferRenderFeature::EngineBufferRenderFeature(
	OvRendering::Core::CompositeRenderer& p_renderer,
	OvRendering::Features::EFeatureExecutionPolicy p_executionPolicy
) : 
	ARenderFeature(p_renderer, p_executionPolicy)
{
	static_assert(sizeof(EngineUBO) == 272, "EngineUBO must match the std140 layout of EngineUBO.ovfxh");
	m_startTime = std::chrono::high_resolution_clock::now();
}

void OvCore::Rendering::EngineBufferRenderFeature::SetCamera(const OvRendering::Entities::Camera& p_camera)
{
	m_data.view = OvMaths::FMatrix4::Transpose(p_camera.GetViewMatrix());
	m_data.projection = OvMaths::FMatrix4::Transpose(p_camera.GetProjectionMatrix());
	m_data.viewPos = p_camera.GetPosition();
	m_dirty = true;
}

void OvCore::Rendering::EngineBufferRenderFeature::OnBeginFrame(const OvRendering::Data::FrameDescriptor& p_frameDescriptor)
{
	OVASSERT(p_frameDescriptor.camera.has_value(), "Camera is not set in the frame descriptor");

	m_engineBuffer.BeginFrame();

	auto currentTime = std::chrono::high_resolution_clock::now();
	auto elapsedTime = std::chrono::duration_cast<std::chrono::duration<float>>(currentTime - m_startTime);

	SetCamera(p_frameDescriptor.camera.value());
	m_data.time = elapsedTime.count();
}

void OvCore::Rendering::EngineBufferRenderFeature::OnEndFrame()
{
	m_engineBuffer.EndFrame();
}

void OvCore::Rendering::EngineBufferRenderFeature::OnBeforeDraw(OvRendering::Data::PipelineState& p_pso, const OvRendering::Entities::Drawable& p_drawable)
{
	ZoneScoped;

	OvTools::Utils::OptRef<const EngineDrawableDescriptor> descriptor;

	if (p_drawable.TryGetDescriptor<EngineDrawableDescriptor>(descriptor))
	{
		const auto modelMatrix = OvMaths::FMatrix4::Transpose(descriptor->modelMatrix);

		// Consecutive draws often share the same matrices (e.g. meshes of the same model):
		// in that case, the currently bound data can be reused as is.
		if (m_dirty ||
			std::memcmp(&modelMatrix, &m_data.model, sizeof(modelMatrix)) != 0 ||
			std::memcmp(&descriptor->userMatrix, &m_data.userMatrix, sizeof(m_data.userMatrix)) != 0)
		{
			m_data.model = modelMatrix;
			m_data.userMatrix = descriptor->userMatrix;
			m_dirty = true;
		}
	}

	// Drawables without descriptor keep the previous model/user matrices (only the camera data may have changed)
	if (m_dirty)
	{
		const auto range = m_engineBuffer.Push(&m_data, sizeof(m_data));
		m_engineBuffer.GetBuffer().Bind(baregl::types::EBufferType::UNIFORM, kEngineUBOBindingPoint, range);
		m_dirty = false;
	}
}
