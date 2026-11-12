/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <optional>
#include <string>
#include <unordered_set>

#include <OvWindowing/Inputs/Gamepads.h>
#include <OvWindowing/Inputs/TypedInput.h>
#include <OvWindowing/Inputs/EMouseButton.h>
#include <OvWindowing/Inputs/EMouseButtonState.h>
#include <OvWindowing/Inputs/EKey.h>
#include <OvWindowing/Inputs/EKeyState.h>
#include <OvWindowing/Window.h>

namespace OvWindowing::Inputs
{
	/**
	* Handles inputs (mouse, keyboard and gamepads)
	*/
	class InputManager
	{
	public:
		/**
		* Create the window
		* @param p_windowSettings
		*/
		InputManager(Window& p_window);

		/**
		* Destroy the input manager by removing listeners on the window
		*/
		~InputManager();

		/**
		* Return the current state of the given key
		* @param p_key
		*/
		EKeyState GetKeyState(EKey p_key) const;

		/**
		* Return the current state of the given mouse button
		* @param p_button
		*/
		EMouseButtonState GetMouseButtonState(EMouseButton p_button) const;

		/**
		* Return true if the given key has been pressed during the frame
		* @param p_key
		*/
		bool IsKeyPressed(EKey p_key) const;

		/**
		* Return true if the given key has been released during the frame
		* @param p_key
		*/
		bool IsKeyReleased(EKey p_key) const;

		/**
		* Return true if the given mouse button has been pressed during the frame
		* @param p_button
		*/
		bool IsMouseButtonPressed(EMouseButton p_button) const;

		/**
		* Return true if the given mouse button has been released during the frame
		* @param p_button
		*/
		bool IsMouseButtonReleased(EMouseButton p_button) const;

		/**
		* Return the name of a printable key in the keyboard's current layout (the "z" of a French keyboard where the
		* key is EKey::KEY_W), or an empty string for a key that prints nothing
		* @param p_key
		*/
		std::string GetKeyName(EKey p_key) const;

		/**
		* Return the mouse position relative to the window, read once per frame (until ClearEvents) and again when the
		* cursor mode changes
		*/
		std::pair<double, double> GetMousePosition() const;

		/**
		* Returns the scroll data for the current frame
		*/
		std::pair<double, double> GetMouseScroll() const;

		/**
		* Returns what the keyboard typed during the frame: characters in UTF-8, and the system's repetitions of keys
		* held down
		*/
		const TypedInput& GetTypedInput() const;

		/**
		* Clear any event occured
		* @note Should be called at the end of every game tick
		*/
		void ClearEvents();

		/**
		* Reads the gamepads the window system knows, as their state for this frame: the buttons pressed and released
		* during the frame are those that changed since the last call. Only a device the window system can map to a
		* gamepad layout is read
		* @note Should be called once a frame, after the window system's events are polled
		*/
		void UpdateGamepads();

		/**
		* Returns the state of the gamepads
		*/
		Gamepads& GetGamepads();

		/**
		* Returns the state of the gamepads
		*/
		const Gamepads& GetGamepads() const;

	private:
		void OnKeyPressed(int p_key);
		void OnKeyReleased(int p_key);
		void OnMouseButtonPressed(int p_button);
		void OnMouseButtonReleased(int p_button);
		void OnMouseScroll(double p_xOffset, double p_yOffset);
		void OnKeyRepeated(int p_key);
		void OnCharacter(uint32_t p_codePoint);

	private:
		Window& m_window;

		OvTools::Eventing::ListenerID m_keyPressedListener;
		OvTools::Eventing::ListenerID m_keyReleasedListener;
		OvTools::Eventing::ListenerID m_mouseButtonPressedListener;
		OvTools::Eventing::ListenerID m_mouseButtonReleasedListener;
		OvTools::Eventing::ListenerID m_mouseScrollListener;
		OvTools::Eventing::ListenerID m_keyRepeatedListener;
		OvTools::Eventing::ListenerID m_characterListener;

		std::unordered_set<EKey> m_keyPressed;
		std::unordered_set<EKey> m_keyReleased;

		std::unordered_set<EMouseButton> m_mouseButtonPressed;
		std::unordered_set<EMouseButton> m_mouseButtonReleased;

		std::pair<double, double> m_scrollData;

		Gamepads m_gamepads;
		TypedInput m_typedInput;

		mutable std::optional<std::pair<double, double>> m_mousePosition;
		mutable Cursor::ECursorMode m_mousePositionMode = Cursor::ECursorMode::NORMAL;
	};
}
