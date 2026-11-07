/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>

namespace OvPhysics::Settings
{
	/**
	* What a physics query looks for
	*/
	struct QuerySettings
	{
		/**
		* The collision layers the query sees: bit i for layer i. The layers' collision matrix plays no part
		*/
		uint16_t layerMask = 0xFFFF;

		/**
		* Whether the query reports triggers
		*/
		bool includeTriggers = false;
	};
}
