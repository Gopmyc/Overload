/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <functional>
#include <optional>
#include <string>

#include <OvMaths/FVector2.h>
#include <OvWindowing/Cursor/ECursorShape.h>
#include <OvWindowing/Inputs/EGamepadAxis.h>
#include <OvWindowing/Inputs/EGamepadButton.h>
#include <OvWindowing/Inputs/EKey.h>
#include <OvWindowing/Inputs/EMouseButton.h>

namespace OvCore::Helpers
{
	/**
	* Input helpers shared between the editor and the game
	*/
	class InputHelpers
	{
	public:
		using MousePositionProviderCallback = std::function<OvMaths::FVector2()>;
		using ViewportSizeProviderCallback = std::function<OvMaths::FVector2()>;

		/**
		* Defines the callback returning the mouse position relative to the area where the game is rendered
		* @param p_provider
		*/
		static void SetMousePositionProvider(MousePositionProviderCallback p_provider);

		/**
		* Returns the mouse position relative to the area where the game is rendered
		* @note Without provider, the position is relative to the window, which is the rendered area of a built game
		*/
		static OvMaths::FVector2 GetMousePosition();

		/**
		* Defines the callback returning the size of the area where the game is rendered
		* @param p_provider
		*/
		static void SetViewportSizeProvider(ViewportSizeProviderCallback p_provider);

		/**
		* Returns the size of the area where the game is rendered
		* @note Without provider, the size is the window size, which is the rendered area of a built game
		*/
		static OvMaths::FVector2 GetViewportSize();

		/**
		* Returns true while the key is held, as the game sees it
		* @note The game's input goes through a gate its host may close (the editor opens it to the focused game view).
		* A built game never touches the gate: its input is always open
		* @param p_key
		*/
		static bool GetKey(OvWindowing::Inputs::EKey p_key);

		/**
		* Returns true if the key was pressed this frame, as the game sees it
		* @param p_key
		*/
		static bool GetKeyDown(OvWindowing::Inputs::EKey p_key);

		/**
		* Returns true if the key was released this frame, as the game sees it (a key held when the gate closes is released then)
		* @param p_key
		*/
		static bool GetKeyUp(OvWindowing::Inputs::EKey p_key);

		/**
		* Returns true while the mouse button is held, as the game sees it
		* @param p_button
		*/
		static bool GetMouseButton(OvWindowing::Inputs::EMouseButton p_button);

		/**
		* Returns true if the mouse button was pressed this frame, as the game sees it
		* @param p_button
		*/
		static bool GetMouseButtonDown(OvWindowing::Inputs::EMouseButton p_button);

		/**
		* Returns true if the mouse button was released this frame, as the game sees it
		* @param p_button
		*/
		static bool GetMouseButtonUp(OvWindowing::Inputs::EMouseButton p_button);

		/**
		* Returns the mouse scroll of this frame, as the game sees it
		*/
		static OvMaths::FVector2 GetMouseScroll();

		/**
		* Returns the first connected gamepad (0 to 3), if any
		*/
		static std::optional<int> GetFirstGamepad();

		/**
		* Returns true if the gamepad is connected
		* @param p_pad (0 to 3)
		*/
		static bool IsGamepadConnected(int p_pad);

		/**
		* Returns the name of the gamepad, empty when it is not connected
		* @param p_pad
		*/
		static std::string GetGamepadName(int p_pad);

		/**
		* Returns true if the gamepad button is held, as the game sees it
		* @param p_pad
		* @param p_button
		*/
		static bool GetGamepadButton(int p_pad, OvWindowing::Inputs::EGamepadButton p_button);

		/**
		* Returns true if the gamepad button was pressed during the frame, as the game sees it
		* @param p_pad
		* @param p_button
		*/
		static bool GetGamepadButtonDown(int p_pad, OvWindowing::Inputs::EGamepadButton p_button);

		/**
		* Returns true if the gamepad button was released during the frame, as the game sees it
		* @param p_pad
		* @param p_button
		*/
		static bool GetGamepadButtonUp(int p_pad, OvWindowing::Inputs::EGamepadButton p_button);

		/**
		* Returns a gamepad axis, dead zone applied, as the game sees it: 0 while the gate is closed
		* @param p_pad
		* @param p_axis
		*/
		static float GetGamepadAxis(int p_pad, OvWindowing::Inputs::EGamepadAxis p_axis);

		/**
		* Defines the share of a stick's travel, from its centre, that reads as 0
		* @param p_deadZone
		*/
		static void SetGamepadDeadZone(float p_deadZone);

		/**
		* Returns the share of a stick's travel, from its centre, that reads as 0
		*/
		static float GetGamepadDeadZone();

		/**
		* Locks (hides and captures) or frees the mouse for the game. While the gate is closed the request is kept,
		* and applied once it opens again
		* @param p_locked
		*/
		static void SetMouseLocked(bool p_locked);

		/**
		* Sets the cursor shape the game asks for, ignored while the gate is closed
		* @param p_shape
		*/
		static void SetCursorShape(OvWindowing::Cursor::ECursorShape p_shape);

		/**
		* Opens or closes the game's input, once a frame, before the game updates
		* @param p_open: whether the game gets any input
		* @param p_mouseOverGame: whether the clicks and the scroll of this frame are meant for the game
		*/
		static void UpdateGameInputGate(bool p_open, bool p_mouseOverGame);

		/**
		* Closes the game's input and forgets what the game asked for, as a new play session starts or ends
		*/
		static void ResetGameInputGate();
	};
}
