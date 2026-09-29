/**
* @project: baregl
* @author: Adrien Givry
* @licence: MIT
*/

#include <baregl/Fence.h>

#include <baregl/detail/glad/glad.h>

namespace baregl
{
	Fence::~Fence()
	{
		Reset();
	}

	void Fence::Insert()
	{
		Reset();
		m_sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	}

	void Fence::Wait()
	{
		if (!m_sync)
		{
			return;
		}

		const auto sync = static_cast<GLsync>(m_sync);

		while (true)
		{
			// The flush bit ensures the fence reaches the GPU, otherwise we could wait forever
			const GLenum result = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 1'000'000'000ULL /* 1s */);

			if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED || result == GL_WAIT_FAILED)
			{
				break;
			}
		}

		Reset();
	}

	bool Fence::IsSignaled()
	{
		if (!m_sync)
		{
			return false;
		}

		const GLenum result = glClientWaitSync(static_cast<GLsync>(m_sync), GL_SYNC_FLUSH_COMMANDS_BIT, 0);
		return result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED;
	}

	void Fence::Reset()
	{
		if (m_sync)
		{
			glDeleteSync(static_cast<GLsync>(m_sync));
			m_sync = nullptr;
		}
	}

	bool Fence::IsPending() const
	{
		return m_sync != nullptr;
	}
}
