/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <format>
#include <ranges>

#include <soloud.h>
#include <soloud_bus.h>
#include <soloud_wav.h>

#include <OvAudio/Core/AudioEngine.h>
#include <OvDebug/Assertion.h>
#include <OvDebug/Logger.h>

OvAudio::Core::AudioEngine::AudioEngine(EDriver p_driver)
{
	m_backend = std::make_unique<SoLoud::Soloud>();

	bool started = false;

	if (p_driver == EDriver::AUTO)
	{
		started = m_backend->init() == SoLoud::SOLOUD_ERRORS::SO_NO_ERROR;

		// Without an audio device (a headless machine, a CI runner), the game runs on with the sounds mixed and
		// discarded: a warning, so that a run that fails on errors does not fail for it
		if (!started)
		{
			OVLOG_WARNING("No audio device: the sounds are mixed and discarded, so that they play, end and loop as with one");
		}
	}

	if (!started)
	{
		started = m_backend->init(
			SoLoud::Soloud::CLIP_ROUNDOFF,
			SoLoud::Soloud::NULLDRIVER,
			kSampleRate,
			SoLoud::Soloud::AUTO,
			kChannels
		) == SoLoud::SOLOUD_ERRORS::SO_NO_ERROR;

		m_nullDriver = started;
	}

	if (!started)
	{
		OVLOG_WARNING("Failed to initialize the audio engine. Playback requests will be ignored.");
		m_backend.reset();
		return;
	}

	m_backend->setMaxActiveVoiceCount(kMaxActiveVoices);

	// SoLoud's listener looks along -Z until told otherwise. The engine's looks along +Z, as a camera with no turn,
	// from the start: a sound played before the first update begins on the side it is on
	m_backend->set3dListenerParameters(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f);
	m_backend->update3dAudio();

	using AudioSourceReceiver = void(AudioEngine::*)(OvAudio::Entities::AudioSource&);
	using AudioListenerReceiver = void(AudioEngine::*)(OvAudio::Entities::AudioListener&);

	m_audioSourceCreatedListenerID = Entities::AudioSource::CreatedEvent +=
		std::bind(static_cast<AudioSourceReceiver>(&AudioEngine::Consider), this, std::placeholders::_1);
	m_audioSourceDestroyedListenerID = Entities::AudioSource::DestroyedEvent +=
		std::bind(static_cast<AudioSourceReceiver>(&AudioEngine::Unconsider), this, std::placeholders::_1);
	m_audioListenerCreatedListenerID = Entities::AudioListener::CreatedEvent +=
		std::bind(static_cast<AudioListenerReceiver>(&AudioEngine::Consider), this, std::placeholders::_1);
	m_audioListenerDestroyedListenerID = Entities::AudioListener::DestroyedEvent +=
		std::bind(static_cast<AudioListenerReceiver>(&AudioEngine::Unconsider), this, std::placeholders::_1);
}

OvAudio::Core::AudioEngine::~AudioEngine()
{
	if (m_audioSourceCreatedListenerID.has_value())
	{
		Entities::AudioSource::CreatedEvent.RemoveListener(m_audioSourceCreatedListenerID.value());
	}

	if (m_audioSourceDestroyedListenerID.has_value())
	{
		Entities::AudioSource::DestroyedEvent.RemoveListener(m_audioSourceDestroyedListenerID.value());
	}

	if (m_audioListenerCreatedListenerID.has_value())
	{
		Entities::AudioListener::CreatedEvent.RemoveListener(m_audioListenerCreatedListenerID.value());
	}

	if (m_audioListenerDestroyedListenerID.has_value())
	{
		Entities::AudioListener::DestroyedEvent.RemoveListener(m_audioListenerDestroyedListenerID.value());
	}

	if (IsValid())
	{
		// A bus stops itself as it goes, which needs the backend
		m_oneShots.clear();
		m_buses.clear();
		m_backend->deinit();
	}
}

bool OvAudio::Core::AudioEngine::IsValid() const
{
	return m_backend.operator bool();
}

