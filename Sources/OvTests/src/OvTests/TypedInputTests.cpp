/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <string>

#include <doctest/doctest.h>

#include <OvWindowing/Inputs/TypedInput.h>

using OvWindowing::Inputs::EKey;
using OvWindowing::Inputs::TypedInput;

TEST_SUITE("Typed input")
{
	TEST_CASE("characters are kept in UTF-8, whatever their length")
	{
		TypedInput input;

		input.AddCharacter(0x41);    // A, one byte
		input.AddCharacter(0xE9);    // e acute, two bytes
		input.AddCharacter(0x416);   // Cyrillic Zhe, two bytes
		input.AddCharacter(0x20AC);  // the euro sign, three bytes
		input.AddCharacter(0x1F600); // an emoji, four bytes

		CHECK(input.GetText() == "A\xC3\xA9\xD0\x96\xE2\x82\xAC\xF0\x9F\x98\x80");
	}

	TEST_CASE("control characters and values that are no character are left out")
	{
		TypedInput input;

		for (const uint32_t codePoint : { 0x08u, 0x0Du, 0x1Bu, 0x7Fu, 0x85u, 0xD800u, 0xDFFFu, 0x110000u })
		{
			input.AddCharacter(codePoint);
		}

		CHECK(input.GetText().empty());

		input.AddCharacter(0x20);
		input.AddCharacter(0xA0);
		CHECK(input.GetText() == " \xC2\xA0");
	}

	TEST_CASE("repeats are counted per key, and a frame forgets them with the text")
	{
		TypedInput input;

		input.AddKeyRepeat(EKey::KEY_BACKSPACE);
		input.AddKeyRepeat(EKey::KEY_BACKSPACE);
		input.AddKeyRepeat(EKey::KEY_LEFT);
		input.AddCharacter(0x61);

		CHECK(input.GetRepeatCount(EKey::KEY_BACKSPACE) == 2);
		CHECK(input.GetRepeatCount(EKey::KEY_LEFT) == 1);
		CHECK(input.GetRepeatCount(EKey::KEY_RIGHT) == 0);

		input.Clear();
		CHECK(input.GetText().empty());
		CHECK(input.GetRepeatCount(EKey::KEY_BACKSPACE) == 0);
	}
}
