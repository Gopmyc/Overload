/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <OvAudio/Data/SoundHandle.h>
#include <OvAudio/Data/SoundInstance.h>
#include <OvAudio/Resources/Sound.h>
#include <OvMaths/FVector3.h>
#include <OvMaths/FTransform.h>
#include <OvTools/Eventing/Event.h>
#include <OvTools/Utils/OptRef.h>
#include <OvTools/Utils/ReferenceOrValue.h>

namespace OvAudio::Core { class AudioEngine; }

namespace OvAudio::Entities
{
	/**
	* An audio source is an entity that can play a sound in a 3D space.
	*/
	class AudioSource
	{
	public:
		/**
		* AudioSource constructor (Internal transform management)
		* @param p_engine
		* @param p_transform
		*/
		AudioSource(
			Core::AudioEngine& p_engine,
			OvTools::Utils::OptRef<OvMaths::FTransform> p_transform = std::nullopt
		);

		/**
		* AudioSource destructor
		*/
		~AudioSource();

		/**
		* Returns the AudioSource transform
		*/
		const OvMaths::FTransform& GetTransform();

		/**
		* Apply every AudioSource settings to the currently tracked sound
		*/
		void ApplySourceSettingsToTrackedSound();

		/**
		* Returns true if the audio source has a sound instance
		*/
		bool HasSound() const;

		/**
		* Returns true if the audio source is currently playing
		*/
		bool IsPlaying() const;

		/**
		* Retrurns true if the audio source is currently paused
		*/
		bool IsPaused() const;

		/**
		* Returns the currently tracked sound if any, or nullptr
		*/
		std::weak_ptr<Data::SoundInstance> GetSoundInstance() const;

		/**
		* Defines the audio source volume
		* @param p_volume
		*/
		void SetVolume(float p_volume);

		/**
		* Defines the audio source pan (Left / Right)
		* @param p_pan
		*/
		void SetPan(float p_pan);

		/**
		* Defines if the audio source should loop
		* @param p_looped
		*/
		void SetLooped(bool p_looped);

		/**
		* Defines the audio source pitch (= frequency or playback speed)
		* @param p_pitch
		*/
		void SetPitch(float p_pitch);

		/**
		* Defines if the audio source should be spatialized or not
		* @param p_value
		*/
		void SetSpatial(bool p_value);

		/**
		* Defines the audio source attenuation threshold (Minimum distance before applying sound attenuation)
		* @param p_distance
		*/
		void SetAttenuationThreshold(float p_distance);

		/**
		* Sets how its sound in the world fades with the distance
		* @param p_model
		*/
		void SetAttenuationModel(Settings::EAttenuationModel p_model);

		/**
		* Sets how fast its sound in the world fades: 1 by default, more for faster (not below 0)
		* @param p_rolloff
		*/
		void SetRolloff(float p_rolloff);

		/**
		* Sets the distance past which its sound in the world fades no more
		* @param p_distance
		*/
		void SetMaxDistance(float p_distance);

		/**
		* Sets the bus its sounds play on, by name: empty, or "Master", for none. A sound playing moves to it, where it
		* was. A bus the project lacks plays them on the Master, with a warning
		* @param p_bus
		*/
		void SetBus(const std::string& p_bus);

		/**
		* Sets by how much the pitch varies at random, up or down, each time it plays a sound (not below 0)
		* @param p_variation
		*/
		void SetPitchVariation(float p_variation);

		/**
		* Returns how its sound in the world fades with the distance
		*/
		Settings::EAttenuationModel GetAttenuationModel() const;

		/**
		* Returns how fast its sound in the world fades
		*/
		float GetRolloff() const;

		/**
		* Returns the distance past which its sound in the world fades no more
		*/
		float GetMaxDistance() const;

		/**
		* Returns all its attenuation settings
		*/
		const Settings::AttenuationSettings& GetAttenuation() const;

		/**
		* Returns the bus its sounds play on, empty for none (the Master)
		*/
		const std::string& GetBus() const;

		/**
		* Returns by how much the pitch varies at random
		*/
		float GetPitchVariation() const;

		/**
		* Returns the audio source volume
		*/
		float GetVolume() const;

		/**
		* Returns the audio source pan (Left / Right)
		*/
		float GetPan() const;

		/**
		* Returns if the audio source is looping
		*/
		bool IsLooped() const;

		/**
		* Returns the audio source pitch (= frequency or playback speed)
		*/
		float GetPitch() const;

		/**
		* Returns true if the audio source is spatialized
		*/
		bool IsSpatial() const;

		/**
		* Returns the audio source attenuation threshold (Minimum distance before applying sound attenuation)
		*/
		float GetAttenuationThreshold() const;

		/**
		* Play the given sound
		* @param p_sound
		*/
		void Play(const Resources::Sound& p_sound);

		/**
		* Plays a sound once, beside its own sound and the others played so: from its place, which it follows, on its
		* bus, with its attenuation, its pitch and its variation. It ends by itself
		* @param p_sound
		* @param p_volumeScale (of its volume)
		*/
		void PlayOneShot(const Resources::Sound& p_sound, float p_volumeScale = 1.0f);

		/**
		* Resume the audio source, and the sounds it played once
		*/
		void Resume();

		/**
		* Pause the audio source, and the sounds it played once
		*/
		void Pause();

		/**
		* Stop the audio source, and the sounds it played once
		*/
		void Stop();

		/**
		* Update the audio source
		*/
		void Update();

	public:
		static OvTools::Eventing::Event<AudioSource&> CreatedEvent;
		static OvTools::Eventing::Event<AudioSource&> DestroyedEvent;

	private:
		void Replay();
		void ApplyAttenuation();

	private:
		Core::AudioEngine& m_engine;
		OvTools::Utils::ReferenceOrValue<OvMaths::FTransform> m_transform;
		std::shared_ptr<Data::SoundInstance> m_instance;
		const Resources::Sound* m_sound = nullptr; // What m_instance plays, to play it again when the mode changes

		// Sound settings
		bool m_spatial = false;
		float m_volume = 1.0f;
		float m_pan = 0.0f;
		bool m_looped = false;
		float m_pitch = 1.0f;
		float m_pitchVariation = 0.0f;
		std::string m_bus; // Empty for the Master
		Settings::AttenuationSettings m_attenuation;
		std::vector<std::shared_ptr<Data::SoundInstance>> m_oneShots;
	};
}