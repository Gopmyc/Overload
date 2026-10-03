/**
* @project: baregl
* @author: Adrien Givry
* @licence: MIT
*/

#pragma once

#include <baregl/data/BufferMemoryRange.h>
#include <baregl/detail/NativeObject.h>
#include <baregl/types/EAccessSpecifier.h>
#include <baregl/types/EBufferType.h>

#include <optional>

namespace baregl
{
	/**
	* Represents a buffer, used to store data on the GPU
	*/
	class Buffer final : public detail::NativeObject
	{
	public:
		/**
		* Creates a buffer
		*/
		Buffer();

		/**
		* Destroys the buffer
		*/
		~Buffer();

		/**
		* Allocates memory for the buffer
		* @param p_size
		* @param p_usage
		* @return The size of the allocated memory in bytes
		*/
		uint64_t Allocate(uint64_t p_size, types::EAccessSpecifier p_usage = types::EAccessSpecifier::STATIC_DRAW);

		/**
		* Allocates immutable memory for the buffer, and keeps it persistently mapped (coherent).
		* The mapped memory can be accessed using GetMappedData().
		* @note The buffer cannot be reallocated afterwards (immutable storage)
		* @param p_size
		* @param p_readable If true, the memory is mapped for reading, otherwise for writing
		* @return The size of the allocated memory in bytes
		*/
		uint64_t AllocatePersistent(uint64_t p_size, bool p_readable = false);

		/**
		* Returns a pointer to the persistently mapped memory, or nullptr if the buffer isn't persistently mapped
		*/
		void* GetMappedData() const;

		/**
		* Uploads data to the buffer
		* @param p_data
		* @param p_range
		*/
		void Upload(const void* p_data, std::optional<data::BufferMemoryRange> p_range = std::nullopt);

		/**
		* Returns true if the buffer is valid (properly allocated)
		*/
		bool IsValid() const;

		/**
		* Returns true if the buffer is empty
		*/
		bool IsEmpty() const;

		/**
		* Returns the size of the allocated buffer in bytes
		*/
		uint64_t GetSize() const;

		/**
		* Binds the buffer
		* @param p_type Type of the buffer to bind
		* @param p_index (Optional) Index to bind the buffer to
		*/
		void Bind(
			types::EBufferType p_type,
			std::optional<uint32_t> p_index = std::nullopt
		);

		/**
		* Binds a range of the buffer to the given indexed binding point
		* @param p_type Type of the buffer to bind
		* @param p_index Index to bind the buffer to
		* @param p_range Range of the buffer to bind
		*/
		void BindRange(
			types::EBufferType p_type,
			uint32_t p_index,
			const data::BufferMemoryRange& p_range
		);

		/**
		* Unbinds the buffer
		*/
		void Unbind();

		/**
		* Returns the alignment required for the offset of a uniform buffer range (see BindRange)
		*/
		static uint32_t GetUniformBufferOffsetAlignment();

	protected:
		uint64_t m_allocatedBytes = 0;
		void* m_mappedData = nullptr;
		std::optional<types::EBufferType> m_boundAs = std::nullopt;
		std::optional<uint32_t> m_bindIndex = std::nullopt;
	};
}
