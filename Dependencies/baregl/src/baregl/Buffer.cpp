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
		if (m_mappedData)
		{
			glUnmapNamedBuffer(m_id);
		}

		glDeleteBuffers(1, &m_id);
		NOTIFY_BUFFER_DESTROYED;
	}

	uint64_t Buffer::Allocate(uint64_t p_size, types::EAccessSpecifier p_usage)
	{
		BAREGL_ASSERT(IsValid(), "Cannot allocate memory for an invalid buffer");
		BAREGL_ASSERT(!m_mappedData, "Cannot reallocate a persistently mapped buffer");
		glNamedBufferData(m_id, p_size, nullptr, utils::EnumToValue<GLenum>(p_usage));
		return m_allocatedBytes = p_size;
	}

	uint64_t Buffer::AllocatePersistent(uint64_t p_size, bool p_readable)
	{
		BAREGL_ASSERT(IsValid(), "Cannot allocate memory for an invalid buffer");
		BAREGL_ASSERT(!m_mappedData && m_allocatedBytes == 0, "Persistent storage can only be allocated once");

		const GLbitfield accessFlags =
			(p_readable ? GL_MAP_READ_BIT : GL_MAP_WRITE_BIT) |
			GL_MAP_PERSISTENT_BIT |
			GL_MAP_COHERENT_BIT;

		glNamedBufferStorage(m_id, p_size, nullptr, accessFlags);
		m_mappedData = glMapNamedBufferRange(m_id, 0, p_size, accessFlags);

		BAREGL_ASSERT(m_mappedData != nullptr, "Failed to persistently map buffer");

		return m_allocatedBytes = p_size;
	}

	void* Buffer::GetMappedData() const
	{
		return m_mappedData;
	}

	void Buffer::Upload(const void* p_data, std::optional<data::BufferMemoryRange> p_range)
	{
		BAREGL_ASSERT(IsValid(), "Trying to upload data to an invalid buffer");
		BAREGL_ASSERT(!IsEmpty(), "Trying to upload data to an empty buffer");
		BAREGL_ASSERT(!m_mappedData, "Persistently mapped buffers must be written through GetMappedData()");

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

	void Buffer::BindRange(
		types::EBufferType p_type,
		uint32_t p_index,
		const data::BufferMemoryRange& p_range
	)
	{
		BAREGL_ASSERT(IsValid(), "Cannot bind an invalid buffer");
		BAREGL_ASSERT(p_range.offset + p_range.size <= m_allocatedBytes, "Buffer range out of bounds");

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

	uint32_t Buffer::GetUniformBufferOffsetAlignment()
	{
		// Only a single context is supported, so the value can be queried once
		static const uint32_t alignment = []() {
			GLint value = 0;
			glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &value);
			return static_cast<uint32_t>(value > 0 ? value : 256);
		}();

		return alignment;
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
