/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include <OvAudio/Data/SoundHandle.h>
#include <OvAudio/Settings/AttenuationSettings.h>
#include <OvAudio/Entities/AudioSource.h>
#include <OvAudio/Entities/AudioListener.h>

namespace SoLoud
{
	class Bus;
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
	* How a sound played once, without a source, is played (see AudioEngine::PlayOneShot)
	*/
	struct OneShotSettings
	{
		/**
		* In the world at this position when set, flat otherwise
		*/
		std::optional<OvMaths::FVector3> position;

		float volume = 1.0f;
		float pan = 0.0f; // For a flat sound
		float pitch = 1.0f;

		/**
		* The pitch is drawn between pitch - pitchVariation and pitch + pitchVariation
		*/
		float pitchVariation = 0.0f;

		/**
		* A bus of the project, or empty for the Master
		*/
		std::string bus;

		Settings::AttenuationSettings attenuation;
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
		static constexpr uint32_t kMaxBuses = 8;
		static constexpr std::string_view kMasterBus = "Master";
		static constexpr float kMinPitch = 0.01f;

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
		* Pauses every sound playing, the sounds played once included. Unsuspend resumes them, and only them
		*/
		void Suspend();

		/**
		* Resumes what Suspend paused: a sound the game paused itself stays paused
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
			bool p_startPaused = false,
			std::string_view p_bus = {}
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
			bool p_startPaused = false,
			std::string_view p_bus = {}
		);

		/**
		* Plays a sound once, beside the others, with no source to hold it: it ends by itself. A sound in the world
		* that cannot be heard (under a thousandth of its volume) ends at once, rather than waiting to be heard. Returns its
		* instance if it plays
		* @param p_sound
		* @param p_settings
		*/
		std::shared_ptr<OvAudio::Data::SoundInstance> PlayOneShot(const Resources::Sound& p_sound, const OneShotSettings& p_settings);

		/**
		* Stops every sound played once (when a game in the editor stops)
		*/
		void StopOneShots();

		/**
		* Sets the project's buses: each plays into the Master, and a sound played on one is heard at its volume. The
		* buses past kMaxBuses are left out. Names that change set every bus back to a volume of 1; the same names
		* change nothing. A sound playing on a bus whose name goes keeps playing there
		* @param p_names
		*/
		void SetBuses(const std::vector<std::string>& p_names);

		/**
		* Returns the names of the project's buses, the Master left out
		*/
		const std::vector<std::string>& GetBusNames() const;

		/**
		* Returns true for a bus of the project, and for the Master
		* @param p_bus
		*/
		bool HasBus(std::string_view p_bus) const;

		/**
		* Sets the volume of a bus, or of everything for the Master (not below 0). Returns false for no such bus
		* @param p_bus
		* @param p_volume
		*/
		bool SetVolume(std::string_view p_bus, float p_volume);

		/**
		* Returns the volume of a bus, or of the Master, if there is such a bus
		* @param p_bus
		*/
		std::optional<float> GetVolume(std::string_view p_bus) const;

		/**
		* Sets every bus, and the Master, back to a volume of 1 (when a game in the editor stops)
		*/
		void ResetVolumes();

		/**
		* Returns a pitch drawn between p_pitch - p_variation and p_pitch + p_variation, never below kMinPitch
		* @param p_pitch
		* @param p_variation
		*/
		float VaryPitch(float p_pitch, float p_variation);

		/**
		* Seeds the draws of VaryPitch, for tests that repeat them
		* @param p_seed
		*/
		void SetRandomSeed(uint32_t p_seed);

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
		* Returns the number of sounds playing or paused, the buses left out
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
		SoLoud::Bus* FindBus(std::string_view p_bus);
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
		std::vector<Data::SoundHandle> m_suspendedSounds; // What Suspend paused
		std::vector<std::shared_ptr<Data::SoundInstance>> m_oneShots;
		std::vector<std::unique_ptr<SoLoud::Bus>> m_buses; // Kept for the whole run, so that no sound is left on a lost bus
		std::vector<Data::SoundHandle> m_busVoices;
		std::vector<std::string> m_busNames;
		std::unordered_set<std::string> m_warnedBuses;
		std::mt19937 m_random{ std::random_device{}() };
		std::vector<std::reference_wrapper<Entities::AudioListener>> m_audioListeners;
		std::optional<OvTools::Eventing::ListenerID> m_audioSourceCreatedListenerID;
		std::optional<OvTools::Eventing::ListenerID> m_audioSourceDestroyedListenerID;
		std::optional<OvTools::Eventing::ListenerID> m_audioListenerCreatedListenerID;
		std::optional<OvTools::Eventing::ListenerID> m_audioListenerDestroyedListenerID;

		std::unique_ptr<SoLoud::Soloud> m_backend;
	};
}
