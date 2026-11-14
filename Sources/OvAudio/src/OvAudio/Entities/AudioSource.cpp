/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>

#include <soloud.h>
#include <soloud_wav.h>

#include <OvAudio/Core/AudioEngine.h>
#include <OvAudio/Entities/AudioSource.h>
#include <OvDebug/Assertion.h>

OvTools::Eventing::Event<OvAudio::Entities::AudioSource&> OvAudio::Entities::AudioSource::CreatedEvent;
OvTools::Eventing::Event<OvAudio::Entities::AudioSource&> OvAudio::Entities::AudioSource::DestroyedEvent;

OvAudio::Entities::AudioSource::AudioSource(Core::AudioEngine& p_engine, OvTools::Utils::OptRef<OvMaths::FTransform> p_transform) :
	m_engine(p_engine),
	m_transform(p_transform)
{
	CreatedEvent.Invoke(*this);
}

OvAudio::Entities::AudioSource::~AudioSource()
{
	Stop();
	DestroyedEvent.Invoke(*this);
}

const OvMaths::FTransform& OvAudio::Entities::AudioSource::GetTransform()
{
	return m_transform;
}

void OvAudio::Entities::AudioSource::ApplySourceSettingsToTrackedSound()
{
	m_instance->SetVolume(m_volume);
	m_instance->SetPan(m_pan);
	m_instance->SetLooped(m_looped);
	m_instance->SetPitch(m_pitch);
	m_instance->SetAttenuation(m_attenuation);
}

bool OvAudio::Entities::AudioSource::HasSound() const
{
	return m_instance && m_instance->IsValid();
}

bool OvAudio::Entities::AudioSource::IsPlaying() const
{
	return HasSound() && !IsPaused();
}

void OvAudio::Entities::AudioSource::Replay()
{
	// A voice keeps its mode and its bus: a sound playing starts again with the new ones, where it was, as fast as it
	// went (its pitch drawn once), and paused if it was
	if (HasSound() && m_sound)
	{
		auto& backend = m_engine.GetBackend();
		const bool paused = IsPaused();
		const double position = backend.getStreamPosition(m_instance->GetHandle());
		const float speed = backend.getRelativePlaySpeed(m_instance->GetHandle());

		Play(*m_sound);

		if (HasSound())
		{
			backend.seek(m_instance->GetHandle(), position);
			m_instance->SetPitch(speed);

			if (paused)
			{
				m_instance->Pause();
			}
		}
	}
}

void OvAudio::Entities::AudioSource::SetSpatial(bool p_value)
{
	m_spatial = p_value;

	if (HasSound() && m_instance->IsSpatial() != m_spatial)
	{
		Replay();
	}
}

void OvAudio::Entities::AudioSource::SetBus(const std::string& p_bus)
{
	const std::string bus = p_bus == Core::AudioEngine::kMasterBus ? std::string() : p_bus;

	if (bus != m_bus)
	{
		m_bus = bus;
		Replay();
	}
}

const std::string& OvAudio::Entities::AudioSource::GetBus() const
{
	return m_bus;
}

void OvAudio::Entities::AudioSource::SetPitchVariation(float p_variation)
{
	m_pitchVariation = std::isfinite(p_variation) ? std::abs(p_variation) : 0.0f;
}

float OvAudio::Entities::AudioSource::GetPitchVariation() const
{
	return m_pitchVariation;
}

void OvAudio::Entities::AudioSource::SetAttenuationThreshold(float p_distance)
{
	m_attenuation.minDistance = std::isfinite(p_distance) ? std::max(p_distance, Settings::AttenuationSettings::kSmallestDistance) : 1.0f;
	ApplyAttenuation();
}

void OvAudio::Entities::AudioSource::SetAttenuationModel(Settings::EAttenuationModel p_model)
{
	m_attenuation.model = p_model;
	ApplyAttenuation();
}

void OvAudio::Entities::AudioSource::SetRolloff(float p_rolloff)
{
	m_attenuation.rolloff = std::isfinite(p_rolloff) ? std::max(p_rolloff, 0.0f) : 1.0f;
	ApplyAttenuation();
}

void OvAudio::Entities::AudioSource::SetMaxDistance(float p_distance)
{
	m_attenuation.maxDistance = std::isfinite(p_distance) ? std::max(p_distance, Settings::AttenuationSettings::kSmallestDistance) : 500.0f;
	ApplyAttenuation();
}

void OvAudio::Entities::AudioSource::ApplyAttenuation()
{
	if (HasSound())
	{
		m_instance->SetAttenuation(m_attenuation);
	}
}

void OvAudio::Entities::AudioSource::SetVolume(float p_volume)
{
	m_volume = p_volume;

	if (HasSound())
	{
		m_instance->SetVolume(m_volume);
	}
}

void OvAudio::Entities::AudioSource::SetPan(float p_pan)
{
	m_pan = p_pan;

	if (HasSound())
	{
		m_instance->SetPan(m_pan);
	}
}

void OvAudio::Entities::AudioSource::SetLooped(bool p_looped)
{
	m_looped = p_looped;

	if (HasSound())
	{
		m_instance->SetLooped(p_looped);
	}
}

