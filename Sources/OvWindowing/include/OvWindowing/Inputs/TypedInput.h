/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <OvWindowing/Inputs/EKey.h>

namespace OvWindowing::Inputs
{
	/**
	* What the keyboard typed during a frame: the characters, as the keyboard's layout and the system's input method
	* make them, in UTF-8, and how many times the system repeated each key held down. It reads no device: the window's
	* events feed it, and the tests can
	*/
	class TypedInput
	{
	public:
		/**
		* Adds a typed character. A control character, a surrogate or a value beyond U+10FFFF is ignored
		* @param p_codePoint
		*/
		void AddCharacter(uint32_t p_codePoint);

		/**
		* Counts a repetition of a key held down
		* @param p_key
		*/
		void AddKeyRepeat(EKey p_key);

		/**
		* Returns the characters typed during the frame, in UTF-8
		*/
		const std::string& GetText() const;

		/**
		* Returns how many times the system repeated the key during the frame
		* @param p_key
		*/
		int GetRepeatCount(EKey p_key) const;

		/**
		* Forgets the frame's text and repetitions
		*/
		void Clear();

	private:
		std::string m_text;
		std::unordered_map<EKey, int> m_repeats;
	};
}
