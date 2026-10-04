/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cstring>

#include <tracy/Tracy.hpp>

#include <OvDebug/Assertion.h>
#include <OvRendering/Utils/StreamingBuffer.h>

namespace
{
	constexpr uint64_t AlignUp(uint64_t p_value, uint64_t p_alignment)
	{
		return (p_value + p_alignment - 1) / p_alignment * p_alignment;
	}
}

OvRendering::Utils::StreamingBuffer::StreamingBuffer(uint64_t p_initialFrameCapacity)
{
	Allocate(AlignUp(std::max<uint64_t>(p_initialFrameCapacity, kAlignment), kAlignment));
}

void OvRendering::Utils::StreamingBuffer::BeginFrame()
{
	ZoneScoped;

	m_frameIndex = (m_frameIndex + 1) % kFrameCount;
	m_head = 0;

	// Usually a no-op: the GPU is at most a couple of frames behind
	m_fences[m_frameIndex].Wait();
}

void OvRendering::Utils::StreamingBuffer::EndFrame()
{
	ZoneScoped;

	m_fences[m_frameIndex].Insert();
}

baregl::data::BufferMemoryRange OvRendering::Utils::StreamingBuffer::Push(const void* p_data, uint64_t p_size)
{
	OVASSERT(p_size > 0, "Cannot push empty data");

	if (m_head + p_size > m_frameCapacity)
	{
		// Replacing the buffer is safe: the GL keeps the previous one alive until
		// the commands using it are executed. The new buffer isn't used by the GPU yet,
		// so the current frame can start again from the beginning of its region.
		Allocate(AlignUp(std::max(m_frameCapacity * 2, p_size), kAlignment));
		m_head = 0;
	}

	const uint64_t offset = static_cast<uint64_t>(m_frameIndex) * m_frameCapacity + m_head;
	std::memcpy(m_mappedMemory + offset, p_data, p_size);
	m_head = AlignUp(m_head + p_size, kAlignment);

	return { offset, p_size };
}

baregl::Buffer& OvRendering::Utils::StreamingBuffer::GetBuffer()
{
	return *m_buffer;
}

uint64_t OvRendering::Utils::StreamingBuffer::GetGeneration() const
{
	return m_generation;
}

void OvRendering::Utils::StreamingBuffer::Allocate(uint64_t p_frameCapacity)
{
	++m_generation;
	m_frameCapacity = p_frameCapacity;
	m_buffer = std::make_unique<baregl::Buffer>();
	m_mappedMemory = static_cast<std::byte*>(m_buffer->AllocatePersistentlyMapped(m_frameCapacity * kFrameCount));

	OVASSERT(m_mappedMemory != nullptr, "Failed to map the streaming buffer");

	// The fences were guarding the previous buffer
	for (auto& fence : m_fences)
	{
		fence.Reset();
	}
}
