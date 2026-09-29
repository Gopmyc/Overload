/**
* @project: baregl
* @author: Adrien Givry
* @licence: MIT
*/

#pragma once

namespace baregl
{
	/**
	* GPU synchronization primitive, used to know when the GPU is done with the commands
	* issued before the fence (e.g. to safely reuse memory of a persistently mapped buffer)
	*/
	class Fence final
	{
	public:
		/**
		* Creates an empty fence (not inserted in the command stream)
		*/
		Fence() = default;

		/**
		* Destroys the fence
		*/
		~Fence();

		Fence(const Fence&) = delete;
		Fence& operator=(const Fence&) = delete;

		/**
		* Inserts the fence in the command stream (replaces the previously inserted fence, if any)
		*/
		void Insert();

		/**
		* Blocks until the GPU executed all the commands issued before the fence, then clears the fence.
		* Returns immediately if the fence hasn't been inserted.
		*/
		void Wait();

		/**
		* Returns true if the GPU executed all the commands issued before the fence (never blocks).
		* Returns false if the fence hasn't been inserted.
		*/
		bool IsSignaled();

		/**
		* Clears the fence without waiting for it
		*/
		void Reset();

		/**
		* Returns true if the fence has been inserted and not waited for yet
		*/
		bool IsPending() const;

	private:
		void* m_sync = nullptr;
	};
}
