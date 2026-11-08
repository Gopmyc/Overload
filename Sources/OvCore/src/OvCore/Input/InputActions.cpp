/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <format>

#include <OvCore/Input/InputActions.h>
#include <OvTools/Utils/Json.h>

namespace Json = OvTools::Utils::Json;

std::vector<OvCore::Input::ActionDefinition> OvCore::Input::InputActions::ParseDefinitions(std::string_view p_text, std::vector<std::string>& p_warnings)
{
	std::string error;
	const auto root = Json::Parse(p_text, error);

	if (!root)
	{
		p_warnings.push_back(error);
		return {};
	}

	const Json::Value* actions = root->Find("actions");
	const Json::Array* list = actions ? actions->AsArray() : nullptr;

	if (!list)
	{
		p_warnings.push_back("no \"actions\" array");
		return {};
	}

	std::vector<ActionDefinition> definitions;

	for (size_t index = 0; index < list->size(); ++index)
	{
		const Json::Value& entry = (*list)[index];
		const Json::Value* name = entry.Find("name");
		const std::string* text = name ? name->AsString() : nullptr;

		if (!text || text->empty())
		{
			p_warnings.push_back(std::format("action {}: no name, skipped", index + 1));
			continue;
		}

		if (std::any_of(definitions.begin(), definitions.end(), [text](const ActionDefinition& p_definition) { return p_definition.name == *text; }))
		{
			p_warnings.push_back(std::format("action \"{}\": declared twice, the first is kept", *text));
			continue;
		}

		ActionDefinition definition{ *text, {} };

		if (const Json::Value* inputs = entry.Find("inputs"))
		{
			const Json::Array* names = inputs->AsArray();

			if (!names)
			{
				p_warnings.push_back(std::format("action \"{}\": \"inputs\" is not an array", *text));
			}
			else
			{
				for (const Json::Value& input : *names)
				{
					const std::string* inputName = input.AsString();
					const auto parsed = inputName ? ParseInput(*inputName) : std::nullopt;

					if (!parsed)
					{
						p_warnings.push_back(std::format("action \"{}\": no input named {}", *text, inputName ? "\"" + *inputName + "\"" : "by a value that is not a string"));
					}
					else if (definition.inputs.size() == kSlotCount)
					{
						p_warnings.push_back(std::format("action \"{}\": more than {} inputs, \"{}\" is left out", *text, kSlotCount, *inputName));
					}
					else
					{
						definition.inputs.push_back(*parsed);
					}
				}
			}
		}

		definitions.push_back(std::move(definition));
	}

	return definitions;
}

void OvCore::Input::InputActions::SetDefinitions(const std::vector<ActionDefinition>& p_definitions)
{
	m_actions.clear();
	m_pressedInput.reset();

	for (const ActionDefinition& definition : p_definitions)
	{
		Action action;
		action.name = definition.name;

		for (size_t slot = 0; slot < std::min(definition.inputs.size(), kSlotCount); ++slot)
		{
			action.bindings[slot] = definition.inputs[slot];
		}

		m_actions.push_back(std::move(action));
	}

	RemoveDuplicates();

	for (Action& action : m_actions)
	{
		action.defaults = action.bindings;
	}
}

void OvCore::Input::InputActions::Update(const IInputSource& p_source)
{
	for (Action& action : m_actions)
	{
		action.wasHeld = action.held;
	}

	m_previousValues = m_values;
	Evaluate(p_source);
}

void OvCore::Input::InputActions::Refresh(const IInputSource& p_source)
{
	Evaluate(p_source);
}

