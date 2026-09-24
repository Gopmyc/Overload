/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cstring>

#include <tracy/Tracy.hpp>

#include <baregl/detail/glad/glad.h>

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

	constexpr GLbitfield kMapFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
}

OvCore::Rendering::EngineBufferRenderFeature::EngineBufferRenderFeature(
	OvRendering::Core::CompositeRenderer& p_renderer,
	OvRendering::Features::EFeatureExecutionPolicy p_executionPolicy
) :
	ARenderFeature(p_renderer, p_executionPolicy)
{
	static_assert(sizeof(EngineUBOData) == kUBOSize, "EngineUBOData must match the std140 EngineUBO layout");

	GLint alignment = 256;
	glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
	m_slotStride = (kUBOSize + alignment - 1) / alignment * alignment;

	const auto bufferSize = static_cast<GLsizeiptr>(m_slotStride * kSlotsPerSegment * kSegmentCount);
	glCreateBuffers(1, &m_bufferID);
	glNamedBufferStorage(m_bufferID, bufferSize, nullptr, kMapFlags);
	m_mappedMemory = static_cast<std::byte*>(glMapNamedBufferRange(m_bufferID, 0, bufferSize, kMapFlags));

	m_startTime = std::chrono::high_resolution_clock::now();
}

OvCore::Rendering::EngineBufferRenderFeature::~EngineBufferRenderFeature()
{
	for (auto& fence : m_segmentFences)
	{
		if (fence)
		{
			glDeleteSync(static_cast<GLsync>(fence));
			fence = nullptr;
		}
	}

	if (m_bufferID)
	{
		glUnmapNamedBuffer(m_bufferID);
		glDeleteBuffers(1, &m_bufferID);
	}
}

void OvCore::Rendering::EngineBufferRenderFeature::AdvanceSegment()
{
	// Fence the segment we just filled, so we know when the GPU is done reading it
	auto& filledFence = m_segmentFences[m_segment];
	if (filledFence)
	{
		glDeleteSync(static_cast<GLsync>(filledFence));
	}
	filledFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

	m_segment = (m_segment + 1) % kSegmentCount;
	m_slotInSegment = 0;

	// Before overwriting the next segment, make sure the GPU is done with it (only blocks if the GPU is a whole ring behind)
	if (auto& nextFence = m_segmentFences[m_segment])
	{
		ZoneScopedN("Wait for engine UBO segment");

		while (glClientWaitSync(static_cast<GLsync>(nextFence), GL_SYNC_FLUSH_COMMANDS_BIT, 1'000'000) == GL_TIMEOUT_EXPIRED) {}
		glDeleteSync(static_cast<GLsync>(nextFence));
		nextFence = nullptr;
	}
}

void OvCore::Rendering::EngineBufferRenderFeature::WriteAndBindCurrentData()
{
	if (m_slotInSegment == kSlotsPerSegment)
	{
		AdvanceSegment();
	}

	const size_t offset = (static_cast<size_t>(m_segment) * kSlotsPerSegment + m_slotInSegment++) * m_slotStride;
	std::memcpy(m_mappedMemory + offset, &m_data, sizeof(m_data));
	glBindBufferRange(GL_UNIFORM_BUFFER, 0, m_bufferID, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(kUBOSize));
}

void OvCore::Rendering::EngineBufferRenderFeature::SetCamera(const OvRendering::Entities::Camera& p_camera)
{
	m_data.view = OvMaths::FMatrix4::Transpose(p_camera.GetViewMatrix());
	m_data.projection = OvMaths::FMatrix4::Transpose(p_camera.GetProjectionMatrix());
	m_data.cameraPosition = p_camera.GetPosition();

	// Make the new camera visible right away, even to draws that don't go through OnBeforeDraw
	WriteAndBindCurrentData();
}

void OvCore::Rendering::EngineBufferRenderFeature::OnBeginFrame(const OvRendering::Data::FrameDescriptor& p_frameDescriptor)
{
	OVASSERT(p_frameDescriptor.camera.has_value(), "Camera is not set in the frame descriptor");

	auto currentTime = std::chrono::high_resolution_clock::now();
	auto elapsedTime = std::chrono::duration_cast<std::chrono::duration<float>>(currentTime - m_startTime);

	m_data.view = OvMaths::FMatrix4::Transpose(p_frameDescriptor.camera->GetViewMatrix());
	m_data.projection = OvMaths::FMatrix4::Transpose(p_frameDescriptor.camera->GetProjectionMatrix());
	m_data.cameraPosition = p_frameDescriptor.camera->GetPosition();
	m_data.elapsedTime = elapsedTime.count();

	WriteAndBindCurrentData();
}

void OvCore::Rendering::EngineBufferRenderFeature::OnEndFrame()
{
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
}

void OvCore::Rendering::EngineBufferRenderFeature::OnBeforeDraw(OvRendering::Data::PipelineState& p_pso, const OvRendering::Entities::Drawable& p_drawable)
{
	ZoneScoped;

	OvTools::Utils::OptRef<const EngineDrawableDescriptor> descriptor;

	if (p_drawable.TryGetDescriptor<EngineDrawableDescriptor>(descriptor))
	{
		m_data.model = OvMaths::FMatrix4::Transpose(descriptor->modelMatrix);
		m_data.userMatrix = descriptor->userMatrix;

		// Each draw reads its own slot: no need to wait for previous draws to consume the buffer
		WriteAndBindCurrentData();
	}
}
