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
	* Defines when the shadow map of a light is rendered
	*/
	enum class EShadowMapUpdateMode : uint8_t
	{
		REALTIME,	// Rendered every frame
		ON_CHANGE	// Rendered only when the light, or something drawn into its shadow map, changed
	};
}
