/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <optional>

#include "OvRendering/Data/PipelineState.h"

namespace OvRendering::Settings
{
	/**
	* Settings that are sent to the driver at construction
	*/
	struct DriverSettings
	{
		bool debugMode = false;
		std::optional<OvRendering::Data::PipelineState> defaultPipelineState = std::nullopt;

		/**
		* Maximum number of frames the CPU can submit before waiting for the GPU to complete them.
		* Without this limit, the graphics driver is free to queue several frames (usually 2 or 3),
		* which adds input latency when the application is GPU-bound and VSync is disabled.
		* 1 means the CPU waits, at the end of frame N, for the GPU to complete frame N-1.
		* 0 disables the limit (driver-controlled queue).
		*/
		uint32_t maxQueuedFrames = 1;
	};
}
