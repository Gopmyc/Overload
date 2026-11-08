/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Helpers/InputHelpers.h>
#include <OvCore/Input/GameInput.h>
#include <OvDebug/Logger.h>
#include <OvTools/Filesystem/IniFile.h>
#include <OvTools/Filesystem/SaveStorage.h>
#include <OvWindowing/Inputs/InputManager.h>

using OvCore::Helpers::InputHelpers;
using OvWindowing::Inputs::EGamepadAxis;
using OvWindowing::Inputs::EGamepadButton;
using OvWindowing::Inputs::EKey;
using OvWindowing::Inputs::EMouseButton;

namespace
{
	/**
	* "LEFT_SHIFT" as "Left Shift"
	*/
	std::string InWords(std::string_view p_name)
	{
		std::string words;
		bool startOfWord = true;

		for (const char character : p_name)
		{
			if (character == '_')
			{
				words += ' ';
				startOfWord = true;
			}
			else
			{
				words += startOfWord ? character : static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
				startOfWord = false;
			}
		}

		return words;
	}
}

float OvCore::Input::GameInputSource::GetValue(const InputId& p_input) const
{
	switch (p_input.device)
	{
	case InputId::EDevice::KEY:
		return InputHelpers::GetKey(static_cast<EKey>(p_input.code)) ? 1.0f : 0.0f;

	case InputId::EDevice::MOUSE_BUTTON:
		return InputHelpers::GetMouseButton(static_cast<EMouseButton>(p_input.code)) ? 1.0f : 0.0f;

	case InputId::EDevice::GAMEPAD_BUTTON:
	{
		const auto pad = InputHelpers::GetFirstGamepad();
		return pad && InputHelpers::GetGamepadButton(*pad, static_cast<EGamepadButton>(p_input.code)) ? 1.0f : 0.0f;
	}

	case InputId::EDevice::GAMEPAD_AXIS:
	{
		const auto pad = InputHelpers::GetFirstGamepad();
		if (!pad)
		{
			return 0.0f;
		}

		const float value = InputHelpers::GetGamepadAxis(*pad, static_cast<EGamepadAxis>(p_input.code));
		return p_input.direction == 0 ? value : std::max(value * static_cast<float>(p_input.direction), 0.0f);
	}
	}

	return 0.0f;
}

bool OvCore::Input::GameInputSource::WasPressed(const InputId& p_input) const
{
	switch (p_input.device)
	{
	case InputId::EDevice::KEY:
		return InputHelpers::GetKeyDown(static_cast<EKey>(p_input.code));

	case InputId::EDevice::MOUSE_BUTTON:
		return InputHelpers::GetMouseButtonDown(static_cast<EMouseButton>(p_input.code));

	case InputId::EDevice::GAMEPAD_BUTTON:
	{
		const auto pad = InputHelpers::GetFirstGamepad();
		return pad && InputHelpers::GetGamepadButtonDown(*pad, static_cast<EGamepadButton>(p_input.code));
	}

	default:
		return false;
	}
}

bool OvCore::Input::GameInputSource::WasReleased(const InputId& p_input) const
{
	switch (p_input.device)
	{
	case InputId::EDevice::KEY:
		return InputHelpers::GetKeyUp(static_cast<EKey>(p_input.code));

	case InputId::EDevice::MOUSE_BUTTON:
		return InputHelpers::GetMouseButtonUp(static_cast<EMouseButton>(p_input.code));

	case InputId::EDevice::GAMEPAD_BUTTON:
	{
		const auto pad = InputHelpers::GetFirstGamepad();
		return pad && InputHelpers::GetGamepadButtonUp(*pad, static_cast<EGamepadButton>(p_input.code));
	}

	default:
		return false;
	}
}

void OvCore::Input::LoadProjectActions(const OvTools::Filesystem::IniFile& p_projectSettings, const std::filesystem::path& p_projectAssetsPath, OvTools::Filesystem::SaveStorage& p_saveStorage, InputActions& p_actions)
{
	std::vector<ActionDefinition> definitions;
	std::string file;

	if (p_projectSettings.TryGet("input_actions", file) && !file.empty())
	{
		std::ifstream stream(p_projectAssetsPath / file, std::ios::binary);

		if (!stream)
		{
			OVLOG_WARNING("input_actions: cannot read \"" + file + "\", the project has no input action");
		}
		else
		{
			std::stringstream text;
			text << stream.rdbuf();

			std::vector<std::string> warnings;
			definitions = InputActions::ParseDefinitions(text.str(), warnings);

			for (const std::string& warning : warnings)
			{
				OVLOG_WARNING("input_actions (" + file + "): " + warning);
			}
		}
	}

	p_actions.SetDefinitions(definitions);

	if (definitions.empty())
	{
		return;
	}

	std::string error;

	if (const auto saved = p_saveStorage.Read(kBindingsFile, error))
	{
		std::vector<std::string> warnings;
		p_actions.LoadBindings(*saved, warnings);

		for (const std::string& warning : warnings)
		{
			OVLOG_WARNING(std::string(kBindingsFile) + ": " + warning);
		}
	}
	else if (error != "not found")
	{
		OVLOG_WARNING(std::string(kBindingsFile) + ": " + error);
	}
}

void OvCore::Input::SaveBindings(OvTools::Filesystem::SaveStorage& p_saveStorage, const InputActions& p_actions)
{
	std::string error;

	if (!p_saveStorage.Write(kBindingsFile, p_actions.SaveBindings(), error))
	{
		OVLOG_WARNING(std::string(kBindingsFile) + ": cannot be written, " + error);
	}
}

std::string OvCore::Input::GetInputLabel(const InputId& p_input)
{
	const std::string name = GetInputName(p_input);

	switch (p_input.device)
	{
	case InputId::EDevice::KEY:
	{
		std::string printed = OVSERVICE(OvWindowing::Inputs::InputManager).GetKeyName(static_cast<EKey>(p_input.code));

		if (!printed.empty())
		{
			std::transform(printed.begin(), printed.end(), printed.begin(), [](unsigned char p_character) { return static_cast<char>(std::toupper(p_character)); });
			return printed;
		}

		return InWords(name);
	}

	case InputId::EDevice::MOUSE_BUTTON:
		// "MOUSE_BUTTON_4" as "Mouse 4"
		return name.starts_with("MOUSE_BUTTON_") ? "Mouse " + name.substr(13) : InWords(name);

	case InputId::EDevice::GAMEPAD_BUTTON:
		return "Pad " + InWords(name.substr(8));

	case InputId::EDevice::GAMEPAD_AXIS:
	{
		if (p_input.direction == 0)
		{
			return "Pad " + InWords(name.substr(8));
		}

		const bool left = name.starts_with("GAMEPAD_LEFT");
		const bool horizontal = name.find("_X") != std::string::npos;
		const char* way = horizontal ? (p_input.direction > 0 ? "Right" : "Left") : (p_input.direction > 0 ? "Up" : "Down");
		return std::string("Pad ") + (left ? "Left" : "Right") + " Stick " + way;
	}
	}

	return name;
}
