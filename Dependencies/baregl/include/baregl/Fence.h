/**
* @project: baregl
* @author: Adrien Givry
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <limits>

namespace baregl
{
	/**
	* Represents a GPU fence (sync object), inserted in the command stream when created.
	* Can be used to know when the GPU has finished processing all the commands submitted before it.
	*/
	class Fence final
	{
	public:
		/**
		* Creates a fence and inserts it in the command stream
		*/
		Fence();

		/**
		* Destroys the fence
		*/
		~Fence();

		/**
		* Deleted copy constructor
		*/
		Fence(const Fence&) = delete;

		/**
		* Deleted assignment operator
		*/
		Fence& operator=(const Fence&) = delete;

		/**
		* Returns true if the GPU has reached this fence (non-blocking)
		*/
		bool IsSignaled() const;

		/**
		* Blocks the calling thread until the GPU reaches this fence, or until the timeout expires
		* @param p_timeoutNs
		* @return true if the fence got signaled, false if the timeout expired or an error occurred
		*/
		bool Wait(uint64_t p_timeoutNs = std::numeric_limits<uint64_t>::max()) const;

	private:
		void* m_sync = nullptr;
	};
}
