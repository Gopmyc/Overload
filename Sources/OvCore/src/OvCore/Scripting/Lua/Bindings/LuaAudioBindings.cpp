/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cmath>
#include <format>
#include <string>

#include <OvCore/Scripting/Lua/LuaComponentRef.h>

#include <OvAudio/Core/AudioEngine.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvMaths/FVector3.h>

namespace
{
	using OvAudio::Core::AudioEngine;
	using OvAudio::Settings::EAttenuationModel;

	AudioEngine& Engine()
	{
		return OVSERVICE(AudioEngine);
	}

	void CheckBus(const char* p_function, const std::string& p_bus)
	{
		if (!Engine().HasBus(p_bus))
		{
			throw sol::error(std::format("Audio.{}: no bus '{}' (Master, or one of the project's audio_buses)", p_function, p_bus));
		}
	}

	float ReadNumber(const char* p_function, const std::string& p_name, const sol::object& p_value)
	{
		if (p_value.get_type() != sol::type::number || !std::isfinite(p_value.as<double>()))
		{
			throw sol::error(std::format("Audio.{}: '{}' must be a number", p_function, p_name));
		}

		return p_value.as<float>();
	}

	/**
	* The options given to PlayOneShot, over the defaults: a key it does not know raises an error, a misspelt key would
	* otherwise be silently dropped
	*/
	OvAudio::Core::OneShotSettings ReadOneShotSettings(const sol::optional<sol::table>& p_table)
	{
		constexpr const char* kFunction = "PlayOneShot";
		OvAudio::Core::OneShotSettings settings;

		if (!p_table)
		{
			return settings;
		}

		for (const auto& [key, value] : *p_table)
		{
			const std::string name = key.is<std::string>() ? key.as<std::string>() : std::string{};

			if (name == "position")
			{
				if (!value.is<OvMaths::FVector3>())
				{
					throw sol::error("Audio.PlayOneShot: 'position' must be a Vector3");
				}
				settings.position = value.as<OvMaths::FVector3>();
			}
			else if (name == "bus")
			{
				if (value.get_type() != sol::type::string)
				{
					throw sol::error("Audio.PlayOneShot: 'bus' must be a bus name");
				}
				settings.bus = value.as<std::string>();
				CheckBus(kFunction, settings.bus);
			}
			else if (name == "attenuation")
			{
				const float model = ReadNumber(kFunction, name, value);
				if (model != std::floor(model) || model < 0.0f || model > static_cast<float>(EAttenuationModel::EXPONENTIAL_DISTANCE))
				{
					throw sol::error("Audio.PlayOneShot: 'attenuation' must be an AttenuationModel");
				}
				settings.attenuation.model = static_cast<EAttenuationModel>(static_cast<int>(model));
			}
			else if (name == "volume") settings.volume = ReadNumber(kFunction, name, value);
			else if (name == "pan") settings.pan = ReadNumber(kFunction, name, value);
			else if (name == "pitch") settings.pitch = ReadNumber(kFunction, name, value);
			else if (name == "pitchVariation") settings.pitchVariation = ReadNumber(kFunction, name, value);
			else if (name == "minDistance") settings.attenuation.minDistance = ReadNumber(kFunction, name, value);
			else if (name == "maxDistance") settings.attenuation.maxDistance = ReadNumber(kFunction, name, value);
			else if (name == "rolloff") settings.attenuation.rolloff = ReadNumber(kFunction, name, value);
			else
			{
				throw sol::error(std::format(
					"Audio.PlayOneShot: unknown option '{}' (position, volume, pan, pitch, pitchVariation, bus, attenuation, minDistance, maxDistance, rolloff)",
					name
				));
			}
		}

		return settings;
	}
}

void BindLuaAudio(sol::state& p_luaState)
{
	p_luaState.create_named_table("Audio",
		"GetDriver", []() { return std::string(Engine().GetDriverName()); },
		"GetVoiceCount", []() { return Engine().GetVoiceCount(); },
		"GetBuses", [](sol::this_state p_state)
		{
			sol::state_view lua(p_state);
			sol::table buses = lua.create_table();
			for (const auto& name : Engine().GetBusNames())
			{
				buses.add(name);
			}
			return buses;
		},
		"SetVolume", [](const std::string& p_bus, float p_volume)
		{
			CheckBus("SetVolume", p_bus);
			if (!std::isfinite(p_volume) || p_volume < 0.0f)
			{
				throw sol::error("Audio.SetVolume: a volume is 0 or more");
			}
			Engine().SetVolume(p_bus, p_volume);
		},
		"GetVolume", [](const std::string& p_bus)
		{
			CheckBus("GetVolume", p_bus);
			return Engine().GetVolume(p_bus).value_or(1.0f);
		},
		"PlayOneShot", [](OvAudio::Resources::Sound* p_sound, sol::optional<sol::table> p_options)
		{
			if (!p_sound)
			{
				throw sol::error("Audio.PlayOneShot: a sound is needed (Resources.GetSound)");
			}

			const auto instance = Engine().PlayOneShot(*p_sound, ReadOneShotSettings(p_options));
			return instance && instance->IsValid();
		}
	);
}
