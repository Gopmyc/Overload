/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace OvCore::Input
{
	/**
	* An input an action can be bound to: a key, a mouse button, a gamepad button, a trigger, or one way of a stick's
	* axis. It is named in files and in Lua: "SPACE" (the names of the Lua enum Key), "MOUSE_LEFT", "MOUSE_BUTTON_4",
	* "GAMEPAD_A", "GAMEPAD_LEFT_TRIGGER", "GAMEPAD_LEFT_X+", "GAMEPAD_LEFT_Y-"
	*/
	struct InputId
	{
		enum class EDevice : uint8_t
		{
			KEY,
			MOUSE_BUTTON,
			GAMEPAD_BUTTON,
			GAMEPAD_AXIS
		};

		EDevice device = EDevice::KEY;
		int code = 0;			// The EKey, EMouseButton, EGamepadButton or EGamepadAxis
		int8_t direction = 0;	// For a stick's axis, +1 or -1 (right and up are positive); 0 otherwise

		bool operator==(const InputId& p_other) const = default;

		/**
		* Returns true for a key or a button, which is held or not; false for an axis, which goes from 0 to 1
		*/
		bool IsButton() const;
	};

	/**
	* Reads an input's name, or nullopt for a name no input has
	* @param p_name
	*/
	std::optional<InputId> ParseInput(std::string_view p_name);

	/**
	* Returns an input's name, as ParseInput reads it
	* @param p_input
	*/
	std::string GetInputName(const InputId& p_input);

	/**
	* Returns every input an action can be bound to, keys first, then the mouse buttons, the gamepad buttons and the
	* gamepad's axes
	*/
	const std::vector<InputId>& GetAllInputs();
}
