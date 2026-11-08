/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>

#include "OvWindowing/Inputs/Gamepads.h"

namespace
{
	int ButtonIndex(OvWindowing::Inputs::EGamepadButton p_button)
	{
		return static_cast<int>(p_button);
	}

	bool IsValidButton(int p_button)
	{
		return p_button >= 0 && p_button < OvWindowing::Inputs::GamepadState::kButtonCount;
	}

	float Finite(float p_value)
	{
		return std::isfinite(p_value) ? std::clamp(p_value, -1.0f, 1.0f) : 0.0f;
	}
}

void OvWindowing::Inputs::Gamepads::SetState(int p_pad, const std::optional<GamepadState>& p_state)
{
	if (p_pad < 0 || p_pad >= kMaxGamepads)
	{
		return;
	}

	Pad& pad = m_pads[p_pad];

	for (int button = 0; button < GamepadState::kButtonCount; ++button)
	{
		const bool held = p_state && p_state->buttons[button];
		pad.pressed[button] = held && !pad.held[button];
		pad.released[button] = !held && pad.held[button];
		pad.held[button] = held;
	}

	pad.connected = p_state.has_value();
	pad.name = p_state ? p_state->name : std::string{};

	for (int axis = 0; axis < GamepadState::kAxisCount; ++axis)
	{
		pad.axes[axis] = p_state ? Finite(p_state->axes[axis]) : 0.0f;
	}

	// A trigger at rest reads -1 from the window system, and 0 when the pad is gone
	if (!p_state)
	{
		pad.axes[static_cast<int>(EGamepadAxis::GAMEPAD_AXIS_LEFT_TRIGGER)] = -1.0f;
		pad.axes[static_cast<int>(EGamepadAxis::GAMEPAD_AXIS_RIGHT_TRIGGER)] = -1.0f;
	}
}

const OvWindowing::Inputs::Gamepads::Pad* OvWindowing::Inputs::Gamepads::FindPad(int p_pad) const
{
	return p_pad >= 0 && p_pad < kMaxGamepads ? &m_pads[p_pad] : nullptr;
}

bool OvWindowing::Inputs::Gamepads::IsConnected(int p_pad) const
{
	const Pad* pad = FindPad(p_pad);
	return pad && pad->connected;
}

std::optional<int> OvWindowing::Inputs::Gamepads::GetFirstConnected() const
{
	for (int pad = 0; pad < kMaxGamepads; ++pad)
	{
		if (m_pads[pad].connected)
		{
			return pad;
		}
	}

	return std::nullopt;
}

const std::string& OvWindowing::Inputs::Gamepads::GetName(int p_pad) const
{
	static const std::string kNoName;
	const Pad* pad = FindPad(p_pad);
	return pad ? pad->name : kNoName;
}

bool OvWindowing::Inputs::Gamepads::IsButtonDown(int p_pad, EGamepadButton p_button) const
{
	const Pad* pad = FindPad(p_pad);
	return pad && IsValidButton(ButtonIndex(p_button)) && pad->held[ButtonIndex(p_button)];
}

bool OvWindowing::Inputs::Gamepads::IsButtonPressed(int p_pad, EGamepadButton p_button) const
{
	const Pad* pad = FindPad(p_pad);
	return pad && IsValidButton(ButtonIndex(p_button)) && pad->pressed[ButtonIndex(p_button)];
}

bool OvWindowing::Inputs::Gamepads::IsButtonReleased(int p_pad, EGamepadButton p_button) const
{
	const Pad* pad = FindPad(p_pad);
	return pad && IsValidButton(ButtonIndex(p_button)) && pad->released[ButtonIndex(p_button)];
}

float OvWindowing::Inputs::Gamepads::GetAxis(int p_pad, EGamepadAxis p_axis) const
{
	const Pad* pad = FindPad(p_pad);
	if (!pad || !pad->connected)
	{
		return 0.0f;
	}

	switch (p_axis)
	{
	case EGamepadAxis::GAMEPAD_AXIS_LEFT_TRIGGER:
	case EGamepadAxis::GAMEPAD_AXIS_RIGHT_TRIGGER:
	{
		// From -1 at rest to 1 pressed fully, as 0 to 1 past a small dead zone
		const float travel = (pad->axes[static_cast<int>(p_axis)] + 1.0f) * 0.5f;
		return travel <= kTriggerDeadZone ? 0.0f : std::min((travel - kTriggerDeadZone) / (1.0f - kTriggerDeadZone), 1.0f);
	}

	case EGamepadAxis::GAMEPAD_AXIS_LEFT_X:
	case EGamepadAxis::GAMEPAD_AXIS_LEFT_Y:
	case EGamepadAxis::GAMEPAD_AXIS_RIGHT_X:
	case EGamepadAxis::GAMEPAD_AXIS_RIGHT_Y:
	{
		// The dead zone is a disc around the stick's centre, so that a push along a diagonal is not cut short
		const bool left = p_axis == EGamepadAxis::GAMEPAD_AXIS_LEFT_X || p_axis == EGamepadAxis::GAMEPAD_AXIS_LEFT_Y;
		const float x = pad->axes[static_cast<int>(left ? EGamepadAxis::GAMEPAD_AXIS_LEFT_X : EGamepadAxis::GAMEPAD_AXIS_RIGHT_X)];
		const float y = -pad->axes[static_cast<int>(left ? EGamepadAxis::GAMEPAD_AXIS_LEFT_Y : EGamepadAxis::GAMEPAD_AXIS_RIGHT_Y)];
		const float length = std::sqrt(x * x + y * y);

		if (length <= m_deadZone)
		{
			return 0.0f;
		}

		const float scale = (std::min(length, 1.0f) - m_deadZone) / (1.0f - m_deadZone) / length;
		const bool isX = p_axis == EGamepadAxis::GAMEPAD_AXIS_LEFT_X || p_axis == EGamepadAxis::GAMEPAD_AXIS_RIGHT_X;
		return std::clamp((isX ? x : y) * scale, -1.0f, 1.0f);
	}
	}

	return 0.0f;
}

void OvWindowing::Inputs::Gamepads::SetDeadZone(float p_deadZone)
{
	if (std::isfinite(p_deadZone))
	{
		m_deadZone = std::clamp(p_deadZone, 0.0f, 0.95f);
	}
}

float OvWindowing::Inputs::Gamepads::GetDeadZone() const
{
	return m_deadZone;
}
