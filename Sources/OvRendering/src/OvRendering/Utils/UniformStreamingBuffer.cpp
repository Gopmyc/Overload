/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <OvDebug/Assertion.h>

#include <OvRendering/Utils/UniformStreamingBuffer.h>

namespace
{
	uint64_t AlignUp(uint64_t p_value, uint64_t p_alignment)
	{
		return (p_value + p_alignment - 1) / p_alignment * p_alignment;
	}
}

OvRendering::Utils::UniformStreamingBuffer::UniformStreamingBuffer(uint64_t p_pageSize) :
	m_pageSize(p_pageSize),
	m_alignment(baregl::Buffer::GetUniformBufferOffsetAlignment())
{
}

OvRendering::Utils::UniformStreamingBuffer::Allocation OvRendering::Utils::UniformStreamingBuffer::Allocate(uint64_t p_size)
{
	OVASSERT(p_size <= m_pageSize, "Allocation is larger than the streaming buffer page size");

	uint64_t offset = AlignUp(m_currentOffset, m_alignment);

	if (!m_currentPage || offset + p_size > m_pageSize)
	{
		AcquirePage();
		offset = 0;
	}

	m_currentOffset = offset + p_size;

	auto& buffer = *m_pages[m_currentPage.value()].buffer;

	return Allocation{
		.buffer = buffer,
		.offset = offset,
		.data = static_cast<uint8_t*>(buffer.GetMappedData()) + offset
	};
}

void OvRendering::Utils::UniformStreamingBuffer::AcquirePage()
{
	// Retire the current page: every command reading from it has already been submitted,
	// so it can be reused once the GPU reaches this fence.
	if (m_currentPage)
	{
		m_pages[m_currentPage.value()].fence = std::make_unique<baregl::Fence>();
		m_inFlightPages.push_back(m_currentPage.value());
		m_currentPage.reset();
	}

	// Recycle the pages the GPU is done with (they complete in submission order)
	while (!m_inFlightPages.empty() && m_pages[m_inFlightPages.front()].fence->IsSignaled())
	{
		m_pages[m_inFlightPages.front()].fence.reset();
		m_freePages.push_back(m_inFlightPages.front());
		m_inFlightPages.pop_front();
	}

	// No page available: create a new one instead of waiting for the GPU
	if (m_freePages.empty())
	{
		auto buffer = std::make_unique<baregl::Buffer>();
		buffer->AllocatePersistent(m_pageSize);
		m_pages.push_back(Page{ std::move(buffer), nullptr });
		m_freePages.push_back(m_pages.size() - 1);
	}

	m_currentPage = m_freePages.back();
	m_freePages.pop_back();
	m_currentOffset = 0;
}