void OvAudio::Entities::AudioSource::SetPitch(float p_pitch)
{
	m_pitch = p_pitch;

	if (HasSound())
	{
		m_instance->SetPitch(p_pitch);
	}
}

bool OvAudio::Entities::AudioSource::IsPaused() const
{
	OVASSERT(HasSound(), "Cannot check if the sound is paused if no sound is currently being tracked");
	return m_instance->IsPaused();
}

std::weak_ptr<OvAudio::Data::SoundInstance> OvAudio::Entities::AudioSource::GetSoundInstance() const
{
	return m_instance;
}

bool OvAudio::Entities::AudioSource::IsSpatial() const
{
	return m_spatial;
}

float OvAudio::Entities::AudioSource::GetAttenuationThreshold() const
{
	return m_attenuation.minDistance;
}

OvAudio::Settings::EAttenuationModel OvAudio::Entities::AudioSource::GetAttenuationModel() const
{
	return m_attenuation.model;
}

float OvAudio::Entities::AudioSource::GetRolloff() const
{
	return m_attenuation.rolloff;
}

float OvAudio::Entities::AudioSource::GetMaxDistance() const
{
	return m_attenuation.maxDistance;
}

const OvAudio::Settings::AttenuationSettings& OvAudio::Entities::AudioSource::GetAttenuation() const
{
	return m_attenuation;
}

float OvAudio::Entities::AudioSource::GetVolume() const
{
	return m_volume;
}

float OvAudio::Entities::AudioSource::GetPan() const
{
	return m_pan;
}

bool OvAudio::Entities::AudioSource::IsLooped() const
{
	return m_looped;
}

float OvAudio::Entities::AudioSource::GetPitch() const
{
	return m_pitch;
}

void OvAudio::Entities::AudioSource::Play(const Resources::Sound& p_sound)
{
	// The sound before goes, the ones played once stay
	if (HasSound())
	{
		m_instance->Stop();
	}

	if (m_spatial)
	{
		m_instance = m_engine.Play3D(
			p_sound,
			m_transform->GetWorldPosition(),
			OvMaths::FVector3::Zero, // TODO: Add support for non-zero velocity
			m_volume,
			true,
			m_bus
		);

		if (m_instance)
		{
			m_instance->SetAttenuation(m_attenuation);

			// Potentially expensive? But necessary so that the settings set above are applied.
			// Otherwise some spatialized sounds may play without attenuation until the next update.
			// This isn't ideal, but I couldn't find a better way to do it (no `update3dAudio` for a single sound instance).
			m_engine.GetBackend().update3dAudio();
		}
	}
	else
	{
		m_instance = m_engine.Play2D(p_sound, m_pan, m_volume, true, m_bus);
	}

	if (m_instance)
	{
		m_sound = &p_sound;
		m_instance->SetLooped(m_looped);
		m_instance->SetPitch(m_engine.VaryPitch(m_pitch, m_pitchVariation));
		m_instance->Play();
	}
}

void OvAudio::Entities::AudioSource::PlayOneShot(const Resources::Sound& p_sound, float p_volumeScale)
{
	Core::OneShotSettings settings;
	settings.position = m_spatial ? std::optional(m_transform->GetWorldPosition()) : std::nullopt;
	settings.volume = m_volume * (std::isfinite(p_volumeScale) ? std::max(p_volumeScale, 0.0f) : 1.0f);
	settings.pan = m_pan;
	settings.pitch = m_pitch;
	settings.pitchVariation = m_pitchVariation;
	settings.bus = m_bus;
	settings.attenuation = m_attenuation;

	if (auto instance = m_engine.PlayOneShot(p_sound, settings); instance && instance->IsValid())
	{
		m_oneShots.push_back(std::move(instance));
	}
}

void OvAudio::Entities::AudioSource::Resume()
{
	if (HasSound())
	{
		m_instance->Play();
	}

	for (const auto& oneShot : m_oneShots)
	{
		if (oneShot->IsValid())
		{
			oneShot->Play();
		}
	}
}

void OvAudio::Entities::AudioSource::Pause()
{
	if (HasSound())
	{
		m_instance->Pause();
	}

	for (const auto& oneShot : m_oneShots)
	{
		if (oneShot->IsValid())
		{
			oneShot->Pause();
		}
	}
}

void OvAudio::Entities::AudioSource::Stop()
{
	if (HasSound())
	{
		m_instance->Stop();
	}

	for (const auto& oneShot : m_oneShots)
	{
		if (oneShot->IsValid())
		{
			oneShot->Stop();
		}
	}

	m_oneShots.clear();
}

void OvAudio::Entities::AudioSource::Update()
{
	std::erase_if(m_oneShots, [](const auto& p_oneShot) { return !p_oneShot->IsValid(); });

	// Its sounds in the world follow it, those played once included
	const auto& position = m_transform->GetWorldPosition();

	if (HasSound() && m_instance->IsSpatial())
	{
		m_instance->SetSpatialParameters(position, OvMaths::FVector3::Zero); // TODO: Add support for non-zero velocity
	}

	for (const auto& oneShot : m_oneShots)
	{
		oneShot->SetSpatialParameters(position, OvMaths::FVector3::Zero);
	}
}
