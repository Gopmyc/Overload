/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

namespace OvWindowing::Inputs
{
	/**
	* Gamepad buttons, by their place on an Xbox layout (A is the bottom face button, Cross on a PlayStation pad)
	*/
	enum class EGamepadButton
	{
		GAMEPAD_BUTTON_A			= 0,
		GAMEPAD_BUTTON_B			= 1,
		GAMEPAD_BUTTON_X			= 2,
		GAMEPAD_BUTTON_Y			= 3,
		GAMEPAD_BUTTON_LEFT_BUMPER	= 4,
		GAMEPAD_BUTTON_RIGHT_BUMPER	= 5,
		GAMEPAD_BUTTON_BACK			= 6,
		GAMEPAD_BUTTON_START		= 7,
		GAMEPAD_BUTTON_GUIDE		= 8,
		GAMEPAD_BUTTON_LEFT_THUMB	= 9,
		GAMEPAD_BUTTON_RIGHT_THUMB	= 10,
		GAMEPAD_BUTTON_DPAD_UP		= 11,
		GAMEPAD_BUTTON_DPAD_RIGHT	= 12,
		GAMEPAD_BUTTON_DPAD_DOWN	= 13,
		GAMEPAD_BUTTON_DPAD_LEFT	= 14
	};
}
