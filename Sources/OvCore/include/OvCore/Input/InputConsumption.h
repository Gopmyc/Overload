/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <functional>
#include <vector>

#include <OvCore/Input/InputId.h>

namespace OvCore::Input
{
	/**
	* What the game's reads leave out: the inputs consumed (by a UI that took a click, say) until they are released,
	* every key while the keyboard is captured, and the scroll consumed for the frame. It reads no device: it is told
	* each input's state, which is what lets the tests drive it
	*/
	class InputConsumption
	{
	public:
		/**
		* An input's state during the frame, as the game would see it without consumption
		*/
		struct State
		{
			float value = 0.0f; // 1 or 0 for a key or a button, 0 to 1 for a trigger or one way of a stick
			bool pressed = false;
			bool released = false;
		};

		/**
		* Reads an input of a pad (0 to 3; any for a key or a mouse button)
		*/
		using StateReader = std::function<State(const InputId& p_input, int p_pad)>;

		/**
		* Starts a frame, before the game reads its input: forgets the frame's releases and the scroll's consumption,
		* and shows again what was released (a key or a button once up, an axis once back to 0)
		* @param p_read
		*/
		void BeginFrame(const StateReader& p_read);

		/**
		* Hides an input that is held, or was pressed during the frame, until it is released. One the game saw held
		* before this frame is released for it on this frame. Returns false when there was nothing to hide: the input
		* is up, or hidden already
		* @param p_input
		* @param p_pad (for a gamepad input)
		* @param p_state
		*/
		bool Consume(const InputId& p_input, int p_pad, const State& p_state);

		/**
		* Hides the scroll of this frame
		*/
		void ConsumeMouseScroll();

		/**
		* Captures the keyboard, which hides every key, or frees it. A key the game saw held is released for it when the
		* capture starts; one held when it ends stays hidden until released
		* @param p_captured
		* @param p_read
		*/
		void SetKeyboardCaptured(bool p_captured, const StateReader& p_read);

		/**
		* Returns true while the keyboard is captured
		*/
		bool IsKeyboardCaptured() const;

		/**
		* Returns true if the game's reads leave the input out
		* @param p_input
		* @param p_pad
		*/
		bool IsHidden(const InputId& p_input, int p_pad) const;

		/**
		* Returns true if hiding the input released it for the game on this frame
		* @param p_input
		* @param p_pad
		*/
		bool IsReleasedForGame(const InputId& p_input, int p_pad) const;

		/**
		* Returns true if the scroll of this frame is hidden
		*/
		bool IsMouseScrollConsumed() const;

		/**
		* Forgets everything, the capture included, as a play session starts or ends
		*/
		void Reset();

	private:
		struct Entry
		{
			InputId input;
			int pad = 0;

			bool operator==(const Entry& p_other) const = default;
		};

		static Entry MakeEntry(const InputId& p_input, int p_pad);
		static bool Contains(const std::vector<Entry>& p_entries, const Entry& p_entry);

	private:
		std::vector<Entry> m_consumed;
		std::vector<Entry> m_releasedForGame;
		bool m_keyboardCaptured = false;
		bool m_scrollConsumed = false;
	};
}
