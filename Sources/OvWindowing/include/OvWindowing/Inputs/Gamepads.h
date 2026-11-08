/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <array>
#include <optional>
#include <string>

#include <OvWindowing/Inputs/EGamepadAxis.h>
#include <OvWindowing/Inputs/EGamepadButton.h>

namespace OvWindowing::Inputs
{
	/**
	* What a gamepad reports in a frame, as the window system gives it: its buttons held, and its axes from -1 to 1
	* (the sticks' Y positive downward, a trigger at rest at -1)
	*/
	struct GamepadState
	{
		static constexpr int kButtonCount = 15;
		static constexpr int kAxisCount = 6;

		std::string name;
		std::array<bool, kButtonCount> buttons{};
		std::array<float, kAxisCount> axes{};
	};

	/**
	* The state of the gamepads, frame after frame: which are connected, which buttons are held, pressed or released
	* during the frame, and their axes once the dead zone is applied. The input manager gives it each pad's state once
	* a frame; it does not read any device itself
	*/
	class Gamepads
	{
	public:
		static constexpr int kMaxGamepads = 4;
		static constexpr float kDefaultDeadZone = 0.2f;
		static constexpr float kTriggerDeadZone = 0.05f;

		/**
		* Gives a pad's state for this frame, or nullopt when it is not connected. The buttons pressed and released
		* during the frame are those that changed since the pad's last state; a pad that goes away releases its buttons
		* @param p_pad (0 to kMaxGamepads - 1)
		* @param p_state
		*/
		void SetState(int p_pad, const std::optional<GamepadState>& p_state);

		/**
		* Returns true if the pad is connected
		* @param p_pad
		*/
		bool IsConnected(int p_pad) const;

		/**
		* Returns the first connected pad, if any
		*/
		std::optional<int> GetFirstConnected() const;

		/**
		* Returns the name of the pad, empty when it is not connected
		* @param p_pad
		*/
		const std::string& GetName(int p_pad) const;

		/**
		* Returns true if the button is held
		* @param p_pad
		* @param p_button
		*/
		bool IsButtonDown(int p_pad, EGamepadButton p_button) const;

		/**
		* Returns true if the button was pressed during the frame
		* @param p_pad
		* @param p_button
		*/
		bool IsButtonPressed(int p_pad, EGamepadButton p_button) const;

		/**
		* Returns true if the button was released during the frame
		* @param p_pad
		* @param p_button
		*/
		bool IsButtonReleased(int p_pad, EGamepadButton p_button) const;

		/**
		* Returns an axis: a stick from -1 to 1, right and up positive, 0 inside the dead zone and scaled beyond it, so
		* that it still reaches 1; a trigger from 0 at rest to 1 pressed fully. 0 when the pad is not connected
		* @param p_pad
		* @param p_axis
		*/
		float GetAxis(int p_pad, EGamepadAxis p_axis) const;

		/**
		* Defines the share of a stick's travel, from its centre, that reads as 0 (0 to 0.95)
		* @param p_deadZone
		*/
		void SetDeadZone(float p_deadZone);

		/**
		* Returns the share of a stick's travel, from its centre, that reads as 0
		*/
		float GetDeadZone() const;

	private:
		struct Pad
		{
			bool connected = false;
			std::string name;
			std::array<bool, GamepadState::kButtonCount> held{};
			std::array<bool, GamepadState::kButtonCount> pressed{};
			std::array<bool, GamepadState::kButtonCount> released{};
			std::array<float, GamepadState::kAxisCount> axes{};
		};

		const Pad* FindPad(int p_pad) const;

	private:
		std::array<Pad, kMaxGamepads> m_pads;
		float m_deadZone = kDefaultDeadZone;
	};
}
