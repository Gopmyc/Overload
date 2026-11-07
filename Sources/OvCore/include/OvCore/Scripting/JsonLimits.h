/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstddef>

namespace OvCore::Scripting
{
	/**
	* What the Lua Json library accepts, for the whole process
	*/
	struct JsonLimits
	{
		/**
		* The longest text encoded or decoded, in bytes (the same ceiling as a saved file by default)
		*/
		size_t maxSize = 4 * 1024 * 1024;

		/**
		* The deepest nesting: no data worth saving reaches it, but a table that holds itself always does
		*/
		int maxDepth = 64;
	};

	/**
	* Sets what the Lua Json library accepts from now on
	* @param p_limits
	*/
	void SetJsonLimits(const JsonLimits& p_limits);

	/**
	* Returns what the Lua Json library accepts
	*/
	const JsonLimits& GetJsonLimits();
}