void OvAudio::Core::AudioEngine::Update(float p_deltaTime)
{
	if (!IsValid())
	{
		return;
	}

	for (const auto& source : m_audioSources)
	{
		source.get().Update();
	}

	// The listener of the last listener created (if any) hears as its actor looks: along its forward, its up above, so
	// that a sound on the left of a camera's picture is heard on the left
	const auto listener = FindMainListener();

	if (listener.has_value())
	{
		const auto& transform = listener->GetTransform();
		const auto& pos = transform.GetWorldPosition();
		const auto at = transform.GetWorldForward();
		const auto up = transform.GetWorldUp();
		m_backend->set3dListenerPosition(pos.x, pos.y, pos.z);
		m_backend->set3dListenerAt(at.x, at.y, at.z);
		m_backend->set3dListenerUp(up.x, up.y, up.z);
	}
	else
	{
		m_backend->set3dListenerPosition(0.0f, 0.0f, 0.0f);
		m_backend->set3dListenerAt(0.0f, 0.0f, 1.0f);
		m_backend->set3dListenerUp(0.0f, 1.0f, 0.0f);
	}

	m_backend->update3dAudio();

	// The sounds played once that ended are let go
	std::erase_if(m_oneShots, [](const auto& p_instance) { return !p_instance->IsValid(); });

	// The null driver plays nothing by itself: the frame's step is mixed here, so that sounds end and loop on time
	if (m_nullDriver && !m_manualMixing && p_deltaTime > 0.0f)
	{
		m_unmixedFrames += static_cast<double>(std::min(p_deltaTime, kMaxMixedSeconds)) * kSampleRate;

		// Whole frames only: the rest waits for the next update
		const auto frames = static_cast<uint32_t>(m_unmixedFrames);
		m_unmixedFrames -= frames;

		m_mixBuffer.resize(static_cast<size_t>(kMixChunkFrames) * kChannels);
		for (uint32_t mixed = 0; mixed < frames; mixed += kMixChunkFrames)
		{
			m_backend->mix(m_mixBuffer.data(), std::min(kMixChunkFrames, frames - mixed));
		}
	}
}

void OvAudio::Core::AudioEngine::Suspend()
{
	// Sound by sound, the sources' and the ones played once: a handle outlives its sound safely, unlike a source
	const auto suspend = [this](const std::shared_ptr<Data::SoundInstance>& p_instance)
	{
		if (p_instance && p_instance->IsValid() && !p_instance->IsPaused())
		{
			p_instance->Pause();
			m_suspendedSounds.push_back(p_instance->GetHandle());
		}
	};

	if (IsValid())
	{
		for (auto& source : m_audioSources)
		{
			suspend(source.get().GetSoundInstance().lock());
		}

		for (const auto& oneShot : m_oneShots)
		{
			suspend(oneShot);
		}
	}

	m_suspended = true;
}

void OvAudio::Core::AudioEngine::Unsuspend()
{
	// Only what Suspend paused: a sound the game paused itself stays paused
	if (IsValid())
	{
		for (const auto handle : m_suspendedSounds)
		{
			if (m_backend->isValidVoiceHandle(handle) && m_backend->getPause(handle))
			{
				m_backend->setPause(handle, false);
			}
		}
	}

	m_suspendedSounds.clear();
	m_suspended = false;
}

bool OvAudio::Core::AudioEngine::IsSuspended() const
{
	return m_suspended;
}

std::shared_ptr<OvAudio::Data::SoundInstance> OvAudio::Core::AudioEngine::Play2D(const Resources::Sound& p_sound, float p_pan, std::optional<float> p_volume, bool p_startPaused, std::string_view p_bus)
{
	if (!IsValid())
	{
		OVLOG_WARNING(std::format("Unable to play {}. Audio engine is not valid", p_sound.path));
		return {};
	}

	auto* bus = FindBus(p_bus);
	const auto handle = bus ?
		bus->play(*p_sound.audioData, p_volume.value_or(-1.0f), p_pan, p_startPaused) :
		m_backend->play(*p_sound.audioData, p_volume.value_or(-1.0f), p_pan, p_startPaused);

	return std::make_shared<Data::SoundInstance>(
		*m_backend,
		handle,
		false
	);
}

std::shared_ptr<OvAudio::Data::SoundInstance> OvAudio::Core::AudioEngine::Play3D(
	const Resources::Sound& p_sound,
	const OvMaths::FVector3& p_position,
	const OvMaths::FVector3& p_velocity,
	std::optional<float> p_volume,
	bool p_startPaused,
	std::string_view p_bus
)
{
	if (!IsValid())
	{
		OVLOG_WARNING(std::format("Unable to play {}. Audio engine is not valid", p_sound.path));
		return {};
	}

	auto* bus = FindBus(p_bus);
	const auto handle = bus ?
		bus->play3d(
			*p_sound.audioData,
			p_position.x, p_position.y, p_position.z,
			p_velocity.x, p_velocity.y, p_velocity.z,
			p_volume.value_or(-1.0f),
			p_startPaused
		) :
		m_backend->play3d(
			*p_sound.audioData,
			p_position.x, p_position.y, p_position.z,
			p_velocity.x, p_velocity.y, p_velocity.z,
			p_volume.value_or(-1.0f),
			p_startPaused
		);

	return std::make_shared<Data::SoundInstance>(
		*m_backend,
		handle,
		true
	);
}

