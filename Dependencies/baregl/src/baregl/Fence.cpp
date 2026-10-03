/**
* @project: baregl
* @author: Adrien Givry
* @licence: MIT
*/

#include <baregl/Fence.h>

#include <baregl/detail/glad/glad.h>

namespace baregl
{
	Fence::Fence() :
		m_sync(glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0))
	{
	}

	Fence::~Fence()
	{
		if (m_sync)
		{
			glDeleteSync(static_cast<GLsync>(m_sync));
		}
	}

	bool Fence::IsSignaled() const
	{
		if (!m_sync)
		{
			return true;
		}

		const GLenum result = glClientWaitSync(static_cast<GLsync>(m_sync), 0, 0);
		return result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED;
	}

	bool Fence::Wait(uint64_t p_timeoutNs) const
	{
		if (!m_sync)
		{
			return true;
		}

		// The flush bit ensures the fence is submitted to the GPU, otherwise we could wait forever
		const GLenum result = glClientWaitSync(static_cast<GLsync>(m_sync), GL_SYNC_FLUSH_COMMANDS_BIT, p_timeoutNs);
		return result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED;
	}
}
