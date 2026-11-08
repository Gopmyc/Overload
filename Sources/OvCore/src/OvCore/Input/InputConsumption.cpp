/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>

#include <OvCore/Input/InputConsumption.h>

namespace
{
	bool IsHeld(const OvCore::Input::InputConsumption::State& p_state)
	{
		return p_state.value > 0.0f;
	}
}

void OvCore::Input::InputConsumption::BeginFrame(const StateReader& p_read)
{
	m_releasedForGame.clear();
	m_scrollConsumed = false;

	std::erase_if(m_consumed, [&p_read](const Entry& p_entry) {
		const State state = p_read(p_entry.input, p_entry.pad);

		// A button stays hidden through the frame it is released on, and a press afresh is a new input
		return p_entry.input.IsButton() ? state.pressed || (!IsHeld(state) && !state.released) : !IsHeld(state);
	});
}

bool OvCore::Input::InputConsumption::Consume(const InputId& p_input, int p_pad, const State& p_state)
{
	const Entry entry = MakeEntry(p_input, p_pad);

	if (IsHidden(p_input, p_pad) || !(IsHeld(p_state) || (p_input.IsButton() && p_state.pressed)))
	{
		return false;
	}

	// Seen held on an earlier frame, it goes up for the game rather than staying down
	if (p_input.IsButton() && IsHeld(p_state) && !p_state.pressed)
	{
		m_releasedForGame.push_back(entry);
	}

	m_consumed.push_back(entry);
	return true;
}

void OvCore::Input::InputConsumption::ConsumeMouseScroll()
{
	m_scrollConsumed = true;
}

void OvCore::Input::InputConsumption::SetKeyboardCaptured(bool p_captured, const StateReader& p_read)
{
	if (p_captured == m_keyboardCaptured)
	{
		return;
	}

	for (const InputId& input : GetAllInputs())
	{
		if (input.device != InputId::EDevice::KEY || Contains(m_consumed, MakeEntry(input, 0)))
		{
			continue;
		}

		const State state = p_read(input, 0);

		if (p_captured && IsHeld(state) && !state.pressed)
		{
			m_releasedForGame.push_back(MakeEntry(input, 0));
		}
		else if (!p_captured && (IsHeld(state) || state.pressed))
		{
			// Pressed while captured, it was meant for what captured the keyboard
			m_consumed.push_back(MakeEntry(input, 0));
		}
	}

	m_keyboardCaptured = p_captured;
}

bool OvCore::Input::InputConsumption::IsKeyboardCaptured() const
{
	return m_keyboardCaptured;
}

bool OvCore::Input::InputConsumption::IsHidden(const InputId& p_input, int p_pad) const
{
	return (m_keyboardCaptured && p_input.device == InputId::EDevice::KEY) || Contains(m_consumed, MakeEntry(p_input, p_pad));
}

bool OvCore::Input::InputConsumption::IsReleasedForGame(const InputId& p_input, int p_pad) const
{
	return Contains(m_releasedForGame, MakeEntry(p_input, p_pad));
}

bool OvCore::Input::InputConsumption::IsMouseScrollConsumed() const
{
	return m_scrollConsumed;
}

void OvCore::Input::InputConsumption::Reset()
{
	m_consumed.clear();
	m_releasedForGame.clear();
	m_keyboardCaptured = false;
	m_scrollConsumed = false;
}

OvCore::Input::InputConsumption::Entry OvCore::Input::InputConsumption::MakeEntry(const InputId& p_input, int p_pad)
{
	// The keyboard and the mouse are no pad's
	const bool gamepad = p_input.device == InputId::EDevice::GAMEPAD_BUTTON || p_input.device == InputId::EDevice::GAMEPAD_AXIS;
	return { p_input, gamepad ? p_pad : 0 };
}

bool OvCore::Input::InputConsumption::Contains(const std::vector<Entry>& p_entries, const Entry& p_entry)
{
	return std::find(p_entries.begin(), p_entries.end(), p_entry) != p_entries.end();
}
