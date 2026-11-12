/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <optional>
#include <type_traits>
#include <unordered_set>

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Helpers/InputHelpers.h>
#include <OvCore/Input/InputConsumption.h>
#include <OvWindowing/Inputs/InputManager.h>
#include <OvWindowing/Window.h>

using OvWindowing::Inputs::EGamepadAxis;
using OvWindowing::Inputs::EGamepadButton;
using OvWindowing::Inputs::EKey;
using OvWindowing::Inputs::EKeyState;
using OvWindowing::Inputs::EMouseButton;
using OvWindowing::Inputs::EMouseButtonState;
using OvCore::Helpers::InputHelpers;
using OvCore::Input::InputConsumption;
using OvCore::Input::InputId;

namespace
{
	constexpr int kFirstKey = static_cast<int>(EKey::KEY_SPACE);
	constexpr int kLastKey = static_cast<int>(EKey::KEY_MENU);
	constexpr int kButtonCount = 8;
	constexpr int kGamepadCount = OvWindowing::Inputs::Gamepads::kMaxGamepads;
	constexpr int kGamepadButtonCount = OvWindowing::Inputs::GamepadState::kButtonCount;

	/**
	* A gamepad button of a pad, as one value for the gate's sets
	*/
	struct PadButton
	{
		int pad;
		EGamepadButton button;

		bool operator==(const PadButton&) const = default;
	};

	struct PadButtonHash
	{
		size_t operator()(const PadButton& p_value) const
		{
			return static_cast<size_t>(p_value.pad * 32 + static_cast<int>(p_value.button));
		}
	};

	OvCore::Helpers::InputHelpers::MousePositionProviderCallback __MOUSE_POSITION_PROVIDER;
	OvCore::Helpers::InputHelpers::ViewportSizeProviderCallback __VIEWPORT_SIZE_PROVIDER;

	// The gate stays open unless a host drives it, so a built game reads its input untouched
	bool __GAME_INPUT_OPEN = true;
	bool __MOUSE_OVER_GAME = true;
	bool __MOUSE_LOCK_REQUESTED = false;
	bool __CURSOR_SHAPE_SET = false;

	// Held when the gate opened, or pressed for the host: hidden from the game until released
	std::unordered_set<EKey> __IGNORED_KEYS;
	std::unordered_set<EMouseButton> __IGNORED_BUTTONS;
	std::unordered_set<PadButton, PadButtonHash> __IGNORED_PAD_BUTTONS;

	// Still held when the gate closed: released for the game on the next frame, so nothing stays down
	std::unordered_set<EKey> __RELEASED_KEYS;
	std::unordered_set<EMouseButton> __RELEASED_BUTTONS;
	std::unordered_set<PadButton, PadButtonHash> __RELEASED_PAD_BUTTONS;

	// Consumed by the game itself (a UI that took a click), the keyboard's capture, and the scroll taken for the frame
	InputConsumption __CONSUMPTION;

	// The position last given to the game, and the offset that keeps a lock taken again from jumping
	std::optional<OvMaths::FVector2> __LAST_MOUSE_POSITION;
	OvMaths::FVector2 __LOCKED_MOUSE_OFFSET;
	bool __REBASE_MOUSE = false;

	OvWindowing::Inputs::InputManager& Manager()
	{
		return OVSERVICE(OvWindowing::Inputs::InputManager);
	}

	bool IsHeld(EKey p_key)
	{
		return Manager().GetKeyState(p_key) == EKeyState::KEY_DOWN;
	}

	bool IsHeld(EMouseButton p_button)
	{
		return Manager().GetMouseButtonState(p_button) == EMouseButtonState::MOUSE_DOWN;
	}

	bool IsHeld(const PadButton& p_button)
	{
		return Manager().GetGamepads().IsButtonDown(p_button.pad, p_button.button);
	}

	/**
	* Calls the function for every gamepad button of every pad
	*/
	template<typename TFunction>
	void ForEachPadButton(TFunction p_function)
	{
		for (int pad = 0; pad < kGamepadCount; ++pad)
		{
			for (int button = 0; button < kGamepadButtonCount; ++button)
			{
				p_function(PadButton{ pad, static_cast<EGamepadButton>(button) });
			}
		}
	}

