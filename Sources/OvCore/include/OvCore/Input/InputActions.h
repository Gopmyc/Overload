/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <OvCore/Input/InputId.h>

namespace OvCore::Input
{
	/**
	* Where the input actions read their inputs: the game's input in the engine, made-up input in the tests
	*/
	class IInputSource
	{
	public:
		virtual ~IInputSource() = default;

		/**
		* Returns how far the input is pushed: 1 or 0 for a key or a button, 0 to 1 for a trigger or one way of a stick
		* @param p_input
		*/
		virtual float GetValue(const InputId& p_input) const = 0;

		/**
		* Returns true if the key or the button went down during the frame (an axis is never asked)
		* @param p_input
		*/
		virtual bool WasPressed(const InputId& p_input) const = 0;

		/**
		* Returns true if the key or the button went up during the frame (an axis is never asked)
		* @param p_input
		*/
		virtual bool WasReleased(const InputId& p_input) const = 0;
	};

	/**
	* An action as a project declares it: a name, and the inputs it is bound to by default
	*/
	struct ActionDefinition
	{
		std::string name;
		std::vector<InputId> inputs;
	};

	/**
	* The project's input actions ("Jump", "Attack"), each bound to up to kSlotCount inputs that the player can change.
	* Gameplay asks whether an action is held, pressed or released rather than reading keys, and works whatever the
	* keyboard, mouse or gamepad input bound to it
	*/
	class InputActions
	{
	public:
		static constexpr size_t kSlotCount = 4;

		// An axis pushed this far holds its action
		static constexpr float kPressThreshold = 0.5f;

		using Bindings = std::array<std::optional<InputId>, kSlotCount>;

		/**
		* Reads a project's actions file: {"actions": [{"name": "Jump", "inputs": ["SPACE", "GAMEPAD_A"]}]}. What it
		* cannot read (a bad name, an unknown input, an action named twice, too many inputs) is reported in p_warnings
		* and skipped; a file it cannot read at all gives no action
		* @param p_text
		* @param p_warnings
		*/
		static std::vector<ActionDefinition> ParseDefinitions(std::string_view p_text, std::vector<std::string>& p_warnings);

		/**
		* Replaces the actions: each is bound to its default inputs, and none is held. An input bound twice stays only
		* where it comes first
		* @param p_definitions
		*/
		void SetDefinitions(const std::vector<ActionDefinition>& p_definitions);

		/**
		* Reads the inputs for this frame. Called once a frame, before the scripts
		* @param p_source
		*/
		void Update(const IInputSource& p_source);

		/**
		* Returns true if the project declares the action
		* @param p_action
		*/
		bool HasAction(std::string_view p_action) const;

		/**
		* Returns the actions' names, in the order the project declares them
		*/
		std::vector<std::string> GetActionNames() const;

		/**
		* Returns true if one of the action's inputs is held (an axis past kPressThreshold)
		* @param p_action
		*/
		bool IsHeld(std::string_view p_action) const;

		/**
		* Returns true if the action began to be held during the frame
		* @param p_action
		*/
		bool IsPressed(std::string_view p_action) const;

		/**
		* Returns true if the action stopped being held during the frame
		* @param p_action
		*/
		bool IsReleased(std::string_view p_action) const;

		/**
		* Returns how far the action is pushed, 0 to 1: the furthest of its inputs
		* @param p_action
		*/
		float GetValue(std::string_view p_action) const;

		/**
		* Returns the inputs the action is bound to, slot by slot, or nullopt for an action the project does not declare
		* @param p_action
		*/
		std::optional<Bindings> GetBindings(std::string_view p_action) const;

		/**
		* Binds a slot of an action to an input, or empties it. An input another slot holds, of this action or another,
		* moves here, and that slot gets back what this one held. Returns false and says why when it cannot
		* @param p_action
		* @param p_slot (0 to kSlotCount - 1)
		* @param p_input
		* @param p_error
		*/
		bool SetBinding(std::string_view p_action, size_t p_slot, const std::optional<InputId>& p_input, std::string& p_error);

		/**
		* Binds an action, or every action without one given, to its default inputs again
		* @param p_action
		*/
		void ResetBindings(std::optional<std::string_view> p_action = std::nullopt);

		/**
		* Returns the first input, in GetAllInputs' order, that went down during the frame: what a menu waits for to bind
		* an action
		*/
		std::optional<InputId> GetPressedInput() const;

		/**
		* Writes the bindings as JSON, every action with its slots: {"bindings": [{"name": "Jump", "inputs": ["SPACE",
		* "", "GAMEPAD_A", ""]}]}
		*/
		std::string SaveBindings() const;

		/**
		* Reads bindings SaveBindings wrote. An action the file lacks keeps its bindings; a slot whose input is unknown
		* keeps its default; an action the project no longer declares is skipped. What cannot be read is reported in
		* p_warnings. An input bound twice stays only where it comes first
		* @param p_text
		* @param p_warnings
		*/
		void LoadBindings(std::string_view p_text, std::vector<std::string>& p_warnings);

	private:
		struct Action
		{
			std::string name;
			Bindings defaults;
			Bindings bindings;
			bool held = false;
			bool pressed = false;
			bool released = false;
			float value = 0.0f;
		};

		Action* Find(std::string_view p_action);
		const Action* Find(std::string_view p_action) const;
		void RemoveDuplicates();

	private:
		std::vector<Action> m_actions;
		std::vector<float> m_axisValues;
		std::optional<InputId> m_pressedInput;
	};
}
