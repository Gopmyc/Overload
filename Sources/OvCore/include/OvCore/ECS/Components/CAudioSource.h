/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <OvAudio/Entities/AudioSource.h>
#include <OvTools/Eventing/Event.h>

#include "OvCore/ECS/Components/AComponent.h"

namespace OvCore::ECS { class Actor; }

namespace OvCore::ECS::Components
{
	/**
	* Represents an audio source. Its position in the world is important if the spatial sound settings is on
	*/
	class CAudioSource : public AComponent
	{
	public:
		/**
		* Constructor
		* @param p_owner
		*/
		CAudioSource(ECS::Actor& p_owner);

		/**
		* Returns the name of the component
		*/
		std::string GetName() override;

		/**
		* Returns the type name of the component
		*/
		virtual std::string GetTypeName() override;

		/**
		* Defines the sound to play on the audio source
		* @param p_sound
		*/
		void SetSound(OvAudio::Resources::Sound* p_sound);

		/**
		* Defines if the audio source should autoplay (Play sound on enable)
		*/
		void SetAutoplay(bool p_autoplay);

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
		void SetAttenuationModel(OvAudio::Settings::EAttenuationModel p_model);

		/**
		* Sets how fast its sound in the world fades (1 by default, not below 0)
		* @param p_rolloff
		*/
		void SetRolloff(float p_rolloff);

		/**
		* Sets the distance past which its sound in the world fades no more
		* @param p_distance
		*/
		void SetMaxDistance(float p_distance);

		/**
		* Sets the bus its sounds play on: empty, or "Master", for none
		* @param p_bus
		*/
		void SetBus(const std::string& p_bus);

		/**
		* Sets by how much the pitch varies at random, up or down, each time it plays
		* @param p_variation
		*/
		void SetPitchVariation(float p_variation);

		/**
		* Returns how its sound in the world fades with the distance
		*/
		OvAudio::Settings::EAttenuationModel GetAttenuationModel() const;

		/**
		* Returns how fast its sound in the world fades
		*/
		float GetRolloff() const;

		/**
		* Returns the distance past which its sound in the world fades no more
		*/
		float GetMaxDistance() const;

		/**
		* Returns the bus its sounds play on, empty for none
		*/
		const std::string& GetBus() const;

		/**
		* Returns by how much the pitch varies at random
		*/
		float GetPitchVariation() const;

		/**
		* Plays a sound once, beside its own (see OvAudio::Entities::AudioSource::PlayOneShot), if its actor is active
		* @param p_sound
		* @param p_volumeScale
		*/
		void PlayOneShot(OvAudio::Resources::Sound& p_sound, float p_volumeScale = 1.0f);

		/**
		* Returns the sound attached to the audio source
		*/
		OvAudio::Resources::Sound* GetSound() const;

		/**
		* Returns true if the audio source should call "Play" OnEnable
		*/
		bool IsAutoplayed() const;

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
		* Returns true if the audio source is currently playing
		*/
		bool IsPlaying() const;

		/**
		* Returns true if the audio source is spatialized
		*/
		bool IsSpatial() const;

		/**
		* Returns the audio source attenuation threshold (Minimum distance before applying sound attenuation)
		*/
		float GetAttenuationThreshold() const;

		/**
		* Play the audio source attached sound
		*/
		void Play();

		/**
		* Resume the audio source
		*/
		void Resume();

		/**
		* Pause the audio source
		*/
		void Pause();

		/**
		* Stop the audio source, and the sounds it played once
		*/
		void Stop();

		/**
		* Serialize the component
		* @param p_doc
		* @param p_node
		*/
		virtual void OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node) override;

		/**
		* Deserialize the component
		* @param p_doc
		* @param p_node
		*/
		virtual void OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node) override;

		/**
		* Defines how the component should be drawn in the inspector
		* @param p_root
		*/
		virtual void OnInspector(OvUI::Internal::WidgetContainer& p_root) override;

	private:
		virtual void OnEnable() override;
		virtual void OnDisable() override;

	private:
		OvAudio::Resources::Sound* m_sound = nullptr;
		OvAudio::Entities::AudioSource m_audioSource;
		bool m_autoPlay = false;
	};

	template<>
	struct ComponentTraits<OvCore::ECS::Components::CAudioSource>
	{
		static constexpr std::string_view Name = "class OvCore::ECS::Components::CAudioSource";
	};
}