	InputId KeyInput(EKey p_key)
	{
		return { InputId::EDevice::KEY, static_cast<int>(p_key) };
	}

	InputId MouseInput(EMouseButton p_button)
	{
		return { InputId::EDevice::MOUSE_BUTTON, static_cast<int>(p_button) };
	}

	InputId PadButtonInput(EGamepadButton p_button)
	{
		return { InputId::EDevice::GAMEPAD_BUTTON, static_cast<int>(p_button) };
	}

	/**
	* The way of the axis a value goes: a trigger has one, a stick's axis two
	*/
	InputId PadAxisInput(EGamepadAxis p_axis, float p_value)
	{
		const bool trigger = p_axis == EGamepadAxis::GAMEPAD_AXIS_LEFT_TRIGGER || p_axis == EGamepadAxis::GAMEPAD_AXIS_RIGHT_TRIGGER;
		return { InputId::EDevice::GAMEPAD_AXIS, static_cast<int>(p_axis), static_cast<int8_t>(trigger ? 0 : (p_value < 0.0f ? -1 : 1)) };
	}

	/**
	* An input as the game would see it without consumption: what the consumption is told
	*/
	InputConsumption::State ReadUnconsumed(const InputId& p_input, int p_pad)
	{
		switch (p_input.device)
		{
		case InputId::EDevice::KEY:
		{
			const auto key = static_cast<EKey>(p_input.code);
			return { InputHelpers::GetKey(key, true) ? 1.0f : 0.0f, InputHelpers::GetKeyDown(key, true), InputHelpers::GetKeyUp(key, true) };
		}

		case InputId::EDevice::MOUSE_BUTTON:
		{
			const auto button = static_cast<EMouseButton>(p_input.code);
			return { InputHelpers::GetMouseButton(button, true) ? 1.0f : 0.0f, InputHelpers::GetMouseButtonDown(button, true), InputHelpers::GetMouseButtonUp(button, true) };
		}

		case InputId::EDevice::GAMEPAD_BUTTON:
		{
			const auto button = static_cast<EGamepadButton>(p_input.code);
			return { InputHelpers::GetGamepadButton(p_pad, button, true) ? 1.0f : 0.0f, InputHelpers::GetGamepadButtonDown(p_pad, button, true), InputHelpers::GetGamepadButtonUp(p_pad, button, true) };
		}

		case InputId::EDevice::GAMEPAD_AXIS:
		{
			const float value = InputHelpers::GetGamepadAxis(p_pad, static_cast<EGamepadAxis>(p_input.code), true);
			return { p_input.direction == 0 ? value : std::max(value * static_cast<float>(p_input.direction), 0.0f) };
		}
		}

		return {};
	}

	/**
	* Whether a read leaves the input out, and whether the consumption released it for the game on this frame
	*/
	bool Hidden(const InputId& p_input, int p_pad, bool p_includeConsumed)
	{
		return !p_includeConsumed && __CONSUMPTION.IsHidden(p_input, p_pad);
	}

	bool ReleasedByConsumption(const InputId& p_input, int p_pad, bool p_includeConsumed)
	{
		return !p_includeConsumed && __CONSUMPTION.IsReleasedForGame(p_input, p_pad);
	}

	OvMaths::FVector2 GetRawMousePosition()
	{
		if (__MOUSE_POSITION_PROVIDER)
		{
			return __MOUSE_POSITION_PROVIDER();
		}

		const auto [mouseX, mouseY] = Manager().GetMousePosition();
		return { static_cast<float>(mouseX), static_cast<float>(mouseY) };
	}

	void ApplyMouseLock(bool p_locked)
	{
		OVSERVICE(OvWindowing::Window).SetCursorMode(p_locked ? OvWindowing::Cursor::ECursorMode::DISABLED : OvWindowing::Cursor::ECursorMode::NORMAL);
	}

