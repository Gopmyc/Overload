/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <OvCore/Scripting/Lua/LuaComponentRef.h>

#include <OvCore/Input/InputActions.h>
#include <OvCore/Scripting/Lua/LuaApi.h>
#include <OvWindowing/Inputs/EKey.h>

using OvCore::Input::GetInputName;
using OvCore::Input::InputActions;
using OvCore::Input::InputId;
using OvCore::Input::ParseInput;

namespace
{
	/**
	* Made-up input: what is held, and what went down or up during the frame
	*/
	struct FakeSource final : OvCore::Input::IInputSource
	{
		std::map<std::string, float> values;
		std::set<std::string> pressed;
		std::set<std::string> released;

		float GetValue(const InputId& p_input) const override
		{
			const auto found = values.find(GetInputName(p_input));
			return found != values.end() ? found->second : 0.0f;
		}

		bool WasPressed(const InputId& p_input) const override { return pressed.contains(GetInputName(p_input)); }
		bool WasReleased(const InputId& p_input) const override { return released.contains(GetInputName(p_input)); }

		/**
		* The next frame: a button goes down or up, an axis takes a value
		*/
		void Press(const std::string& p_input) { pressed.insert(p_input); values[p_input] = 1.0f; }
		void Release(const std::string& p_input) { released.insert(p_input); values[p_input] = 0.0f; }
		void NextFrame() { pressed.clear(); released.clear(); }
	};

	constexpr std::string_view kDefinitions = R"({
		"actions": [
			{ "name": "Jump", "inputs": ["SPACE", "GAMEPAD_A"] },
			{ "name": "Attack", "inputs": ["MOUSE_LEFT", "GAMEPAD_RIGHT_TRIGGER"] },
			{ "name": "MoveRight", "inputs": ["D", "GAMEPAD_LEFT_X+"] },
			{ "name": "Interact", "inputs": ["E"] }
		]
	})";

	InputActions MakeActions()
	{
		std::vector<std::string> warnings;
		InputActions actions;
		actions.SetDefinitions(InputActions::ParseDefinitions(kDefinitions, warnings));
		CHECK(warnings.empty());
		return actions;
	}

	std::vector<std::string> Bindings(const InputActions& p_actions, std::string_view p_action)
	{
		std::vector<std::string> names;
		const auto bindings = p_actions.GetBindings(p_action);
		REQUIRE(bindings);
		for (const auto& binding : *bindings)
			names.push_back(binding ? GetInputName(*binding) : "");
		return names;
	}

	using Slots = std::vector<std::string>;
}

