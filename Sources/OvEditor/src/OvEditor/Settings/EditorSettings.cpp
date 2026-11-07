/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <filesystem>

#include <OvEditor/Settings/EditorSettings.h>
#include <OvTools/Filesystem/IniFile.h>
#include <OvTools/Utils/SystemCalls.h>

template<class T>
void LoadIniEntry(OvTools::Filesystem::IniFile& iniFile, const std::string& entry, OvEditor::Settings::EditorSettings::Property<T>& out)
{
	if (iniFile.IsKeyExisting(entry))
	{
		out = iniFile.Get<T>(entry);
	}
}

OvTools::Filesystem::IniFile GetEditorIniFile()
{
	const auto filePath = std::filesystem::path{ OvTools::Utils::SystemCalls::GetPathToAppdata() } /
		"OverloadTech" /
		"OvEditor" /
		"editor.ini";

	return OvTools::Filesystem::IniFile{ filePath.string() };
}

void OvEditor::Settings::EditorSettings::Save()
{
	OvTools::Filesystem::IniFile iniFile = GetEditorIniFile();

	iniFile.RemoveAll();
	iniFile.Add("show_geometry_bounds", ShowGeometryBounds.Get());
	iniFile.Add("show_light_bounds", ShowLightBounds.Get());
	iniFile.Add("editor_frustum_geometry_culling", EditorFrustumGeometryCulling.Get());
	iniFile.Add("editor_frustum_light_culling", EditorFrustumLightCulling.Get());
	iniFile.Add("light_billboard_scale", LightBillboardScale.Get());
	iniFile.Add("reflection_probe_scale", ReflectionProbeScale.Get());
	iniFile.Add("translation_snap_unit", TranslationSnapUnit.Get());
	iniFile.Add("rotation_snap_unit", RotationSnapUnit.Get());
	iniFile.Add("scaling_snap_unit", ScalingSnapUnit.Get());
	iniFile.Add("color_theme", ColorTheme.Get());
	iniFile.Add("ui_scale", UIScale.Get());
	iniFile.Add("beta_dpi_awareness_dialog_presented", BetaDpiAwarenessDialogPresented.Get());
	iniFile.Add("console_max_logs", ConsoleMaxLogs.Get());
	iniFile.Add("code_editor_command", CodeEditorCommand.Get());
	iniFile.Add("always_regenerate_scripting_symbols", RegenerateScriptingProjectFilesOnStartup.Get());
	iniFile.Add("play_key", PlayKey.Get());
	iniFile.Add("stop_key", StopKey.Get());
	iniFile.Rewrite();
}

void OvEditor::Settings::EditorSettings::Load()
{
	OvTools::Filesystem::IniFile iniFile = GetEditorIniFile();

	LoadIniEntry<bool>(iniFile, "show_geometry_bounds", ShowGeometryBounds);
	LoadIniEntry<bool>(iniFile, "show_light_bounds", ShowLightBounds);
	LoadIniEntry<bool>(iniFile, "show_geometry_frustum_culling_in_scene_view", EditorFrustumGeometryCulling);
	LoadIniEntry<bool>(iniFile, "show_light_frustum_culling_in_scene_view", EditorFrustumLightCulling);
	LoadIniEntry<float>(iniFile, "light_billboard_scale", LightBillboardScale);
	LoadIniEntry<float>(iniFile, "reflection_probe_scale", ReflectionProbeScale);
	LoadIniEntry<float>(iniFile, "translation_snap_unit", TranslationSnapUnit);
	LoadIniEntry<float>(iniFile, "rotation_snap_unit", RotationSnapUnit);
	LoadIniEntry<float>(iniFile, "scaling_snap_unit", ScalingSnapUnit);
	LoadIniEntry<int>(iniFile, "color_theme", ColorTheme);
	LoadIniEntry<int>(iniFile, "ui_scale", UIScale);
	LoadIniEntry<bool>(iniFile, "beta_dpi_awareness_dialog_presented", BetaDpiAwarenessDialogPresented);
	LoadIniEntry<int>(iniFile, "console_max_logs", ConsoleMaxLogs);
	LoadIniEntry<std::string>(iniFile, "code_editor_command", CodeEditorCommand);
	LoadIniEntry<bool>(iniFile, "always_regenerate_scripting_symbols", RegenerateScriptingProjectFilesOnStartup);
	LoadIniEntry<int>(iniFile, "play_key", PlayKey);
	LoadIniEntry<int>(iniFile, "stop_key", StopKey);

	// A key the editor does not offer goes back to F5
	const auto isOffered = [](int p_key)
	{
		const auto& keys = GetPlayShortcutKeys();
		return std::any_of(keys.begin(), keys.end(), [p_key](const auto& p_entry) { return p_entry.first == p_key; });
	};

	for (auto* key : { &PlayKey, &StopKey })
	{
		if (!isOffered(key->Get()))
		{
			*key = static_cast<int>(OvWindowing::Inputs::EKey::KEY_F5);
		}
	}
}

const std::vector<std::pair<int, std::string>>& OvEditor::Settings::EditorSettings::GetPlayShortcutKeys()
{
	using OvWindowing::Inputs::EKey;

	static const std::vector<std::pair<int, std::string>> keys = []
	{
		std::vector<std::pair<int, std::string>> result = {
			{ static_cast<int>(EKey::KEY_UNKNOWN), "None" },
			{ static_cast<int>(EKey::KEY_ESCAPE), "Escape" },
			{ static_cast<int>(EKey::KEY_PAUSE), "Pause" },
		};

		for (int index = 0; index < 12; ++index)
		{
			result.emplace_back(static_cast<int>(EKey::KEY_F1) + index, "F" + std::to_string(index + 1));
		}

		return result;
	}();

	return keys;
}
