/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

namespace OvWindowing::Inputs
{
	/**
	* Gamepad axes: the sticks go from -1 to 1 (right and up are positive), the triggers from 0 to 1
	*/
	enum class EGamepadAxis
	{
		GAMEPAD_AXIS_LEFT_X			= 0,
		GAMEPAD_AXIS_LEFT_Y			= 1,
		GAMEPAD_AXIS_RIGHT_X		= 2,
		GAMEPAD_AXIS_RIGHT_Y		= 3,
		GAMEPAD_AXIS_LEFT_TRIGGER	= 4,
		GAMEPAD_AXIS_RIGHT_TRIGGER	= 5
	};
}
