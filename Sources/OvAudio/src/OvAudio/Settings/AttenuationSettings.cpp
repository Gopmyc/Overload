/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>

#include <OvAudio/Settings/AttenuationSettings.h>

OvAudio::Settings::AttenuationSettings OvAudio::Settings::AttenuationSettings::Sanitized() const
{
	AttenuationSettings result = *this;

	result.minDistance = std::isfinite(minDistance) ? std::max(minDistance, kSmallestDistance) : 1.0f;
	result.maxDistance = std::isfinite(maxDistance) ? std::max(maxDistance, result.minDistance + kSmallestDistance) : result.minDistance + 500.0f;
	result.rolloff = std::isfinite(rolloff) ? std::max(rolloff, 0.0f) : 1.0f;

	if (result.model == EAttenuationModel::LINEAR_DISTANCE)
	{
		result.rolloff = std::min(result.rolloff, 1.0f);
	}

	return result;
}

float OvAudio::Settings::AttenuationSettings::Evaluate(float p_distance) const
{
	// SoLoud's formulas (soloud_core_3d.cpp), on the distance brought between the two bounds
	const AttenuationSettings settings = Sanitized();
	const float distance = std::clamp(p_distance, settings.minDistance, settings.maxDistance);

	switch (settings.model)
	{
	case EAttenuationModel::INVERSE_DISTANCE:
		return settings.minDistance / (settings.minDistance + settings.rolloff * (distance - settings.minDistance));
	case EAttenuationModel::LINEAR_DISTANCE:
		return 1.0f - settings.rolloff * (distance - settings.minDistance) / (settings.maxDistance - settings.minDistance);
	case EAttenuationModel::EXPONENTIAL_DISTANCE:
		return std::pow(distance / settings.minDistance, -settings.rolloff);
	default:
		return 1.0f;
	}
}