void OvCore::Input::InputActions::Evaluate(const IInputSource& p_source)
{
	const std::vector<InputId>& inputs = GetAllInputs();
	m_previousValues.resize(inputs.size(), 0.0f);
	m_values.assign(inputs.size(), 0.0f);

	std::vector<bool> down(inputs.size());
	std::vector<bool> up(inputs.size());

	m_pressedInput.reset();

	for (size_t index = 0; index < inputs.size(); ++index)
	{
		const InputId& input = inputs[index];
		m_values[index] = std::clamp(p_source.GetValue(input), 0.0f, 1.0f);

		if (input.IsButton())
		{
			down[index] = p_source.WasPressed(input);
			up[index] = p_source.WasReleased(input);
		}
		else
		{
			// An axis is pressed when it crosses the threshold
			const float previous = m_previousValues[index];
			down[index] = previous < kPressThreshold && m_values[index] >= kPressThreshold;
			up[index] = previous >= kPressThreshold && m_values[index] < kPressThreshold;
		}

		if (down[index] && !m_pressedInput)
		{
			m_pressedInput = input;
		}
	}

	const auto indexOf = [&inputs](const InputId& p_input)
	{
		return static_cast<size_t>(std::find(inputs.begin(), inputs.end(), p_input) - inputs.begin());
	};

	for (Action& action : m_actions)
	{
		bool anyDown = false;
		bool anyUp = false;

		action.held = false;
		action.value = 0.0f;

		for (const auto& binding : action.bindings)
		{
			if (!binding)
			{
				continue;
			}

			const size_t index = indexOf(*binding);
			if (index >= inputs.size())
			{
				continue;
			}

			action.value = std::max(action.value, m_values[index]);
			action.held = action.held || m_values[index] >= kPressThreshold;
			anyDown = anyDown || down[index];
			anyUp = anyUp || up[index];
		}

		// A tap within a frame both presses and releases the action
		action.pressed = !action.wasHeld && (action.held || anyDown);
		action.released = !action.held && (action.wasHeld || (anyDown && anyUp));
	}
}

OvCore::Input::InputActions::Action* OvCore::Input::InputActions::Find(std::string_view p_action)
{
	const auto found = std::find_if(m_actions.begin(), m_actions.end(), [p_action](const Action& p_candidate) { return p_candidate.name == p_action; });
	return found != m_actions.end() ? &*found : nullptr;
}

const OvCore::Input::InputActions::Action* OvCore::Input::InputActions::Find(std::string_view p_action) const
{
	const auto found = std::find_if(m_actions.begin(), m_actions.end(), [p_action](const Action& p_candidate) { return p_candidate.name == p_action; });
	return found != m_actions.end() ? &*found : nullptr;
}

bool OvCore::Input::InputActions::HasAction(std::string_view p_action) const
{
	return Find(p_action) != nullptr;
}

std::vector<std::string> OvCore::Input::InputActions::GetActionNames() const
{
	std::vector<std::string> names;

	for (const Action& action : m_actions)
	{
		names.push_back(action.name);
	}

	return names;
}

bool OvCore::Input::InputActions::IsHeld(std::string_view p_action) const
{
	const Action* action = Find(p_action);
	return action && action->held;
}

bool OvCore::Input::InputActions::IsPressed(std::string_view p_action) const
{
	const Action* action = Find(p_action);
	return action && action->pressed;
}

bool OvCore::Input::InputActions::IsReleased(std::string_view p_action) const
{
	const Action* action = Find(p_action);
	return action && action->released;
}

float OvCore::Input::InputActions::GetValue(std::string_view p_action) const
{
	const Action* action = Find(p_action);
	return action ? action->value : 0.0f;
}

std::optional<OvCore::Input::InputActions::Bindings> OvCore::Input::InputActions::GetBindings(std::string_view p_action) const
{
	const Action* action = Find(p_action);
	return action ? std::optional<Bindings>(action->bindings) : std::nullopt;
}

bool OvCore::Input::InputActions::SetBinding(std::string_view p_action, size_t p_slot, const std::optional<InputId>& p_input, std::string& p_error)
{
	Action* action = Find(p_action);

	if (!action)
	{
		p_error = std::format("no action named \"{}\"", p_action);
		return false;
	}

	if (p_slot >= kSlotCount)
	{
		p_error = std::format("no slot {}: an action has {}", p_slot + 1, kSlotCount);
		return false;
	}

	std::optional<InputId>& slot = action->bindings[p_slot];

	if (slot == p_input)
	{
		return true;
	}

	// Whoever holds the input gets back what this slot held
	if (p_input)
	{
		for (Action& other : m_actions)
		{
			for (auto& binding : other.bindings)
			{
				if (&binding != &slot && binding == p_input)
				{
					binding = slot;
				}
			}
		}
	}

	slot = p_input;
	return true;
}