	/**
	* What was ignored leaves the set once it is up again (released on an earlier frame), or pressed afresh
	*/
	template<typename TSet>
	void PruneIgnored(TSet& p_ignored)
	{
		using T = typename TSet::value_type;

		auto& input = Manager();

		std::erase_if(p_ignored, [&input](const T& p_input) {
			if constexpr (std::is_same_v<T, EKey>)
				return input.IsKeyPressed(p_input) || (!IsHeld(p_input) && !input.IsKeyReleased(p_input));
			else if constexpr (std::is_same_v<T, EMouseButton>)
				return input.IsMouseButtonPressed(p_input) || (!IsHeld(p_input) && !input.IsMouseButtonReleased(p_input));
			else
			{
				const auto& gamepads = input.GetGamepads();
				return gamepads.IsButtonPressed(p_input.pad, p_input.button) || (!IsHeld(p_input) && !gamepads.IsButtonReleased(p_input.pad, p_input.button));
			}
		});
	}

	void CloseGate()
	{
		for (int key = kFirstKey; key <= kLastKey; ++key)
		{
			if (const auto value = static_cast<EKey>(key); IsHeld(value) && !__IGNORED_KEYS.contains(value))
				__RELEASED_KEYS.insert(value);
		}

		for (int button = 0; button < kButtonCount; ++button)
		{
			if (const auto value = static_cast<EMouseButton>(button); IsHeld(value) && !__IGNORED_BUTTONS.contains(value))
				__RELEASED_BUTTONS.insert(value);
		}

		ForEachPadButton([](const PadButton& p_button) {
			if (IsHeld(p_button) && !__IGNORED_PAD_BUTTONS.contains(p_button))
				__RELEASED_PAD_BUTTONS.insert(p_button);
		});

		__IGNORED_KEYS.clear();
		__IGNORED_BUTTONS.clear();
		__IGNORED_PAD_BUTTONS.clear();

		if (__MOUSE_LOCK_REQUESTED)
		{
			ApplyMouseLock(false);
		}

		// The window keeps a shape until another is set: the game's goes with its input
		if (__MOUSE_LOCK_REQUESTED || __CURSOR_SHAPE_SET)
		{
			OVSERVICE(OvWindowing::Window).SetCursorShape(OvWindowing::Cursor::ECursorShape::ARROW);
			__CURSOR_SHAPE_SET = false;
		}
	}

	void OpenGate()
	{
		auto& input = Manager();

		for (int key = kFirstKey; key <= kLastKey; ++key)
		{
			if (const auto value = static_cast<EKey>(key); IsHeld(value) && !input.IsKeyPressed(value))
				__IGNORED_KEYS.insert(value);
		}

		for (int button = 0; button < kButtonCount; ++button)
		{
			if (const auto value = static_cast<EMouseButton>(button); IsHeld(value) && !input.IsMouseButtonPressed(value))
				__IGNORED_BUTTONS.insert(value);
		}

		ForEachPadButton([&input](const PadButton& p_button) {
			if (IsHeld(p_button) && !input.GetGamepads().IsButtonPressed(p_button.pad, p_button.button))
				__IGNORED_PAD_BUTTONS.insert(p_button);
		});

		if (__MOUSE_LOCK_REQUESTED)
		{
			ApplyMouseLock(true);
			__REBASE_MOUSE = true;
		}
	}
}

void OvCore::Helpers::InputHelpers::SetMousePositionProvider(MousePositionProviderCallback p_provider)
{
	__MOUSE_POSITION_PROVIDER = std::move(p_provider);
}

OvMaths::FVector2 OvCore::Helpers::InputHelpers::GetMousePosition()
{
	const OvMaths::FVector2 raw = GetRawMousePosition();

	// Closed, the mouse stands still for the game, so that nothing it drives turns
	if (!__GAME_INPUT_OPEN)
	{
		if (!__LAST_MOUSE_POSITION)
		{
			__LAST_MOUSE_POSITION = raw;
		}

		return __LAST_MOUSE_POSITION.value();
	}

	if (__REBASE_MOUSE)
	{
		__LOCKED_MOUSE_OFFSET = __LAST_MOUSE_POSITION ? __LAST_MOUSE_POSITION.value() - raw : OvMaths::FVector2{};
		__REBASE_MOUSE = false;
	}

	const OvMaths::FVector2 position = __MOUSE_LOCK_REQUESTED ? raw + __LOCKED_MOUSE_OFFSET : raw;
	__LAST_MOUSE_POSITION = position;
	return position;
}

