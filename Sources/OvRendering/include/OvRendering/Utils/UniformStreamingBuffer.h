/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <vector>

#include <baregl/Buffer.h>
#include <baregl/Fence.h>

namespace OvRendering::Utils
{
	/**
	* Streams small blocks of uniform data (e.g. per-draw data) to the GPU.
	* Blocks are written into persistently mapped buffer pages, and are meant to be bound with Buffer::BindRange.
	* Unlike rewriting the same buffer between draw calls (glBufferSubData), this never forces the driver to
	* version the buffer or the CPU to wait: pages are only recycled once the GPU is done reading them.
	*/
	class UniformStreamingBuffer final
	{
	public:
		/**
		* A block of memory allocated in the streaming buffer
		*/
		struct Allocation
		{
			baregl::Buffer& buffer;
			uint64_t offset;
			void* data;
		};

		/**
		* Creates the streaming buffer
		* @param p_pageSize Size of each buffer page in bytes (a single allocation cannot exceed it)
		*/
		UniformStreamingBuffer(uint64_t p_pageSize = 256 * 1024);

		/**
		* Allocates a block of memory, with an offset suitable for a uniform buffer range binding.
		* The returned memory can be written until the next draw call reading it is issued.
		* @param p_size
		*/
		Allocation Allocate(uint64_t p_size);

	private:
		void AcquirePage();

	private:
		struct Page
		{
			std::unique_ptr<baregl::Buffer> buffer;
			std::unique_ptr<baregl::Fence> fence;
		};

		const uint64_t m_pageSize;
		const uint64_t m_alignment;
		std::vector<Page> m_pages;
		std::deque<size_t> m_inFlightPages;
		std::vector<size_t> m_freePages;
		std::optional<size_t> m_currentPage;
		uint64_t m_currentOffset = 0;
	};
}
