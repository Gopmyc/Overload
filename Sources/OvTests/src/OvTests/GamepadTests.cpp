/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cmath>
#include <optional>

#include <doctest/doctest.h>

#include <OvWindowing/Inputs/Gamepads.h>

using OvWindowing::Inputs::EGamepadAxis;
using OvWindowing::Inputs::EGamepadButton;
using OvWindowing::Inputs::GamepadState;
using OvWindowing::Inputs::Gamepads;

namespace
{
	GamepadState Pad(std::initializer_list<EGamepadButton> p_held = {})
	{
		GamepadState state;
		state.name = "Test Pad";

		for (const EGamepadButton button : p_held)
			state.buttons[static_cast<int>(button)] = true;

		// Triggers at rest, as the window system reports them
		state.axes[static_cast<int>(EGamepadAxis::GAMEPAD_AXIS_LEFT_TRIGGER)] = -1.0f;
		state.axes[static_cast<int>(EGamepadAxis::GAMEPAD_AXIS_RIGHT_TRIGGER)] = -1.0f;
		return state;
	}

	GamepadState WithAxis(GamepadState p_state, EGamepadAxis p_axis, float p_value)
	{
		p_state.axes[static_cast<int>(p_axis)] = p_value;
		return p_state;
	}
}

TEST_SUITE("Gamepads")
{
	TEST_CASE("a pad connects with its name, and none is connected at first")
	{
		Gamepads gamepads;
		CHECK_FALSE(gamepads.GetFirstConnected());
		CHECK_FALSE(gamepads.IsConnected(0));
		CHECK(gamepads.GetName(0).empty());

		gamepads.SetState(2, Pad());
		CHECK(gamepads.IsConnected(2));
		CHECK(gamepads.GetFirstConnected() == 2);
		CHECK(gamepads.GetName(2) == "Test Pad");

		// Out of range, a pad is never connected
		gamepads.SetState(7, Pad());
		CHECK_FALSE(gamepads.IsConnected(7));
		CHECK_FALSE(gamepads.IsConnected(-1));
		CHECK(gamepads.GetName(9).empty());
	}

	TEST_CASE("a button is pressed on the frame it goes down, held while down, released on the frame it goes up")
	{
		constexpr auto kA = EGamepadButton::GAMEPAD_BUTTON_A;
		Gamepads gamepads;

		gamepads.SetState(0, Pad());
		CHECK_FALSE(gamepads.IsButtonDown(0, kA));

		gamepads.SetState(0, Pad({ kA }));
		CHECK(gamepads.IsButtonPressed(0, kA));
		CHECK(gamepads.IsButtonDown(0, kA));
		CHECK_FALSE(gamepads.IsButtonReleased(0, kA));

		gamepads.SetState(0, Pad({ kA }));
		CHECK_FALSE(gamepads.IsButtonPressed(0, kA));
		CHECK(gamepads.IsButtonDown(0, kA));

		gamepads.SetState(0, Pad());
		CHECK(gamepads.IsButtonReleased(0, kA));
		CHECK_FALSE(gamepads.IsButtonDown(0, kA));

		gamepads.SetState(0, Pad());
		CHECK_FALSE(gamepads.IsButtonReleased(0, kA));

		// Another pad's buttons are its own
		gamepads.SetState(1, Pad({ kA }));
		CHECK(gamepads.IsButtonDown(1, kA));
		CHECK_FALSE(gamepads.IsButtonDown(0, kA));
	}

	TEST_CASE("a pad that goes away releases what it held, and reads nothing")
	{
		constexpr auto kStart = EGamepadButton::GAMEPAD_BUTTON_START;
		Gamepads gamepads;

		gamepads.SetState(0, WithAxis(Pad({ kStart }), EGamepadAxis::GAMEPAD_AXIS_LEFT_X, 1.0f));
		CHECK(gamepads.GetAxis(0, EGamepadAxis::GAMEPAD_AXIS_LEFT_X) == doctest::Approx(1.0f));

		gamepads.SetState(0, std::nullopt);
		CHECK_FALSE(gamepads.IsConnected(0));
		CHECK(gamepads.IsButtonReleased(0, kStart));
		CHECK_FALSE(gamepads.IsButtonDown(0, kStart));
		CHECK(gamepads.GetAxis(0, EGamepadAxis::GAMEPAD_AXIS_LEFT_X) == 0.0f);
		CHECK(gamepads.GetAxis(0, EGamepadAxis::GAMEPAD_AXIS_LEFT_TRIGGER) == 0.0f);
		CHECK(gamepads.GetName(0).empty());
	}

	TEST_CASE("a stick reads 0 inside its dead zone, reaches 1 at full travel, and up is positive")
	{
		constexpr auto kX = EGamepadAxis::GAMEPAD_AXIS_LEFT_X;
		constexpr auto kY = EGamepadAxis::GAMEPAD_AXIS_LEFT_Y;
		Gamepads gamepads;
		CHECK(gamepads.GetDeadZone() == doctest::Approx(Gamepads::kDefaultDeadZone));

		gamepads.SetState(0, WithAxis(Pad(), kX, 0.15f));
		CHECK(gamepads.GetAxis(0, kX) == 0.0f);

		gamepads.SetState(0, WithAxis(Pad(), kX, 1.0f));
		CHECK(gamepads.GetAxis(0, kX) == doctest::Approx(1.0f));

		// Halfway between the dead zone and full travel
		gamepads.SetState(0, WithAxis(Pad(), kX, -0.6f));
		CHECK(gamepads.GetAxis(0, kX) == doctest::Approx(-0.5f));

		// The window system's Y points down: pushed up, the stick reads positive
		gamepads.SetState(0, WithAxis(Pad(), kY, -1.0f));
		CHECK(gamepads.GetAxis(0, kY) == doctest::Approx(1.0f));
		CHECK(gamepads.GetAxis(0, kX) == 0.0f);

		// The dead zone is a disc: a diagonal past it keeps both axes, and stays within the unit circle
		auto diagonal = WithAxis(WithAxis(Pad(), kX, 0.7071f), kY, -0.7071f);
		gamepads.SetState(0, diagonal);
		const float x = gamepads.GetAxis(0, kX);
		const float y = gamepads.GetAxis(0, kY);
		CHECK(x == doctest::Approx(0.7071f).epsilon(0.01));
		CHECK(y == doctest::Approx(0.7071f).epsilon(0.01));
		CHECK(std::sqrt(x * x + y * y) <= 1.0001f);

		// A diagonal whose axes are each within the dead zone but which leaves the disc moves the stick
		gamepads.SetState(0, WithAxis(WithAxis(Pad(), kX, 0.19f), kY, -0.19f));
		CHECK(gamepads.GetAxis(0, kX) > 0.0f);
		CHECK(gamepads.GetAxis(0, kY) > 0.0f);

		// The other stick is its own
		gamepads.SetState(0, WithAxis(Pad(), EGamepadAxis::GAMEPAD_AXIS_RIGHT_X, 1.0f));
		CHECK(gamepads.GetAxis(0, EGamepadAxis::GAMEPAD_AXIS_RIGHT_X) == doctest::Approx(1.0f));
		CHECK(gamepads.GetAxis(0, kX) == 0.0f);
	}

	TEST_CASE("the dead zone can be changed within its range")
	{
		constexpr auto kX = EGamepadAxis::GAMEPAD_AXIS_LEFT_X;
		Gamepads gamepads;

		gamepads.SetDeadZone(0.0f);
		gamepads.SetState(0, WithAxis(Pad(), kX, 0.1f));
		CHECK(gamepads.GetAxis(0, kX) == doctest::Approx(0.1f));

		gamepads.SetDeadZone(2.0f);
		CHECK(gamepads.GetDeadZone() == doctest::Approx(0.95f));
		gamepads.SetDeadZone(-1.0f);
		CHECK(gamepads.GetDeadZone() == 0.0f);
		gamepads.SetDeadZone(std::nanf(""));
		CHECK(gamepads.GetDeadZone() == 0.0f);
	}

	TEST_CASE("a trigger reads 0 at rest and 1 pressed fully")
	{
		constexpr auto kTrigger = EGamepadAxis::GAMEPAD_AXIS_RIGHT_TRIGGER;
		Gamepads gamepads;

		gamepads.SetState(0, Pad());
		CHECK(gamepads.GetAxis(0, kTrigger) == 0.0f);

		gamepads.SetState(0, WithAxis(Pad(), kTrigger, 1.0f));
		CHECK(gamepads.GetAxis(0, kTrigger) == doctest::Approx(1.0f));

		// Half its travel: -1 to 1 is 0 to 1, past a small dead zone
		gamepads.SetState(0, WithAxis(Pad(), kTrigger, 0.0f));
		CHECK(gamepads.GetAxis(0, kTrigger) == doctest::Approx((0.5f - Gamepads::kTriggerDeadZone) / (1.0f - Gamepads::kTriggerDeadZone)));

		// Anything that is not a number reads as rest
		gamepads.SetState(0, WithAxis(Pad(), EGamepadAxis::GAMEPAD_AXIS_LEFT_X, std::nanf("")));
		CHECK(gamepads.GetAxis(0, EGamepadAxis::GAMEPAD_AXIS_LEFT_X) == 0.0f);
	}
}