void OvCore::Helpers::InputHelpers::SetViewportSizeProvider(ViewportSizeProviderCallback p_provider)
{
	__VIEWPORT_SIZE_PROVIDER = std::move(p_provider);
}

OvMaths::FVector2 OvCore::Helpers::InputHelpers::GetViewportSize()
{
	if (__VIEWPORT_SIZE_PROVIDER)
	{
		return __VIEWPORT_SIZE_PROVIDER();
	}

	const auto [width, height] = OVSERVICE(OvWindowing::Window).GetSize();
	return { static_cast<float>(width), static_cast<float>(height) };
}

bool OvCore::Helpers::InputHelpers::GetKey(EKey p_key, bool p_includeConsumed)
{
	return __GAME_INPUT_OPEN && !__IGNORED_KEYS.contains(p_key) && !Hidden(KeyInput(p_key), 0, p_includeConsumed) && IsHeld(p_key);
}

bool OvCore::Helpers::InputHelpers::GetKeyDown(EKey p_key, bool p_includeConsumed)
{
	return __GAME_INPUT_OPEN && !__IGNORED_KEYS.contains(p_key) && !Hidden(KeyInput(p_key), 0, p_includeConsumed) && Manager().IsKeyPressed(p_key);
}

bool OvCore::Helpers::InputHelpers::GetKeyUp(EKey p_key, bool p_includeConsumed)
{
	const bool hidden = Hidden(KeyInput(p_key), 0, p_includeConsumed);
	return (__RELEASED_KEYS.contains(p_key) && !hidden) || ReleasedByConsumption(KeyInput(p_key), 0, p_includeConsumed) || (__GAME_INPUT_OPEN && !__IGNORED_KEYS.contains(p_key) && !hidden && Manager().IsKeyReleased(p_key));
}

int OvCore::Helpers::InputHelpers::GetKeyRepeatCount(EKey p_key, bool p_includeConsumed)
{
	const bool visible = __GAME_INPUT_OPEN && !__IGNORED_KEYS.contains(p_key) && !Hidden(KeyInput(p_key), 0, p_includeConsumed);
	return visible ? Manager().GetTypedInput().GetRepeatCount(p_key) : 0;
}

std::string OvCore::Helpers::InputHelpers::GetTypedText(bool p_includeConsumed)
{
	if (!__GAME_INPUT_OPEN || (!p_includeConsumed && __CONSUMPTION.IsKeyboardCaptured()))
	{
		return {};
	}

	// A printable key held when the gate opened repeats its character for the system: the text waits until it is up
	for (const EKey key : __IGNORED_KEYS)
	{
		if (key == EKey::KEY_SPACE || !Manager().GetKeyName(key).empty())
		{
			return {};
		}
	}

	return Manager().GetTypedInput().GetText();
}

bool OvCore::Helpers::InputHelpers::GetMouseButton(EMouseButton p_button, bool p_includeConsumed)
{
	return __GAME_INPUT_OPEN && !__IGNORED_BUTTONS.contains(p_button) && !Hidden(MouseInput(p_button), 0, p_includeConsumed) && IsHeld(p_button);
}

bool OvCore::Helpers::InputHelpers::GetMouseButtonDown(EMouseButton p_button, bool p_includeConsumed)
{
	return __GAME_INPUT_OPEN && !__IGNORED_BUTTONS.contains(p_button) && !Hidden(MouseInput(p_button), 0, p_includeConsumed) && Manager().IsMouseButtonPressed(p_button);
}

