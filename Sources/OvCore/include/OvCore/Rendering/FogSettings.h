/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <OvMaths/FVector3.h>

namespace OvCore::Rendering
{
	/**
	* Distance fog, set on a post process stack and applied by the shaders that include Fog.ovfxh.
	* Past start meters from the eye, a surface fades towards the fog colour by 1 - exp(-(distance * density)^2),
	* never beyond maxOpacity
	*/
	struct FogSettings
	{
		bool enabled = false;
		OvMaths::FVector3 color = { 0.75f, 0.8f, 0.85f };
		float intensity = 1.0f;
		float start = 20.0f;
		float density = 0.02f;
		float maxOpacity = 1.0f;
	};
}