OvTools::Utils::OptRef<OvAudio::Entities::AudioListener> OvAudio::Core::AudioEngine::FindMainListener(bool p_includeDisabled) const
{
	for (auto& listener : m_audioListeners | std::views::reverse)
	{
		if (p_includeDisabled || listener.get().IsEnabled())
		{
			return listener.get();
		}
	}

	return std::nullopt;
}

bool OvAudio::Core::AudioEngine::IsNullDriver() const
{
	return IsValid() && m_nullDriver;
}

std::string_view OvAudio::Core::AudioEngine::GetDriverName() const
{
	if (!IsValid())
	{
		return "none";
	}

	return m_nullDriver ? "null" : "device";
}

uint32_t OvAudio::Core::AudioEngine::GetVoiceCount() const
{
	if (!IsValid())
	{
		return 0;
	}

	// The buses play as voices too, for the whole run
	const auto voices = m_backend->getVoiceCount();
	const auto buses = static_cast<uint32_t>(m_busVoices.size());
	return voices > buses ? voices - buses : 0;
}

std::shared_ptr<OvAudio::Data::SoundInstance> OvAudio::Core::AudioEngine::PlayOneShot(const Resources::Sound& p_sound, const OneShotSettings& p_settings)
{
	const float volume = std::max(p_settings.volume, 0.0f);

	auto instance = p_settings.position ?
		Play3D(p_sound, *p_settings.position, OvMaths::FVector3::Zero, volume, true, p_settings.bus) :
		Play2D(p_sound, p_settings.pan, volume, true, p_settings.bus);

	if (!instance || !instance->IsValid())
	{
		return {};
	}

	instance->SetPitch(VaryPitch(p_settings.pitch, p_settings.pitchVariation));

	// A sound played once that no one can hear ends, rather than wait (SoLoud stops moving it on) to be heard
	m_backend->setInaudibleBehavior(instance->GetHandle(), false, true);

	if (instance->IsSpatial())
	{
		instance->SetAttenuation(p_settings.attenuation);
		m_backend->update3dAudio();
	}

	// It may already have ended, too far to be heard
	if (instance->IsValid())
	{
		instance->Play();
		m_oneShots.push_back(instance);
	}

	return instance;
}

void OvAudio::Core::AudioEngine::StopOneShots()
{
	for (const auto& oneShot : m_oneShots)
	{
		if (oneShot->IsValid())
		{
			oneShot->Stop();
		}
	}

	m_oneShots.clear();
}

void OvAudio::Core::AudioEngine::SetBuses(const std::vector<std::string>& p_names)
{
	std::vector<std::string> names(p_names.begin(), p_names.begin() + std::min<size_t>(p_names.size(), kMaxBuses));

	if (!IsValid() || names == m_busNames)
	{
		return;
	}

	// A bus is a voice that plays the voices sent to it, always: protected from being taken for another sound, and
	// moving them on even at a volume of 0, so that a sound on a silent bus still ends on time
	while (m_buses.size() < names.size())
	{
		auto& bus = m_buses.emplace_back(std::make_unique<SoLoud::Bus>());
		const auto voice = m_backend->play(*bus);
		m_backend->setProtectVoice(voice, true);
		m_backend->setInaudibleBehavior(voice, true, false);

		// Both channels whole: SoLoud's pan law would take 3 dB off at the centre, so that a sound on a bus at a
		// volume of 1 would be quieter than on the Master
		m_backend->setPanAbsolute(voice, 1.0f, 1.0f);
		m_busVoices.push_back(voice);
	}

	for (const auto voice : m_busVoices)
	{
		m_backend->setVolume(voice, 1.0f);
	}

	m_busNames = std::move(names);
	m_warnedBuses.clear();

	// The buses hold a voice each, beside the sounds heard
	m_backend->setMaxActiveVoiceCount(kMaxActiveVoices + static_cast<uint32_t>(m_busVoices.size()));
}

const std::vector<std::string>& OvAudio::Core::AudioEngine::GetBusNames() const
{
	return m_busNames;
}

bool OvAudio::Core::AudioEngine::HasBus(std::string_view p_bus) const
{
	return p_bus == kMasterBus || std::ranges::find(m_busNames, p_bus) != m_busNames.end();
}

