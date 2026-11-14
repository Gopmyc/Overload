/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <format>
#include <ranges>

#include <soloud.h>
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
	for (auto& audioSourceRef : m_audioSources)
	{
		auto& source = audioSourceRef.get();
		if (source.HasSound() && !source.IsPaused())
		{
			m_suspendedAudioSources.push_back(audioSourceRef);
			source.Pause();
		}
	}

	m_suspended = true;
}

void OvAudio::Core::AudioEngine::Unsuspend()
{
	// Only what Suspend paused: a source the game paused itself stays paused
	for (auto& audioSourceRef : m_suspendedAudioSources)
	{
		auto& source = audioSourceRef.get();
		if (source.HasSound() && source.IsPaused())
		{
			source.Resume();
		}
	}

	m_suspendedAudioSources.clear();
	m_suspended = false;
}

bool OvAudio::Core::AudioEngine::IsSuspended() const
{
	return m_suspended;
}

std::shared_ptr<OvAudio::Data::SoundInstance> OvAudio::Core::AudioEngine::Play2D(const Resources::Sound& p_sound, float p_pan, std::optional<float> p_volume, bool p_startPaused)
{
	if (!IsValid())
	{
		OVLOG_WARNING(std::format("Unable to play {}. Audio engine is not valid", p_sound.path));
		return {};
	}

	const auto handle = m_backend->play(
		*p_sound.audioData,
		p_volume.value_or(-1.0f),
		p_pan,
		p_startPaused
	);

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
	bool p_startPaused
)
{
	if (!IsValid())
	{
		OVLOG_WARNING(std::format("Unable to play {}. Audio engine is not valid", p_sound.path));
		return {};
	}

	const auto handle = m_backend->play3d(
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
	return IsValid() ? m_backend->getVoiceCount() : 0;
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

	if (m_suspended)
	{
		auto found = std::find_if(m_suspendedAudioSources.begin(), m_suspendedAudioSources.end(), [&p_audioSource](std::reference_wrapper<Entities::AudioSource> element)
		{
			return std::addressof(p_audioSource) == std::addressof(element.get());
		});

		if (found != m_suspendedAudioSources.end())
			m_suspendedAudioSources.erase(found);
	}
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
