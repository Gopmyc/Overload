/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <filesystem>
#include <string>

#include <OvCore/Input/InputActions.h>

namespace OvTools::Filesystem
{
	class IniFile;
	class SaveStorage;
}

namespace OvCore::Input
{
	// Where the player's bindings are kept, in the project's save folder
	constexpr std::string_view kBindingsFile = "input_bindings.json";

	/**
	* The game's input as the scripts see it (InputHelpers): the editor's gate applies, and a gamepad input is read
	* on the first connected pad
	*/
	class GameInputSource final : public IInputSource
	{
	public:
		float GetValue(const InputId& p_input) const override;
		bool WasPressed(const InputId& p_input) const override;
		bool WasReleased(const InputId& p_input) const override;
	};

	/**
	* Loads the project's actions from the file its "input_actions" key names (none without the key), then the
	* player's bindings from the save folder. What cannot be read is logged as a warning
	* @param p_projectSettings
	* @param p_projectAssetsPath
	* @param p_saveStorage
	* @param p_actions
	*/
	void LoadProjectActions(const OvTools::Filesystem::IniFile& p_projectSettings, const std::filesystem::path& p_projectAssetsPath, OvTools::Filesystem::SaveStorage& p_saveStorage, InputActions& p_actions);

	/**
	* Writes the player's bindings to the save folder, and logs a warning when it cannot
	* @param p_saveStorage
	* @param p_actions
	*/
	void SaveBindings(OvTools::Filesystem::SaveStorage& p_saveStorage, const InputActions& p_actions);

	/**
	* Returns a label a menu can show for an input: a printable key as the keyboard's layout names it ("Z" for the W of
	* a French keyboard), the others in words ("Left Shift", "Mouse Right", "Pad A", "Pad Left Stick Up")
	* @param p_input
	*/
	std::string GetInputLabel(const InputId& p_input);
}