TEST_SUITE("Input actions")
{
	TEST_CASE("every input has a name that reads back, and the keys are the Lua enum Key's")
	{
		for (const InputId& input : OvCore::Input::GetAllInputs())
		{
			const std::string name = GetInputName(input);
			CAPTURE(name);
			REQUIRE_FALSE(name.empty());
			CHECK(ParseInput(name) == input);
		}

		CHECK_FALSE(ParseInput("NOPE"));
		CHECK_FALSE(ParseInput("UNKNOWN"));
		CHECK_FALSE(ParseInput(""));

		// The names a file uses for keys are those scripts know
		sol::state state;
		OvCore::Scripting::LuaApi::OpenLibraries(state);
		OvCore::Scripting::LuaApi::Bind(state);

		size_t luaKeys = 0;
		for (const std::string& described : OvCore::Scripting::LuaApi::Describe())
		{
			if (!described.starts_with("Key.") || described == "Key.UNKNOWN")
				continue;

			const std::string name = described.substr(4);
			CAPTURE(name);
			const auto input = ParseInput(name);
			REQUIRE(input);
			CHECK(input->device == InputId::EDevice::KEY);
			CHECK(input->code == state["Key"][name].get<int>());
			++luaKeys;
		}

		const auto keys = std::count_if(OvCore::Input::GetAllInputs().begin(), OvCore::Input::GetAllInputs().end(), [](const InputId& p_input)
		{
			return p_input.device == InputId::EDevice::KEY;
		});
		CHECK(luaKeys == static_cast<size_t>(keys));
	}

	TEST_CASE("a project's file declares its actions; what cannot be read is reported and skipped")
	{
		std::vector<std::string> warnings;
		const auto definitions = InputActions::ParseDefinitions(R"({ "actions": [
			{ "name": "Jump", "inputs": ["SPACE", "NOPE", 3] },
			{ "inputs": ["E"] },
			{ "name": "Jump", "inputs": ["J"] },
			{ "name": "Many", "inputs": ["A", "B", "C", "D", "E"] },
			{ "name": "Bare" },
			{ "name": "Odd", "inputs": "SPACE" }
		] })", warnings);

		REQUIRE(definitions.size() == 4);
		CHECK(definitions[0].name == "Jump");
		CHECK(definitions[0].inputs.size() == 1);
		CHECK(definitions[1].name == "Many");
		CHECK(definitions[1].inputs.size() == InputActions::kSlotCount);
		CHECK(definitions[2].inputs.empty());
		CHECK(warnings.size() == 6);

		warnings.clear();
		CHECK(InputActions::ParseDefinitions("{ \"actions\": ", warnings).empty());
		CHECK(warnings.size() == 1);

		warnings.clear();
		CHECK(InputActions::ParseDefinitions("{ \"other\": [] }", warnings).empty());
		CHECK(warnings.front() == "no \"actions\" array");
	}

	TEST_CASE("an action is pressed on the frame one of its inputs goes down, held, then released")
	{
		InputActions actions = MakeActions();
		FakeSource source;

		CHECK(actions.GetActionNames() == std::vector<std::string>{ "Jump", "Attack", "MoveRight", "Interact" });
		CHECK(actions.HasAction("Jump"));
		CHECK_FALSE(actions.HasAction("Fly"));

		actions.Update(source);
		CHECK_FALSE(actions.IsHeld("Jump"));

		source.Press("SPACE");
		actions.Update(source);
		CHECK(actions.IsPressed("Jump"));
		CHECK(actions.IsHeld("Jump"));
		CHECK(actions.GetValue("Jump") == 1.0f);

		// A second input of the same action changes nothing
		source.NextFrame();
		source.Press("GAMEPAD_A");
		actions.Update(source);
		CHECK_FALSE(actions.IsPressed("Jump"));
		CHECK(actions.IsHeld("Jump"));

		// Released by one, still held by the other
		source.NextFrame();
		source.Release("SPACE");
		actions.Update(source);
		CHECK(actions.IsHeld("Jump"));
		CHECK_FALSE(actions.IsReleased("Jump"));

		source.NextFrame();
		source.Release("GAMEPAD_A");
		actions.Update(source);
		CHECK(actions.IsReleased("Jump"));
		CHECK_FALSE(actions.IsHeld("Jump"));

		source.NextFrame();
		actions.Update(source);
		CHECK_FALSE(actions.IsReleased("Jump"));

		// An action no input moves, or that the project does not declare, reads as released
		CHECK_FALSE(actions.IsHeld("Interact"));
		CHECK_FALSE(actions.IsHeld("Fly"));
		CHECK(actions.GetValue("Fly") == 0.0f);
	}

	TEST_CASE("a tap within a frame both presses and releases its action")
	{
		InputActions actions = MakeActions();
		FakeSource source;

		source.pressed.insert("E");
		source.released.insert("E");
		actions.Update(source);

		CHECK(actions.IsPressed("Interact"));
		CHECK(actions.IsReleased("Interact"));
		CHECK_FALSE(actions.IsHeld("Interact"));
	}

	TEST_CASE("a stick's way and a trigger push their actions from 0 to 1, held from halfway")
	{
		InputActions actions = MakeActions();
		FakeSource source;

		source.values["GAMEPAD_LEFT_X+"] = 0.3f;
		actions.Update(source);
		CHECK(actions.GetValue("MoveRight") == doctest::Approx(0.3f));
		CHECK_FALSE(actions.IsHeld("MoveRight"));

		source.values["GAMEPAD_LEFT_X+"] = 0.8f;
		actions.Update(source);
		CHECK(actions.IsPressed("MoveRight"));
		CHECK(actions.GetValue("MoveRight") == doctest::Approx(0.8f));

		// A key held pushes it fully
		source.values["D"] = 1.0f;
		actions.Update(source);
		CHECK(actions.GetValue("MoveRight") == 1.0f);
		CHECK_FALSE(actions.IsPressed("MoveRight"));

		source.values["D"] = 0.0f;
		source.values["GAMEPAD_LEFT_X+"] = 0.1f;
		actions.Update(source);
		CHECK(actions.IsReleased("MoveRight"));

		source.values["GAMEPAD_RIGHT_TRIGGER"] = 0.6f;
		actions.Update(source);
		CHECK(actions.IsPressed("Attack"));
	}

	TEST_CASE("the input pressed during a frame is told, keys before buttons, buttons before axes")
	{
		InputActions actions = MakeActions();
		FakeSource source;

		actions.Update(source);
		CHECK_FALSE(actions.GetPressedInput());

		source.Press("GAMEPAD_B");
		source.Press("K");
		actions.Update(source);
		REQUIRE(actions.GetPressedInput());
		CHECK(GetInputName(*actions.GetPressedInput()) == "K");

		source.NextFrame();
		source.values["GAMEPAD_RIGHT_Y-"] = 0.9f;
		actions.Update(source);
		REQUIRE(actions.GetPressedInput());
		CHECK(GetInputName(*actions.GetPressedInput()) == "GAMEPAD_RIGHT_Y-");

		// Held, it is pressed no more
		actions.Update(source);
		CHECK_FALSE(actions.GetPressedInput());
	}

	TEST_CASE("binding an input another slot holds swaps them, and a reset brings the defaults back")
	{
		InputActions actions = MakeActions();
		std::string error;

		CHECK(Bindings(actions, "Jump") == Slots{ "SPACE", "GAMEPAD_A", "", "" });

		// E belongs to Interact: Interact gets back what Jump's first slot held
		REQUIRE(actions.SetBinding("Jump", 0, ParseInput("E"), error));
		CHECK(Bindings(actions, "Jump") == Slots{ "E", "GAMEPAD_A", "", "" });
		CHECK(Bindings(actions, "Interact") == Slots{ "SPACE", "", "", "" });

		// Within an action, two slots swap
		REQUIRE(actions.SetBinding("Jump", 3, ParseInput("GAMEPAD_A"), error));
		CHECK(Bindings(actions, "Jump") == Slots{ "E", "", "", "GAMEPAD_A" });

		// Emptied
		REQUIRE(actions.SetBinding("Jump", 3, std::nullopt, error));
		CHECK(Bindings(actions, "Jump") == Slots{ "E", "", "", "" });

		CHECK_FALSE(actions.SetBinding("Fly", 0, ParseInput("F"), error));
		CHECK(error.find("no action named \"Fly\"") != std::string::npos);
		CHECK_FALSE(actions.SetBinding("Jump", 4, ParseInput("F"), error));
		CHECK(error.find("no slot 5") != std::string::npos);

		// One action back to its defaults takes them from whoever holds them
		actions.ResetBindings("Jump");
		CHECK(Bindings(actions, "Jump") == Slots{ "SPACE", "GAMEPAD_A", "", "" });
		CHECK(Bindings(actions, "Interact") == Slots{ "", "", "", "" });

		actions.ResetBindings();
		CHECK(Bindings(actions, "Interact") == Slots{ "E", "", "", "" });
		CHECK_FALSE(actions.GetBindings("Fly"));
	}

	TEST_CASE("the bindings saved read back; what the file gets wrong keeps the defaults")
	{
		InputActions actions = MakeActions();
		std::string error;
		REQUIRE(actions.SetBinding("Attack", 1, ParseInput("MOUSE_RIGHT"), error));
		REQUIRE(actions.SetBinding("MoveRight", 0, ParseInput("RIGHT"), error));

		const std::string saved = actions.SaveBindings();
		CHECK(saved.find("\"MOUSE_RIGHT\"") != std::string::npos);

		InputActions reloaded = MakeActions();
		std::vector<std::string> warnings;
		reloaded.LoadBindings(saved, warnings);
		CHECK(warnings.empty());
		CHECK(Bindings(reloaded, "Attack") == Slots{ "MOUSE_LEFT", "MOUSE_RIGHT", "", "" });
		CHECK(Bindings(reloaded, "MoveRight") == Slots{ "RIGHT", "GAMEPAD_LEFT_X+", "", "" });

		// An action gone, an input unknown, and an input bound twice: the first holder keeps it
		InputActions other = MakeActions();
		other.LoadBindings(R"({ "bindings": [
			{ "name": "Gone", "inputs": ["X"] },
			{ "name": "Jump", "inputs": ["NOPE", "E"] },
			{ "name": "Interact", "inputs": ["E"] }
		] })", warnings);

		CHECK(warnings.size() == 2);
		CHECK(Bindings(other, "Jump") == Slots{ "SPACE", "E", "", "" });
		CHECK(Bindings(other, "Interact") == Slots{ "", "", "", "" });

		warnings.clear();
		other.LoadBindings("not json", warnings);
		CHECK(warnings.size() == 1);
	}

	TEST_CASE("an input two actions declare stays with the first")
	{
		std::vector<std::string> warnings;
		InputActions actions;
		actions.SetDefinitions(InputActions::ParseDefinitions(R"({ "actions": [
			{ "name": "A", "inputs": ["SPACE"] },
			{ "name": "B", "inputs": ["SPACE", "B"] }
		] })", warnings));

		CHECK(Bindings(actions, "A") == Slots{ "SPACE", "", "", "" });
		CHECK(Bindings(actions, "B") == Slots{ "", "B", "", "" });

		// What the reset brings back is that, too
		actions.ResetBindings();
		CHECK(Bindings(actions, "B") == Slots{ "", "B", "", "" });
	}
}
