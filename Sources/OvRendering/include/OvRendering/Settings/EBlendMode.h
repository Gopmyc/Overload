/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>

namespace OvRendering::Settings
{
	/**
	* How a blendable material's colour is combined with what is already drawn
	*/
	enum class EBlendMode : uint8_t
	{
		ALPHA,			// The blending of the pass: alpha blending (source × alpha + destination × (1 - alpha)) in every scene pass
		ADDITIVE,		// Source × alpha + destination: light that adds up, such as sparks, glows and fire
		PREMULTIPLIED	// Source + destination × (1 - alpha): a colour already multiplied by its alpha
	};

	/**
	* The number of blend modes
	*/
	constexpr uint8_t kBlendModeCount = 3;
}