void OvCore::Input::InputActions::ResetBindings(std::optional<std::string_view> p_action)
{
	if (!p_action)
	{
		for (Action& action : m_actions)
		{
			action.bindings = action.defaults;
		}

		return;
	}

	Action* action = Find(*p_action);

	if (!action)
	{
		return;
	}

	action->bindings = action->defaults;

	// The other actions give up what this one takes back
	for (Action& other : m_actions)
	{
		if (&other == action)
		{
			continue;
		}

		for (auto& binding : other.bindings)
		{
			if (binding && std::find(action->bindings.begin(), action->bindings.end(), binding) != action->bindings.end())
			{
				binding.reset();
			}
		}
	}
}

std::optional<OvCore::Input::InputId> OvCore::Input::InputActions::GetPressedInput() const
{
	return m_pressedInput;
}

std::string OvCore::Input::InputActions::SaveBindings() const
{
	Json::Array bindings;

	for (const Action& action : m_actions)
	{
		Json::Array inputs;

		for (const auto& binding : action.bindings)
		{
			inputs.emplace_back(binding ? GetInputName(*binding) : std::string{});
		}

		bindings.emplace_back(Json::Object{ { "name", Json::Value(action.name) }, { "inputs", Json::Value(std::move(inputs)) } });
	}

	return Json::Write(Json::Value(Json::Object{ { "bindings", Json::Value(std::move(bindings)) } }));
}

void OvCore::Input::InputActions::LoadBindings(std::string_view p_text, std::vector<std::string>& p_warnings)
{
	std::string error;
	const auto root = Json::Parse(p_text, error);

	if (!root)
	{
		p_warnings.push_back(error);
		return;
	}

	const Json::Value* bindings = root->Find("bindings");
	const Json::Array* list = bindings ? bindings->AsArray() : nullptr;

	if (!list)
	{
		p_warnings.push_back("no \"bindings\" array");
		return;
	}

	for (const Json::Value& entry : *list)
	{
		const Json::Value* name = entry.Find("name");
		const std::string* text = name ? name->AsString() : nullptr;
		Action* action = text ? Find(*text) : nullptr;

		if (!action)
		{
			p_warnings.push_back(text ? std::format("no action named \"{}\" any more, skipped", *text) : std::string("an entry without a name, skipped"));
			continue;
		}

		const Json::Value* inputs = entry.Find("inputs");
		const Json::Array* names = inputs ? inputs->AsArray() : nullptr;

		if (!names)
		{
			continue;
		}

		for (size_t slot = 0; slot < std::min(names->size(), kSlotCount); ++slot)
		{
			const std::string* inputName = (*names)[slot].AsString();

			if (inputName && inputName->empty())
			{
				action->bindings[slot].reset();
			}
			else if (const auto input = inputName ? ParseInput(*inputName) : std::nullopt)
			{
				action->bindings[slot] = input;
			}
			else
			{
				p_warnings.push_back(std::format("action \"{}\": no input named {}, its slot {} keeps its default", action->name, inputName ? "\"" + *inputName + "\"" : "by a value that is not a string", slot + 1));
				action->bindings[slot] = action->defaults[slot];
			}
		}
	}

	RemoveDuplicates();
}

void OvCore::Input::InputActions::RemoveDuplicates()
{
	std::vector<InputId> seen;

	for (Action& action : m_actions)
	{
		for (auto& binding : action.bindings)
		{
			if (!binding)
			{
				continue;
			}

			if (std::find(seen.begin(), seen.end(), *binding) != seen.end())
			{
				binding.reset();
			}
			else
			{
				seen.push_back(*binding);
			}
		}
	}
}