bool OvCore::Helpers::InputHelpers::GetMouseButtonUp(EMouseButton p_button, bool p_includeConsumed)
{
	const bool hidden = Hidden(MouseInput(p_button), 0, p_includeConsumed);
	return (__RELEASED_BUTTONS.contains(p_button) && !hidden) || ReleasedByConsumption(MouseInput(p_button), 0, p_includeConsumed) || (__GAME_INPUT_OPEN && !__IGNORED_BUTTONS.contains(p_button) && !hidden && Manager().IsMouseButtonReleased(p_button));
}

OvMaths::FVector2 OvCore::Helpers::InputHelpers::GetMouseScroll(bool p_includeConsumed)
{
	if (!__GAME_INPUT_OPEN || !__MOUSE_OVER_GAME || (!p_includeConsumed && __CONSUMPTION.IsMouseScrollConsumed()))
	{
		return {};
	}

	const auto [scrollX, scrollY] = Manager().GetMouseScroll();
	return { static_cast<float>(scrollX), static_cast<float>(scrollY) };
}

std::optional<int> OvCore::Helpers::InputHelpers::GetFirstGamepad()
{
	return Manager().GetGamepads().GetFirstConnected();
}

bool OvCore::Helpers::InputHelpers::IsGamepadConnected(int p_pad)
{
	return Manager().GetGamepads().IsConnected(p_pad);
}

std::string OvCore::Helpers::InputHelpers::GetGamepadName(int p_pad)
{
	return Manager().GetGamepads().GetName(p_pad);
}

bool OvCore::Helpers::InputHelpers::GetGamepadButton(int p_pad, EGamepadButton p_button, bool p_includeConsumed)
{
	const PadButton button{ p_pad, p_button };
	return __GAME_INPUT_OPEN && !__IGNORED_PAD_BUTTONS.contains(button) && !Hidden(PadButtonInput(p_button), p_pad, p_includeConsumed) && IsHeld(button);
}

bool OvCore::Helpers::InputHelpers::GetGamepadButtonDown(int p_pad, EGamepadButton p_button, bool p_includeConsumed)
{
	const PadButton button{ p_pad, p_button };
	return __GAME_INPUT_OPEN && !__IGNORED_PAD_BUTTONS.contains(button) && !Hidden(PadButtonInput(p_button), p_pad, p_includeConsumed) && Manager().GetGamepads().IsButtonPressed(p_pad, p_button);
}

bool OvCore::Helpers::InputHelpers::GetGamepadButtonUp(int p_pad, EGamepadButton p_button, bool p_includeConsumed)
{
	const PadButton button{ p_pad, p_button };
	const bool hidden = Hidden(PadButtonInput(p_button), p_pad, p_includeConsumed);
	return (__RELEASED_PAD_BUTTONS.contains(button) && !hidden) || ReleasedByConsumption(PadButtonInput(p_button), p_pad, p_includeConsumed) || (__GAME_INPUT_OPEN && !__IGNORED_PAD_BUTTONS.contains(button) && !hidden && Manager().GetGamepads().IsButtonReleased(p_pad, p_button));
}

float OvCore::Helpers::InputHelpers::GetGamepadAxis(int p_pad, EGamepadAxis p_axis, bool p_includeConsumed)
{
	const float value = __GAME_INPUT_OPEN ? Manager().GetGamepads().GetAxis(p_pad, p_axis) : 0.0f;

	// A consumed way of a stick reads as 0, while the other way still reads
	return value != 0.0f && Hidden(PadAxisInput(p_axis, value), p_pad, p_includeConsumed) ? 0.0f : value;
}

void OvCore::Helpers::InputHelpers::SetGamepadDeadZone(float p_deadZone)
{
	Manager().GetGamepads().SetDeadZone(p_deadZone);
}

float OvCore::Helpers::InputHelpers::GetGamepadDeadZone()
{
	return Manager().GetGamepads().GetDeadZone();
}

bool OvCore::Helpers::InputHelpers::Consume(const InputId& p_input, int p_pad)
{
	__CONSUMPTION.Consume(p_input, p_pad, ReadUnconsumed(p_input, p_pad));
	return __CONSUMPTION.IsHidden(p_input, p_pad);
}

