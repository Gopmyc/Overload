/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <OvAudio/Core/AudioEngine.h>
#include <OvCore/Helpers/ProjectSettings.h>
#include <OvDebug/Logger.h>

namespace
{
	using OvPhysics::Settings::PhysicsSettings;

	std::string_view Trim(std::string_view p_text)
	{
		constexpr std::string_view kBlanks = " \t\r\n";

		const size_t first = p_text.find_first_not_of(kBlanks);
		if (first == std::string_view::npos)
		{
			return {};
		}

		const size_t last = p_text.find_last_not_of(kBlanks);
		return p_text.substr(first, last - first + 1);
	}

	/**
	* Splits the text at each separator and trims every part (an empty text gives no part)
	*/
	std::vector<std::string_view> Split(std::string_view p_text, char p_separator)
	{
		std::vector<std::string_view> parts;

		if (Trim(p_text).empty())
		{
			return parts;
		}

		size_t start = 0;
		while (true)
		{
			const size_t end = p_text.find(p_separator, start);
			parts.push_back(Trim(p_text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start)));

			if (end == std::string_view::npos)
			{
				return parts;
			}

			start = end + 1;
		}
	}

	/**
	* A layer named in the project file: its index, or the name the layer was given
	*/
	std::optional<uint16_t> ParseLayer(std::string_view p_text, const PhysicsSettings& p_settings)
	{
		if (p_text.empty())
		{
			return std::nullopt;
		}

		uint32_t index = 0;
		const auto [end, error] = std::from_chars(p_text.data(), p_text.data() + p_text.size(), index);
		if (error == std::errc{} && end == p_text.data() + p_text.size())
		{
			return index < PhysicsSettings::kLayerCount ? std::optional<uint16_t>(static_cast<uint16_t>(index)) : std::nullopt;
		}

		for (uint16_t layer = 0; layer < PhysicsSettings::kLayerCount; ++layer)
		{
			if (p_settings.layerNames[layer] == p_text)
			{
				return layer;
			}
		}

		return std::nullopt;
	}

	/**
	* A number of the project file, its default when the file lacks it, brought within range when it is out of it
	*/
	template<typename T>
	T ReadInRange(const OvTools::Filesystem::IniFile& p_projectSettings, const std::string& p_key, T p_default, T p_min, T p_max)
	{
		T value = p_default;

		if (!p_projectSettings.TryGet(p_key, value))
		{
			return p_default;
		}

		if constexpr (std::is_floating_point_v<T>)
		{
			if (std::isnan(value))
			{
				OVLOG_WARNING(p_key + ": not a number, " + std::to_string(p_default) + " is used");
				return p_default;
			}
		}

		if (value < p_min || value > p_max)
		{
			const T clamped = std::clamp(value, p_min, p_max);
			OVLOG_WARNING(p_key + ": " + std::to_string(value) + " is out of range, " + std::to_string(clamped) + " is used");
			return clamped;
		}

		return value;
	}

	void ReadLayerNames(const std::string& p_value, PhysicsSettings& p_settings)
	{
		const auto names = Split(p_value, ',');

		if (names.size() > PhysicsSettings::kLayerCount)
		{
			OVLOG_WARNING("physics_layer_names: only the first " + std::to_string(PhysicsSettings::kLayerCount) + " names are used");
		}

		for (size_t layer = 0; layer < names.size() && layer < PhysicsSettings::kLayerCount; ++layer)
		{
			p_settings.layerNames[layer] = std::string(names[layer]);
		}
	}

	void ReadIgnoredLayerPairs(const std::string& p_value, PhysicsSettings& p_settings)
	{
		for (const std::string_view pair : Split(p_value, ','))
		{
			const size_t colon = pair.find(':');
			const auto layer1 = colon == std::string_view::npos ? std::nullopt : ParseLayer(Trim(pair.substr(0, colon)), p_settings);
			const auto layer2 = colon == std::string_view::npos ? std::nullopt : ParseLayer(Trim(pair.substr(colon + 1)), p_settings);

			if (!layer1 || !layer2)
			{
				OVLOG_WARNING("physics_ignored_layer_pairs: \"" + std::string(pair) + "\" is not a pair of layers (\"a:b\"), skipped");
				continue;
			}

			auto& mask1 = p_settings.collisionMasks[*layer1];
			auto& mask2 = p_settings.collisionMasks[*layer2];
			mask1 = static_cast<uint16_t>(mask1 & ~(1u << *layer2));
			mask2 = static_cast<uint16_t>(mask2 & ~(1u << *layer1));
		}
	}
}

