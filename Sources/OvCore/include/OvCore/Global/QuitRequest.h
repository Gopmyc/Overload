/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <OvTools/Eventing/Event.h>

namespace OvCore::Global
{
	/**
	* Lets gameplay code ask the running application to quit without knowing which one runs it:
	* the game closes its window, the editor leaves play mode
	*/
	struct QuitRequest
	{
		static inline OvTools::Eventing::Event<> RequestedEvent;
	};
}
