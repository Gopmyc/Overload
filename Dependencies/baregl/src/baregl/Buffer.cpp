/**
* @project: baregl
* @author: Adrien Givry
* @licence: MIT
*/

#include <baregl/Buffer.h>

#include <baregl/debug/Assert.h>
#include <baregl/debug/Event.h>
#include <baregl/detail/glad/glad.h>
#include <baregl/detail/Types.h>

namespace baregl
{
	Buffer::Buffer()
	{
		glCreateBuffers(1, &m_id);
		NOTIFY_BUFFER_CREATED;
	}

	Buffer::~Buffer()
	{
		glDeleteBuffers(1, &m_id);
		NOTIFY_BUFFER_DESTROYED;
	}

	uint64_t Buffer::Allocate(uint64_t p_size, types::EAccessSpecifier p_usage)
	{
		BAREGL_ASSERT(IsValid(), "Cannot allocate memory for an invalid buffer");
		BAREGL_ASSERT(!m_immutable, "Cannot reallocate a buffer with an immutable storage");
		glNamedBufferData(m_id, p_size, nullptr, utils::EnumToValue<GLenum>(p_usage));
		return m_allocatedBytes = p_size;
	}

	void* Buffer::AllocatePersistentlyMapped(uint64_t p_size)
	{
		BAREGL_ASSERT(IsValid(), "Cannot allocate memory for an invalid buffer");
		BAREGL_ASSERT(!m_immutable, "Cannot reallocate a buffer with an immutable storage");
		BAREGL_ASSERT(p_size > 0, "Cannot allocate an empty persistently mapped buffer");

		constexpr GLbitfield kFlags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
		glNamedBufferStorage(m_id, p_size, nullptr, kFlags);
		m_immutable = true;
		m_allocatedBytes = p_size;

		return glMapNamedBufferRange(m_id, 0, p_size, kFlags);
	}

	void Buffer::Upload(const void* p_data, std::optional<data::BufferMemoryRange> p_range)
	{
		BAREGL_ASSERT(IsValid(), "Trying to upload data to an invalid buffer");
		BAREGL_ASSERT(!IsEmpty(), "Trying to upload data to an empty buffer");

		glNamedBufferSubData(
			m_id,
			p_range ? p_range->offset : 0,
			p_range ? p_range->size : m_allocatedBytes,
			p_data
		);
	}

	void Buffer::Bind(
		types::EBufferType p_type,
		std::optional<uint32_t> p_index
	)
	{
		BAREGL_ASSERT(IsValid(), "Cannot bind an invalid buffer");

		if (p_index.has_value())
		{
			glBindBufferBase(utils::EnumToValue<GLenum>(p_type), p_index.value(), m_id);
		}
		else
		{
			glBindBuffer(utils::EnumToValue<GLenum>(p_type), m_id);
		}

		m_boundAs = p_type;
		m_bindIndex = p_index;
	}

	void Buffer::Bind(
		types::EBufferType p_type,
		uint32_t p_index,
		const data::BufferMemoryRange& p_range
	)
	{
		BAREGL_ASSERT(IsValid(), "Cannot bind an invalid buffer");
		BAREGL_ASSERT(p_range.offset + p_range.size <= m_allocatedBytes, "Cannot bind a range outside of the buffer");

		glBindBufferRange(
			utils::EnumToValue<GLenum>(p_type),
			p_index,
			m_id,
			static_cast<GLintptr>(p_range.offset),
			static_cast<GLsizeiptr>(p_range.size)
		);

		m_boundAs = p_type;
		m_bindIndex = p_index;
	}

	void Buffer::Unbind()
	{
		BAREGL_ASSERT(IsValid(), "Cannot unbind an invalid buffer");
		BAREGL_ASSERT(m_boundAs.has_value(), "Cannot unbind a buffer that is not bound");

		if (m_bindIndex.has_value())
		{
			glBindBufferBase(utils::EnumToValue<GLenum>(m_boundAs.value()), m_bindIndex.value(), m_id);
		}
		else
		{
			glBindBuffer(utils::EnumToValue<GLenum>(m_boundAs.value()), 0);
		}

		m_boundAs.reset();
	}

	bool Buffer::IsValid() const
	{
		return m_id != 0;
	}

	bool Buffer::IsEmpty() const
	{
		return GetSize() == 0;
	}

	uint64_t Buffer::GetSize() const
	{
		BAREGL_ASSERT(IsValid(), "Cannot get size of an invalid buffer");
		return m_allocatedBytes;
	}
}
