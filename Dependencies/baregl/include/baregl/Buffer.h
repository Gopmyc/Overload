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
		* Allocates an immutable storage for the buffer, and maps it persistently (and coherently) for CPU writes.
		* The mapping stays valid for the whole lifetime of the buffer, so CPU writes are directly visible to
		* subsequent GPU commands. It is up to the caller to not overwrite memory still in use by the GPU (see Fence).
		* @note The storage is immutable: it cannot be reallocated.
		* @param p_size
		* @return Pointer to the mapped memory
		*/
		void* AllocatePersistentlyMapped(uint64_t p_size);

		/**
		* Uploads data to the buffer
		* @param p_data
		* @param p_range
		*/
		void Upload(const void* p_data, std::optional<data::BufferMemoryRange> p_range = std::nullopt);

		/**
		* Downloads data from the buffer
		* @note Blocks until the GPU is done writing to the buffer: use a Fence to avoid stalling
		* @param p_data
		* @param p_range
		*/
		void Download(void* p_data, std::optional<data::BufferMemoryRange> p_range = std::nullopt) const;

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
		* Binds a range of the buffer to an indexed binding point (UNIFORM or SHADER_STORAGE)
		* @param p_type Type of the buffer to bind
		* @param p_index Index to bind the buffer to
		* @param p_range Range of the buffer to bind (offset must satisfy the binding point alignment)
		*/
		void Bind(
			types::EBufferType p_type,
			uint32_t p_index,
			const data::BufferMemoryRange& p_range
		);

		/**
		* Unbinds the buffer
		*/
		void Unbind();

	protected:
		uint64_t m_allocatedBytes = 0;
		bool m_immutable = false;
		std::optional<types::EBufferType> m_boundAs = std::nullopt;
		std::optional<uint32_t> m_bindIndex = std::nullopt;
	};
}