void OvCore::Helpers::InputHelpers::ConsumeMouseScroll()
{
	__CONSUMPTION.ConsumeMouseScroll();
}

void OvCore::Helpers::InputHelpers::SetKeyboardCaptured(bool p_captured)
{
	__CONSUMPTION.SetKeyboardCaptured(p_captured, ReadUnconsumed);
}

bool OvCore::Helpers::InputHelpers::IsKeyboardCaptured()
{
	return __CONSUMPTION.IsKeyboardCaptured();
}

void OvCore::Helpers::InputHelpers::SetMouseLocked(bool p_locked)
{
	// Asked again (some games lock every frame), the offset that hides a reopening stays
	if (p_locked != __MOUSE_LOCK_REQUESTED)
	{
		__LOCKED_MOUSE_OFFSET = {};
		__REBASE_MOUSE = false;
	}

	__MOUSE_LOCK_REQUESTED = p_locked;

	if (__GAME_INPUT_OPEN)
	{
		ApplyMouseLock(p_locked);
	}
}

void OvCore::Helpers::InputHelpers::SetCursorShape(OvWindowing::Cursor::ECursorShape p_shape)
{
	if (__GAME_INPUT_OPEN)
	{
		OVSERVICE(OvWindowing::Window).SetCursorShape(p_shape);
		__CURSOR_SHAPE_SET = p_shape != OvWindowing::Cursor::ECursorShape::ARROW;
	}
}

void OvCore::Helpers::InputHelpers::BeginFrame()
{
	__RELEASED_KEYS.clear();
	__RELEASED_BUTTONS.clear();
	__RELEASED_PAD_BUTTONS.clear();

	PruneIgnored(__IGNORED_KEYS);
	PruneIgnored(__IGNORED_BUTTONS);
	PruneIgnored(__IGNORED_PAD_BUTTONS);

	__CONSUMPTION.BeginFrame(ReadUnconsumed);
}

void OvCore::Helpers::InputHelpers::UpdateGameInputGate(bool p_open, bool p_mouseOverGame)
{
	BeginFrame();

	if (p_open != __GAME_INPUT_OPEN)
	{
		if (p_open)
			OpenGate();
		else
			CloseGate();

		__GAME_INPUT_OPEN = p_open;
	}
	else if (p_open && __MOUSE_LOCK_REQUESTED && OVSERVICE(OvWindowing::Window).GetCursorMode() != OvWindowing::Cursor::ECursorMode::DISABLED)
	{
		// Open, the game owns the lock it asked for: whatever let it go takes it back
		ApplyMouseLock(true);
		__REBASE_MOUSE = true;
	}

	__MOUSE_OVER_GAME = p_mouseOverGame;

	// A click away from the game's image is the host's (a toolbar button, a click that moves the focus)
	if (__GAME_INPUT_OPEN && !__MOUSE_OVER_GAME)
	{
		for (int button = 0; button < kButtonCount; ++button)
		{
			if (const auto value = static_cast<EMouseButton>(button); Manager().IsMouseButtonPressed(value))
				__IGNORED_BUTTONS.insert(value);
		}
	}
}

void OvCore::Helpers::InputHelpers::ResetGameInputGate()
{
	__GAME_INPUT_OPEN = false;
	__MOUSE_OVER_GAME = true;
	__MOUSE_LOCK_REQUESTED = false;
	__CURSOR_SHAPE_SET = false;
	__IGNORED_KEYS.clear();
	__IGNORED_BUTTONS.clear();
	__IGNORED_PAD_BUTTONS.clear();
	__RELEASED_KEYS.clear();
	__RELEASED_BUTTONS.clear();
	__RELEASED_PAD_BUTTONS.clear();
	__CONSUMPTION.Reset();
	__LAST_MOUSE_POSITION.reset();

	// A play session starts with the default dead zone, whatever the last one set
	Manager().GetGamepads().SetDeadZone(OvWindowing::Inputs::Gamepads::kDefaultDeadZone);
	__LOCKED_MOUSE_OFFSET = {};
	__REBASE_MOUSE = false;
}