bool OvAudio::Core::AudioEngine::SetVolume(std::string_view p_bus, float p_volume)
{
	const float volume = std::isfinite(p_volume) ? std::max(p_volume, 0.0f) : 1.0f;

	if (p_bus == kMasterBus)
	{
		if (IsValid())
		{
			m_backend->setGlobalVolume(volume);
		}
		return true;
	}

	const auto found = std::ranges::find(m_busNames, p_bus);
	if (found == m_busNames.end())
	{
		return false;
	}

	m_backend->setVolume(m_busVoices[static_cast<size_t>(found - m_busNames.begin())], volume);
	return true;
}

std::optional<float> OvAudio::Core::AudioEngine::GetVolume(std::string_view p_bus) const
{
	if (p_bus == kMasterBus)
	{
		return IsValid() ? m_backend->getGlobalVolume() : 1.0f;
	}

	const auto found = std::ranges::find(m_busNames, p_bus);
	if (found == m_busNames.end())
	{
		return std::nullopt;
	}

	return m_backend->getVolume(m_busVoices[static_cast<size_t>(found - m_busNames.begin())]);
}

void OvAudio::Core::AudioEngine::ResetVolumes()
{
	if (!IsValid())
	{
		return;
	}

	m_backend->setGlobalVolume(1.0f);

	for (const auto voice : m_busVoices)
	{
		m_backend->setVolume(voice, 1.0f);
	}
}

float OvAudio::Core::AudioEngine::VaryPitch(float p_pitch, float p_variation)
{
	const float pitch = std::isfinite(p_pitch) ? p_pitch : 1.0f;
	const float variation = std::isfinite(p_variation) ? std::abs(p_variation) : 0.0f;

	if (variation <= 0.0f)
	{
		return std::max(pitch, kMinPitch);
	}

	std::uniform_real_distribution<float> draw(pitch - variation, pitch + variation);
	return std::max(draw(m_random), kMinPitch);
}

void OvAudio::Core::AudioEngine::SetRandomSeed(uint32_t p_seed)
{
	m_random.seed(p_seed);
}

SoLoud::Bus* OvAudio::Core::AudioEngine::FindBus(std::string_view p_bus)
{
	if (p_bus.empty() || p_bus == kMasterBus)
	{
		return nullptr;
	}

	if (const auto found = std::ranges::find(m_busNames, p_bus); found != m_busNames.end())
	{
		return m_buses[static_cast<size_t>(found - m_busNames.begin())].get();
	}

	// A bus the project no longer has: the Master, said once
	if (m_warnedBuses.emplace(p_bus).second)
	{
		OVLOG_WARNING("No audio bus \"" + std::string(p_bus) + "\" in the project: its sounds play on the Master");
	}

	return nullptr;
}

bool OvAudio::Core::AudioEngine::Mix(float* p_buffer, uint32_t p_frames)
{
	if (!IsNullDriver())
	{
		return false;
	}

	for (uint32_t mixed = 0; mixed < p_frames; mixed += kMixChunkFrames)
	{
		m_backend->mix(p_buffer + static_cast<size_t>(mixed) * kChannels, std::min(kMixChunkFrames, p_frames - mixed));
	}

	return true;
}

void OvAudio::Core::AudioEngine::SetManualMixing(bool p_manual)
{
	m_manualMixing = p_manual;
}

SoLoud::Soloud& OvAudio::Core::AudioEngine::GetBackend() const
{
	OVASSERT(IsValid(), "Audio engine is not valid");
	return *m_backend;
}

void OvAudio::Core::AudioEngine::Consider(OvAudio::Entities::AudioSource & p_audioSource)
{
	m_audioSources.push_back(std::ref(p_audioSource));
}

void OvAudio::Core::AudioEngine::Consider(OvAudio::Entities::AudioListener & p_audioListener)
{
	m_audioListeners.push_back(std::ref(p_audioListener));
}

void OvAudio::Core::AudioEngine::Unconsider(OvAudio::Entities::AudioSource & p_audioSource)
{
	auto found = std::find_if(m_audioSources.begin(), m_audioSources.end(), [&p_audioSource](std::reference_wrapper<Entities::AudioSource> element)
	{
		return std::addressof(p_audioSource) == std::addressof(element.get());
	});

	if (found != m_audioSources.end())
		m_audioSources.erase(found);
}

void OvAudio::Core::AudioEngine::Unconsider(OvAudio::Entities::AudioListener & p_audioListener)
{
	auto found = std::find_if(m_audioListeners.begin(), m_audioListeners.end(), [&p_audioListener](std::reference_wrapper<Entities::AudioListener> element)
	{
		return std::addressof(p_audioListener) == std::addressof(element.get());
	});

	if (found != m_audioListeners.end())
		m_audioListeners.erase(found);
}
