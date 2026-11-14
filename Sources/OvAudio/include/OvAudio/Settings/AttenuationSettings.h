/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <OvAudio/Settings/EAttenuationModel.h>

namespace OvAudio::Settings
{
	/**
	* How a sound in the world fades with its distance to the listener
	*/
	struct AttenuationSettings
	{
		static constexpr float kSmallestDistance = 0.01f;

		EAttenuationModel model = EAttenuationModel::EXPONENTIAL_DISTANCE;

		/**
		* Within it, the sound is at its full volume (the source's attenuation threshold)
		*/
		float minDistance = 1.0f;

		/**
		* Beyond it, the sound fades no more: the linear model is silent there with a rolloff of 1, the others keep the
		* volume they reach
		*/
		float maxDistance = 500.0f;

		/**
		* How fast the sound fades: 1 by default, more for faster
		*/
		float rolloff = 1.0f;

		/**
		* Returns the settings as the audio engine uses them: the minimum distance at least kSmallestDistance, the maximum
		* above the minimum, the rolloff not below 0, and at most 1 for the linear model (which would turn negative past
		* its silence otherwise)
		*/
		AttenuationSettings Sanitized() const;

		/**
		* Returns the share of its volume a sound keeps at this distance, from 1 down, as the audio engine works it out
		* @param p_distance
		*/
		float Evaluate(float p_distance) const;
	};
}
