/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include <baregl/Buffer.h>
#include <baregl/Fence.h>

namespace OvRendering::Utils
{
	/**
	* Persistently mapped ring buffer, used to stream per-draw data to the GPU without
	* synchronizing with it. Each frame writes into its own region of the buffer, and a
	* region is only reused once the GPU is done with the frame that last used it.
	* Pushed data is written to a fresh (never in-flight) range, which can then be bound
	* with baregl::Buffer::Bind(type, index, range).
	*/
	class StreamingBuffer
	{
	public:
		/**
		* Constructor
		* @param p_initialFrameCapacity Initial capacity (in bytes) of each frame region. Grows when needed.
		*/
		StreamingBuffer(uint64_t p_initialFrameCapacity = 1024 * 1024);

		/**
		* Moves to the next frame region, waiting for the GPU to release it if needed
		*/
		void BeginFrame();

		/**
		* Marks the end of the frame, so its region can be reused once the GPU is done with it
		*/
		void EndFrame();

		/**
		* Copies the given data into the current frame region, and returns the range where it has been written.
		* The range offset is aligned to satisfy any buffer binding point offset alignment.
		* @note The underlying buffer can change when the region needs to grow, so always use GetBuffer() after pushing.
		* @param p_data
		* @param p_size
		*/
		baregl::data::BufferMemoryRange Push(const void* p_data, uint64_t p_size);

		/**
		* Returns the underlying buffer
		*/
		baregl::Buffer& GetBuffer();

		/**
		* Returns a counter incremented every time the underlying buffer is replaced (ranges returned
		* by Push() before that are only valid for the previous buffer)
		*/
		uint64_t GetGeneration() const;

	private:
		void Allocate(uint64_t p_frameCapacity);

	private:
		// Maximum value allowed by the specification for UNIFORM/SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT
		static constexpr uint64_t kAlignment = 256;
		static constexpr uint32_t kFrameCount = 3;

		std::unique_ptr<baregl::Buffer> m_buffer;
		std::byte* m_mappedMemory = nullptr;
		uint64_t m_frameCapacity = 0;
		uint32_t m_frameIndex = 0;
		uint64_t m_head = 0;
		uint64_t m_generation = 0;
		std::array<baregl::Fence, kFrameCount> m_fences;
	};
}
