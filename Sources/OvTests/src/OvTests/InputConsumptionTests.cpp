/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <map>
#include <set>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <OvCore/Input/InputActions.h>
#include <OvCore/Input/InputConsumption.h>

using OvCore::Input::GetInputName;
using OvCore::Input::InputActions;
using OvCore::Input::InputConsumption;
using OvCore::Input::InputId;

namespace
{
	using State = InputConsumption::State;

	InputId Named(const std::string& p_name)
	{
		const auto input = OvCore::Input::ParseInput(p_name);
		REQUIRE_MESSAGE(input, p_name);
		return *input;
	}

	/**
	* Made-up devices, as the game would see them without consumption: what each input is doing this frame
	*/
	struct Devices
	{
		std::map<std::string, State> states;

		void Hold(const std::string& p_input) { states[p_input] = { 1.0f, false, false }; }
		void Press(const std::string& p_input) { states[p_input] = { 1.0f, true, false }; }
		void Release(const std::string& p_input) { states[p_input] = { 0.0f, false, true }; }
		void Tap(const std::string& p_input) { states[p_input] = { 0.0f, true, true }; }
		void Push(const std::string& p_input, float p_value) { states[p_input] = { p_value, false, false }; }
		void Idle(const std::string& p_input) { states.erase(p_input); }

		State Read(const InputId& p_input) const
		{
			const auto found = states.find(GetInputName(p_input));
			return found != states.end() ? found->second : State{};
		}

		InputConsumption::StateReader Reader() const
		{
			return [this](const InputId& p_input, int) { return Read(p_input); };
		}
	};

	/**
	* The devices as the game reads them once the consumption leaves its inputs out, as InputHelpers does
	*/
	struct ConsumedSource final : OvCore::Input::IInputSource
	{
		const Devices& devices;
		const InputConsumption& consumption;

		ConsumedSource(const Devices& p_devices, const InputConsumption& p_consumption) : devices(p_devices), consumption(p_consumption) {}

		float GetValue(const InputId& p_input) const override { return consumption.IsHidden(p_input, 0) ? 0.0f : devices.Read(p_input).value; }
		bool WasPressed(const InputId& p_input) const override { return !consumption.IsHidden(p_input, 0) && devices.Read(p_input).pressed; }

		bool WasReleased(const InputId& p_input) const override
		{
			return consumption.IsReleasedForGame(p_input, 0) || (!consumption.IsHidden(p_input, 0) && devices.Read(p_input).released);
		}
	};

	InputActions MakeActions()
	{
		std::vector<std::string> warnings;
		InputActions actions;
		actions.SetDefinitions(InputActions::ParseDefinitions(R"({
			"actions": [
				{ "name": "Jump", "inputs": ["SPACE", "GAMEPAD_A"] },
				{ "name": "Click", "inputs": ["MOUSE_LEFT"] },
				{ "name": "MoveRight", "inputs": ["D", "GAMEPAD_LEFT_X+"] }
			]
		})", warnings));
		CHECK(warnings.empty());
		return actions;
	}
}

