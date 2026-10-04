/**
* @project: baregl
* @author: Adrien Givry
* @licence: MIT
*/

#pragma once

#include <cstdint>

namespace baregl::types
{
	/**
	* Source of a texture component, when it is sampled (texture swizzling)
	*/
	enum class ETextureSwizzle : uint8_t
	{
		RED,
		GREEN,
		BLUE,
		ALPHA,
		ZERO,
		ONE
	};
}
