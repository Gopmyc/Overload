/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <OvAudio/Data/SoundHandle.h>
#include <OvAudio/Entities/AudioSource.h>
#include <OvAudio/Entities/AudioListener.h>

namespace SoLoud
{
	class Soloud;
}

namespace OvAudio::Core
{
	/**
	* What the sounds are played on
	*/
	enum class EDriver
	{
		AUTO, // The system's audio device, else the null driver
		NULL_DRIVER // No device: the sounds are mixed and discarded, so that they play, end and loop as with one
	};

	/**
	* AudioEngine is the main class of the audio system.
	* It's responsible for initializing the audio backend and managing the audio sources and listeners.
	*/
	class AudioEngine
	{
	public:
		static constexpr uint32_t kSampleRate = 44100; // Of the null driver
		static constexpr uint32_t kChannels = 2;
		static constexpr uint32_t kMaxActiveVoices = 64;
		static constexpr float kMaxMixedSeconds = 0.25f; // The longest step a frame mixes on the null driver
		static constexpr uint32_t kMixChunkFrames = 2048; // The null driver's buffer: a mix goes by chunks of it

		/**
		* Constructor of the AudioEngine
		* @param p_driver (without a device, AUTO falls back to the null driver)
		*/
		AudioEngine(EDriver p_driver = EDriver::AUTO);

		/**
		* Destructor of the AudioEngine
		*/
		~AudioEngine();

		/**
		* Returns true if the AudioEngine is valid (properly initialized and available)
		*/
		bool IsValid() const;

		/**
		* Update AudioSources and AudioListeners. On the null driver, also mixes the frame's step (a quarter of a second
		* at most), so that the sounds keep step with the game: what a pause or an editor not playing leaves out of the
		* game's time is left out of theirs
		* @param p_deltaTime (the frame's step, in seconds, unscaled by the game's time scale)
		*/
		void Update(float p_deltaTime);

		/**
		* Suspend every sounds. Keeps every sound state (Pause and play) to Unsuspend them correctly
		*/
		void Suspend();

		/**
		* Suspend every sounds. Resume previously played sounds
		*/
		void Unsuspend();

		/**
		* Returns true if the AudioEngine is currently suspended
		*/
		bool IsSuspended() const;

		/**
		* Play a 2D sound in and return a SoundInstance if successful
		* @param p_sound
		* @param p_pan
		* @param p_volume
		* @param p_startPaused
		*/
		std::shared_ptr<OvAudio::Data::SoundInstance> Play2D(
			const Resources::Sound& p_sound,
			float p_pan,
			std::optional<float> p_volume = std::nullopt,
			bool p_startPaused = false
		);

		/**
		* Play a 3D (spatial) sound and return a SoundInstance if successful
		* @param p_sound
		* @param p_position
		* @param p_velocity
		* @param p_volume
		* @param p_startPaused
		*/
		std::shared_ptr<OvAudio::Data::SoundInstance> Play3D(
			const Resources::Sound& p_sound,
			const OvMaths::FVector3& p_position,
			const OvMaths::FVector3& p_velocity,
			std::optional<float> p_volume = std::nullopt,
			bool p_startPaused = false
		);

		/**
		* Returns the main listener
		* @param p_includeDisabled
		*/
		OvTools::Utils::OptRef<OvAudio::Entities::AudioListener> FindMainListener(bool p_includeDisabled = false) const;

		/**
		* Returns true if the sounds are played on the null driver: mixed by Update, then discarded
		*/
		bool IsNullDriver() const;

		/**
		* Returns what the sounds are played on: "device", "null", or "none" when the engine could not start
		*/
		std::string_view GetDriverName() const;

		/**
		* Returns the number of sounds playing or paused
		*/
		uint32_t GetVoiceCount() const;

		/**
		* Mixes the sounds playing into a buffer of interleaved stereo samples, as a device would play them, and moves
		* them on by as much time. Only on the null driver, for tests: returns false on a device
		* @param p_buffer (p_frames * kChannels samples)
		* @param p_frames
		*/
		bool Mix(float* p_buffer, uint32_t p_frames);

		/**
		* Sets whether the null driver mixes only through Mix, instead of each Update's step: for tests that move time on
		* themselves
		* @param p_manual
		*/
		void SetManualMixing(bool p_manual);

		/**
		* Returns a reference to the backend engine
		*/
		SoLoud::Soloud& GetBackend() const;

	private:
		void Consider(Entities::AudioSource& p_audioSource);
		void Consider(Entities::AudioListener& p_audioListener);

		void Unconsider(Entities::AudioSource& p_audioSource);
		void Unconsider(Entities::AudioListener& p_audioListener);

	private:
		bool m_suspended = false;
		bool m_nullDriver = false;
		bool m_manualMixing = false;
		double m_unmixedFrames = 0.0; // The part of a frame the steps have left to mix
		std::vector<float> m_mixBuffer;

		std::vector<std::reference_wrapper<Entities::AudioSource>> m_audioSources;
		std::vector<std::reference_wrapper<Entities::AudioSource>> m_suspendedAudioSources;
		std::vector<std::reference_wrapper<Entities::AudioListener>> m_audioListeners;
		std::optional<OvTools::Eventing::ListenerID> m_audioSourceCreatedListenerID;
		std::optional<OvTools::Eventing::ListenerID> m_audioSourceDestroyedListenerID;
		std::optional<OvTools::Eventing::ListenerID> m_audioListenerCreatedListenerID;
		std::optional<OvTools::Eventing::ListenerID> m_audioListenerDestroyedListenerID;

		std::unique_ptr<SoLoud::Soloud> m_backend;
	};
}