TEST_SUITE("Input consumption")
{
	TEST_CASE("a button consumed as it goes down stays hidden until released, its release included")
	{
		Devices devices;
		InputConsumption consumption;
		const InputId click = Named("MOUSE_LEFT");

		devices.Press("MOUSE_LEFT");
		consumption.BeginFrame(devices.Reader());
		CHECK(consumption.Consume(click, 0, devices.Read(click)));
		CHECK(consumption.IsHidden(click, 0));
		CHECK_FALSE(consumption.IsReleasedForGame(click, 0)); // The game never saw it down

		// Consumed again, by a UI that does so every frame, it stays as it is
		CHECK_FALSE(consumption.Consume(click, 0, devices.Read(click)));

		devices.Hold("MOUSE_LEFT");
		consumption.BeginFrame(devices.Reader());
		CHECK(consumption.IsHidden(click, 0));

		devices.Release("MOUSE_LEFT");
		consumption.BeginFrame(devices.Reader());
		CHECK(consumption.IsHidden(click, 0));

		devices.Idle("MOUSE_LEFT");
		consumption.BeginFrame(devices.Reader());
		CHECK_FALSE(consumption.IsHidden(click, 0));

		// Other inputs are left alone
		CHECK_FALSE(consumption.IsHidden(Named("MOUSE_RIGHT"), 0));
	}

	TEST_CASE("a button the game saw held goes up for it when consumed")
	{
		Devices devices;
		InputConsumption consumption;
		const InputId space = Named("SPACE");

		devices.Hold("SPACE");
		consumption.BeginFrame(devices.Reader());
		CHECK(consumption.Consume(space, 0, devices.Read(space)));
		CHECK(consumption.IsHidden(space, 0));
		CHECK(consumption.IsReleasedForGame(space, 0));

		consumption.BeginFrame(devices.Reader());
		CHECK(consumption.IsHidden(space, 0));
		CHECK_FALSE(consumption.IsReleasedForGame(space, 0));
	}

	TEST_CASE("what is up is not consumed, and a tap within a frame is hidden for that frame")
	{
		Devices devices;
		InputConsumption consumption;
		const InputId space = Named("SPACE");

		consumption.BeginFrame(devices.Reader());
		CHECK_FALSE(consumption.Consume(space, 0, devices.Read(space)));
		CHECK_FALSE(consumption.IsHidden(space, 0));

		// Released this frame after the game saw it held: the release still reaches the game
		devices.Release("SPACE");
		CHECK_FALSE(consumption.Consume(space, 0, devices.Read(space)));

		devices.Tap("SPACE");
		consumption.BeginFrame(devices.Reader());
		CHECK(consumption.Consume(space, 0, devices.Read(space)));
		CHECK(consumption.IsHidden(space, 0));
		CHECK_FALSE(consumption.IsReleasedForGame(space, 0));

		devices.Idle("SPACE");
		consumption.BeginFrame(devices.Reader());
		CHECK_FALSE(consumption.IsHidden(space, 0));
	}

	TEST_CASE("a press afresh is a new input")
	{
		Devices devices;
		InputConsumption consumption;
		const InputId space = Named("SPACE");

		devices.Press("SPACE");
		consumption.BeginFrame(devices.Reader());
		consumption.Consume(space, 0, devices.Read(space));

		// Released and pressed again between two frames
		devices.Press("SPACE");
		consumption.BeginFrame(devices.Reader());
		CHECK_FALSE(consumption.IsHidden(space, 0));
	}

	TEST_CASE("a gamepad input is consumed on its pad, a key or a mouse button on every one")
	{
		InputConsumption consumption;
		const InputId a = Named("GAMEPAD_A");
		const InputId space = Named("SPACE");

		CHECK(consumption.Consume(a, 1, { 1.0f, true, false }));
		CHECK(consumption.IsHidden(a, 1));
		CHECK_FALSE(consumption.IsHidden(a, 0));

		CHECK(consumption.Consume(space, 2, { 1.0f, true, false }));
		CHECK(consumption.IsHidden(space, 0));
		CHECK(consumption.IsHidden(space, 3));
	}

	TEST_CASE("a way of a stick stays hidden until the stick is back to the centre")
	{
		Devices devices;
		InputConsumption consumption;
		const InputId right = Named("GAMEPAD_LEFT_X+");
		const InputId left = Named("GAMEPAD_LEFT_X-");
		const InputId trigger = Named("GAMEPAD_RIGHT_TRIGGER");

		devices.Push("GAMEPAD_LEFT_X+", 0.8f);
		devices.Push("GAMEPAD_RIGHT_TRIGGER", 0.1f);
		consumption.BeginFrame(devices.Reader());
		CHECK(consumption.Consume(right, 0, devices.Read(right)));
		CHECK(consumption.Consume(trigger, 0, devices.Read(trigger)));
		CHECK_FALSE(consumption.Consume(left, 0, devices.Read(left)));
		CHECK_FALSE(consumption.IsReleasedForGame(right, 0)); // An axis has no edge of its own

		devices.Push("GAMEPAD_LEFT_X+", 0.3f);
		consumption.BeginFrame(devices.Reader());
		CHECK(consumption.IsHidden(right, 0));
		CHECK_FALSE(consumption.IsHidden(left, 0));

		devices.Idle("GAMEPAD_LEFT_X+");
		devices.Idle("GAMEPAD_RIGHT_TRIGGER");
		consumption.BeginFrame(devices.Reader());
		CHECK_FALSE(consumption.IsHidden(right, 0));
		CHECK_FALSE(consumption.IsHidden(trigger, 0));
	}

	TEST_CASE("a captured keyboard hides every key, and frees them one by one as they are released")
	{
		Devices devices;
		InputConsumption consumption;
		const InputId w = Named("W");
		const InputId e = Named("E");
		const InputId space = Named("SPACE");
		const InputId a = Named("A");

		devices.Hold("W");
		devices.Press("E");
		devices.Hold("MOUSE_LEFT");
		consumption.BeginFrame(devices.Reader());
		consumption.SetKeyboardCaptured(true, devices.Reader());
		CHECK(consumption.IsKeyboardCaptured());

		// The key the game saw held goes up for it; the one pressed this frame was never seen
		CHECK(consumption.IsReleasedForGame(w, 0));
		CHECK_FALSE(consumption.IsReleasedForGame(e, 0));
		CHECK(consumption.IsHidden(w, 0));
		CHECK(consumption.IsHidden(a, 0));
		CHECK_FALSE(consumption.IsHidden(Named("MOUSE_LEFT"), 0));
		CHECK_FALSE(consumption.IsHidden(Named("GAMEPAD_A"), 0));

		// Captured again, nothing is released twice
		consumption.BeginFrame(devices.Reader());
		consumption.SetKeyboardCaptured(true, devices.Reader());
		CHECK_FALSE(consumption.IsReleasedForGame(w, 0));
		CHECK(consumption.IsHidden(a, 0));

		// Freed while W is held and SPACE goes down: both were meant for what captured the keyboard
		devices.Idle("E");
		devices.Press("SPACE");
		consumption.BeginFrame(devices.Reader());
		consumption.SetKeyboardCaptured(false, devices.Reader());
		CHECK_FALSE(consumption.IsKeyboardCaptured());
		CHECK(consumption.IsHidden(w, 0));
		CHECK(consumption.IsHidden(space, 0));
		CHECK_FALSE(consumption.IsHidden(e, 0));
		CHECK_FALSE(consumption.IsHidden(a, 0));

		devices.Release("W");
		devices.Hold("SPACE");
		consumption.BeginFrame(devices.Reader());
		CHECK(consumption.IsHidden(w, 0));
		CHECK(consumption.IsHidden(space, 0));

		devices.Idle("W");
		consumption.BeginFrame(devices.Reader());
		CHECK_FALSE(consumption.IsHidden(w, 0));
		CHECK(consumption.IsHidden(space, 0));
	}

	TEST_CASE("the scroll is consumed for one frame, and a reset forgets everything")
	{
		Devices devices;
		InputConsumption consumption;

		consumption.ConsumeMouseScroll();
		CHECK(consumption.IsMouseScrollConsumed());
		consumption.BeginFrame(devices.Reader());
		CHECK_FALSE(consumption.IsMouseScrollConsumed());

		devices.Hold("MOUSE_LEFT");
		consumption.Consume(Named("MOUSE_LEFT"), 0, devices.Read(Named("MOUSE_LEFT")));
		consumption.SetKeyboardCaptured(true, devices.Reader());
		consumption.ConsumeMouseScroll();
		consumption.Reset();
		CHECK_FALSE(consumption.IsHidden(Named("MOUSE_LEFT"), 0));
		CHECK_FALSE(consumption.IsReleasedForGame(Named("MOUSE_LEFT"), 0));
		CHECK_FALSE(consumption.IsKeyboardCaptured());
		CHECK_FALSE(consumption.IsHidden(Named("W"), 0));
		CHECK_FALSE(consumption.IsMouseScrollConsumed());
	}

	TEST_CASE("actions read again leave out an input consumed during the frame")
	{
		Devices devices;
		InputConsumption consumption;
		InputActions actions = MakeActions();
		const ConsumedSource source(devices, consumption);

		const auto frame = [&]() { consumption.BeginFrame(devices.Reader()); actions.Update(source); };
		const auto consume = [&](const std::string& p_input) { consumption.Consume(Named(p_input), 0, devices.Read(Named(p_input))); actions.Refresh(source); };

		SUBCASE("held before the frame, the action is released")
		{
			devices.Press("SPACE");
			frame();
			CHECK(actions.IsPressed("Jump"));

			devices.Hold("SPACE");
			frame();
			REQUIRE(actions.IsHeld("Jump"));

			consume("SPACE");
			CHECK_FALSE(actions.IsHeld("Jump"));
			CHECK_FALSE(actions.IsPressed("Jump"));
			CHECK(actions.IsReleased("Jump"));
			CHECK(actions.GetValue("Jump") == 0.0f);

			// Still held, still hidden: nothing more happens to the action
			frame();
			CHECK_FALSE(actions.IsHeld("Jump"));
			CHECK_FALSE(actions.IsPressed("Jump"));
			CHECK_FALSE(actions.IsReleased("Jump"));

			devices.Release("SPACE");
			frame();
			CHECK_FALSE(actions.IsReleased("Jump"));

			// Pressed again once up, it is the game's
			devices.Press("SPACE");
			frame();
			CHECK(actions.IsPressed("Jump"));
		}

		SUBCASE("consumed as it goes down, the action never moves")
		{
			devices.Press("MOUSE_LEFT");
			frame();
			REQUIRE(actions.IsPressed("Click"));
			REQUIRE(actions.GetPressedInput() == Named("MOUSE_LEFT"));

			consume("MOUSE_LEFT");
			CHECK_FALSE(actions.IsPressed("Click"));
			CHECK_FALSE(actions.IsHeld("Click"));
			CHECK_FALSE(actions.IsReleased("Click"));
			CHECK_FALSE(actions.GetPressedInput());

			devices.Release("MOUSE_LEFT");
			frame();
			CHECK_FALSE(actions.IsReleased("Click"));
		}

		SUBCASE("a stick's way, consumed, releases the action until the stick is back")
		{
			devices.Push("GAMEPAD_LEFT_X+", 0.9f);
			frame();
			devices.Push("GAMEPAD_LEFT_X+", 0.9f);
			frame();
			REQUIRE(actions.IsHeld("MoveRight"));

			consume("GAMEPAD_LEFT_X+");
			CHECK(actions.IsReleased("MoveRight"));
			CHECK(actions.GetValue("MoveRight") == 0.0f);

			// The other input of the action still reads
			devices.Press("D");
			frame();
			CHECK(actions.IsPressed("MoveRight"));
			CHECK(actions.GetValue("MoveRight") == 1.0f);
		}

		SUBCASE("the captured keyboard releases what it held, and the mouse still reads")
		{
			devices.Press("SPACE");
			devices.Press("MOUSE_LEFT");
			frame();
			devices.Hold("SPACE");
			devices.Hold("MOUSE_LEFT");
			frame();

			consumption.SetKeyboardCaptured(true, devices.Reader());
			actions.Refresh(source);
			CHECK(actions.IsReleased("Jump"));
			CHECK(actions.IsHeld("Click"));
			CHECK_FALSE(actions.IsReleased("Click"));
		}
	}
}