OvPhysics::Settings::PhysicsSettings OvCore::Helpers::ProjectSettings::ReadPhysicsSettings(const OvTools::Filesystem::IniFile& p_projectSettings)
{
	PhysicsSettings settings;

	settings.gravity = { 0.0f, p_projectSettings.GetOrDefault<float>("gravity", settings.gravity.y), 0.0f };

	// The names come first, so that the other keys can name the layers
	if (std::string names; p_projectSettings.TryGet("physics_layer_names", names))
	{
		ReadLayerNames(names, settings);
	}

	if (std::string pairs; p_projectSettings.TryGet("physics_ignored_layer_pairs", pairs))
	{
		ReadIgnoredLayerPairs(pairs, settings);
	}

	if (std::string ragdollLayer; p_projectSettings.TryGet("physics_ragdoll_layer", ragdollLayer) && !Trim(ragdollLayer).empty())
	{
		if (const auto layer = ParseLayer(Trim(ragdollLayer), settings))
		{
			settings.ragdollLayer = *layer;
		}
		else
		{
			OVLOG_WARNING("physics_ragdoll_layer: no layer \"" + ragdollLayer + "\", the ragdolls stay on layer " + std::to_string(settings.ragdollLayer));
		}
	}

	settings.ragdollIgnoresDynamicBodies = p_projectSettings.GetOrDefault<bool>("physics_ragdoll_ignores_dynamic_bodies", settings.ragdollIgnoresDynamicBodies);

	return settings;
}

std::string OvCore::Helpers::ProjectSettings::GetSaveName(const OvTools::Filesystem::IniFile& p_projectSettings)
{
	// The project's identity outlives its executable's name, so that renaming the game keeps its saves
	for (const char* key : { "project_id", "executable_name" })
	{
		const std::string value = p_projectSettings.GetOrDefault<std::string>(key, {});

		if (const std::string_view name = Trim(value); !name.empty())
		{
			return std::string(name);
		}
	}

	return "Game";
}

OvCore::Helpers::ProjectSettings::RuntimeLimits OvCore::Helpers::ProjectSettings::ReadRuntimeLimits(const OvTools::Filesystem::IniFile& p_projectSettings)
{
	constexpr size_t kMebibyte = 1024 * 1024;

	RuntimeLimits limits;

	const int fileMiB = ReadInRange(p_projectSettings, "storage_max_file_mib", 4, 1, 1024);
	const int totalMiB = ReadInRange(p_projectSettings, "storage_max_total_mib", std::max(64, fileMiB), fileMiB, 16384);
	limits.storage.maxFileSize = static_cast<size_t>(fileMiB) * kMebibyte;
	limits.storage.maxTotalSize = static_cast<size_t>(totalMiB) * kMebibyte;
	limits.storage.maxEntryCount = static_cast<size_t>(ReadInRange(p_projectSettings, "storage_max_entries", 1024, 1, 1000000));

	limits.json.maxSize = static_cast<size_t>(ReadInRange(p_projectSettings, "json_max_mib", 4, 1, 1024)) * kMebibyte;
	limits.json.maxDepth = ReadInRange(p_projectSettings, "json_max_depth", 64, 1, 256);

	limits.maxFrameDelta = ReadInRange(p_projectSettings, "max_frame_delta", 0.1f, 0.001f, 1.0f);

	return limits;
}

std::vector<std::string> OvCore::Helpers::ProjectSettings::ReadAudioBuses(const OvTools::Filesystem::IniFile& p_projectSettings)
{
	using OvAudio::Core::AudioEngine;

	std::string value = kDefaultAudioBuses;
	p_projectSettings.TryGet("audio_buses", value);

	std::vector<std::string> buses;

	for (const std::string_view entry : Split(value, ','))
	{
		const std::string name(entry);

		if (name.empty())
		{
			continue;
		}

		if (name == AudioEngine::kMasterBus)
		{
			OVLOG_WARNING("audio_buses: \"Master\" is the volume of every sound, always there, not a bus to name: left out");
		}
		else if (std::ranges::find(buses, name) != buses.end())
		{
			OVLOG_WARNING("audio_buses: \"" + name + "\" is named twice: left out the second time");
		}
		else if (buses.size() == AudioEngine::kMaxBuses)
		{
			OVLOG_WARNING("audio_buses: only the first " + std::to_string(AudioEngine::kMaxBuses) + " buses are used");
			break;
		}
		else
		{
			buses.push_back(name);
		}
	}

	return buses;
}
