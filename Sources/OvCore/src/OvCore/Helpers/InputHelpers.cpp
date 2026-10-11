/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <optional>
#include <type_traits>
#include <unordered_set>

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Helpers/InputHelpers.h>
#include <OvWindowing/Inputs/InputManager.h>
#include <OvWindowing/Window.h>

using OvWindowing::Inputs::EKey;
using OvWindowing::Inputs::EKeyState;
using OvWindowing::Inputs::EMouseButton;
using OvWindowing::Inputs::EMouseButtonState;

namespace
{
	constexpr int kFirstKey = static_cast<int>(EKey::KEY_SPACE);
	constexpr int kLastKey = static_cast<int>(EKey::KEY_MENU);
	constexpr int kButtonCount = 8;

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

	// Still held when the gate closed: released for the game on the next frame, so nothing stays down
	std::unordered_set<EKey> __RELEASED_KEYS;
	std::unordered_set<EMouseButton> __RELEASED_BUTTONS;

	// The position last given to the game, and the offset that keeps a lock taken again from jumping
	std::optional<OvMaths::FVector2> __LAST_MOUSE_POSITION;
	OvMaths::FVector2 __LOCKED_MOUSE_OFFSET;
	bool __REBASE_MOUSE = false;

	OvWindowing::Inputs::InputManager& Input()
	{
		return OVSERVICE(OvWindowing::Inputs::InputManager);
	}

	bool IsHeld(EKey p_key)
	{
		return Input().GetKeyState(p_key) == EKeyState::KEY_DOWN;
	}

	bool IsHeld(EMouseButton p_button)
	{
		return Input().GetMouseButtonState(p_button) == EMouseButtonState::MOUSE_DOWN;
	}

	OvMaths::FVector2 GetRawMousePosition()
	{
		if (__MOUSE_POSITION_PROVIDER)
		{
			return __MOUSE_POSITION_PROVIDER();
		}

		const auto [mouseX, mouseY] = Input().GetMousePosition();
		return { static_cast<float>(mouseX), static_cast<float>(mouseY) };
	}

	void ApplyMouseLock(bool p_locked)
	{
		OVSERVICE(OvWindowing::Window).SetCursorMode(p_locked ? OvWindowing::Cursor::ECursorMode::DISABLED : OvWindowing::Cursor::ECursorMode::NORMAL);
	}

	/**
	* What was ignored leaves the set once it is up again (released on an earlier frame), or pressed afresh
	*/
	template<typename T>
	void PruneIgnored(std::unordered_set<T>& p_ignored)
	{
		auto& input = Input();

		std::erase_if(p_ignored, [&input](T p_input) {
			if constexpr (std::is_same_v<T, EKey>)
				return input.IsKeyPressed(p_input) || (!IsHeld(p_input) && !input.IsKeyReleased(p_input));
			else
				return input.IsMouseButtonPressed(p_input) || (!IsHeld(p_input) && !input.IsMouseButtonReleased(p_input));
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

		__IGNORED_KEYS.clear();
		__IGNORED_BUTTONS.clear();

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
		auto& input = Input();

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

bool OvCore::Helpers::InputHelpers::GetKey(EKey p_key)
{
	return __GAME_INPUT_OPEN && !__IGNORED_KEYS.contains(p_key) && IsHeld(p_key);
}

bool OvCore::Helpers::InputHelpers::GetKeyDown(EKey p_key)
{
	return __GAME_INPUT_OPEN && !__IGNORED_KEYS.contains(p_key) && Input().IsKeyPressed(p_key);
}

bool OvCore::Helpers::InputHelpers::GetKeyUp(EKey p_key)
{
	return __RELEASED_KEYS.contains(p_key) || (__GAME_INPUT_OPEN && !__IGNORED_KEYS.contains(p_key) && Input().IsKeyReleased(p_key));
}

bool OvCore::Helpers::InputHelpers::GetMouseButton(EMouseButton p_button)
{
	return __GAME_INPUT_OPEN && !__IGNORED_BUTTONS.contains(p_button) && IsHeld(p_button);
}

bool OvCore::Helpers::InputHelpers::GetMouseButtonDown(EMouseButton p_button)
{
	return __GAME_INPUT_OPEN && !__IGNORED_BUTTONS.contains(p_button) && Input().IsMouseButtonPressed(p_button);
}

bool OvCore::Helpers::InputHelpers::GetMouseButtonUp(EMouseButton p_button)
{
	return __RELEASED_BUTTONS.contains(p_button) || (__GAME_INPUT_OPEN && !__IGNORED_BUTTONS.contains(p_button) && Input().IsMouseButtonReleased(p_button));
}

OvMaths::FVector2 OvCore::Helpers::InputHelpers::GetMouseScroll()
{
	if (!__GAME_INPUT_OPEN || !__MOUSE_OVER_GAME)
	{
		return {};
	}

	const auto [scrollX, scrollY] = Input().GetMouseScroll();
	return { static_cast<float>(scrollX), static_cast<float>(scrollY) };
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

void OvCore::Helpers::InputHelpers::UpdateGameInputGate(bool p_open, bool p_mouseOverGame)
{
	__RELEASED_KEYS.clear();
	__RELEASED_BUTTONS.clear();

	PruneIgnored(__IGNORED_KEYS);
	PruneIgnored(__IGNORED_BUTTONS);

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
			if (const auto value = static_cast<EMouseButton>(button); Input().IsMouseButtonPressed(value))
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
	__RELEASED_KEYS.clear();
	__RELEASED_BUTTONS.clear();
	__LAST_MOUSE_POSITION.reset();
	__LOCKED_MOUSE_OFFSET = {};
	__REBASE_MOUSE = false;
}
