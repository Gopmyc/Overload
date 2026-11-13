/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <numbers>

#include <OvCore/Animation/LookAtSolver.h>

namespace
{
	using OvMaths::FQuaternion;
	using OvMaths::FVector3;

	constexpr float kDegreesToRadians = std::numbers::pi_v<float> / 180.0f;
	constexpr float kMinDirection = 0.0001f;
	constexpr float kSettledAngle = 0.0005f;
	constexpr float kSettledSpeed = 0.05f;

	// The difference between two angles, the short way round
	float DeltaAngle(float p_from, float p_to)
	{
		constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
		float delta = std::fmod(p_to - p_from, kTwoPi);

		if (delta < 0.0f)
		{
			delta += kTwoPi;
		}

		return delta > std::numbers::pi_v<float> ? delta - kTwoPi : delta;
	}

	FQuaternion AngleAxis(const FVector3& p_axis, float p_angle)
	{
		const float half = p_angle * 0.5f;
		const float s = std::sin(half);
		return { p_axis.x * s, p_axis.y * s, p_axis.z * s, std::cos(half) };
	}
}

float OvCore::Animation::LookAtSolver::Damp(float p_current, float p_target, float& p_velocity, float p_smoothTime, float p_deltaTime, float p_maxSpeed)
{
	if (p_smoothTime <= 0.0f)
	{
		p_velocity = 0.0f;
		return p_target;
	}

	const float omega = 2.0f / p_smoothTime;
	const float x = omega * p_deltaTime;
	const float decay = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
	float change = p_current - p_target;

	if (p_maxSpeed > 0.0f)
	{
		const float maxChange = p_maxSpeed * p_smoothTime;
		change = std::clamp(change, -maxChange, maxChange);
	}

	const float spring = (p_velocity + omega * change) * p_deltaTime;
	const float output = p_current - change + (change + spring) * decay;

	// Never past the target
	if ((p_target - p_current > 0.0f) == (output > p_target))
	{
		p_velocity = 0.0f;
		return p_target;
	}

	p_velocity = (p_velocity - omega * spring) * decay;
	return output;
}

void OvCore::Animation::LookAtSolver::Step(float p_bodyYaw, const std::optional<FVector3>& p_target, float p_deltaTime, const LookAtSettings& p_settings)
{
	const float maxYaw = std::max(p_settings.maxYaw, 0.0f) * kDegreesToRadians;
	const float minPitch = std::min(p_settings.minPitch, p_settings.maxPitch) * kDegreesToRadians;
	const float maxPitch = std::max(p_settings.minPitch, p_settings.maxPitch) * kDegreesToRadians;
	const float maxSpeed = std::max(p_settings.maxTurnSpeed, 0.0f) * kDegreesToRadians;

	std::optional<float> targetYaw;
	std::optional<float> targetPitch;

	if (p_target)
	{
		const float flat = std::sqrt(p_target->x * p_target->x + p_target->z * p_target->z);

		if (flat > kMinDirection)
		{
			targetYaw = std::clamp(std::atan2(p_target->x, p_target->z), -maxYaw, maxYaw);
			targetPitch = std::clamp(std::atan2(p_target->y, flat), minPitch, maxPitch);
		}
		else if (std::abs(p_target->y) > kMinDirection)
		{
			// Straight above or below: the yaw stays, the pitch goes as far as it may
			targetYaw = std::clamp(m_yaw, -maxYaw, maxYaw);
			targetPitch = p_target->y > 0.0f ? maxPitch : minPitch;
		}
	}

	// Looking at something, the head holds that direction in the world while the body turns under it
	if (targetYaw && m_bodyYaw)
	{
		m_yaw -= DeltaAngle(*m_bodyYaw, p_bodyYaw);
	}

	m_bodyYaw = p_bodyYaw;

	if (p_deltaTime > 0.0f)
	{
		m_yaw = Damp(m_yaw, targetYaw.value_or(0.0f), m_yawVelocity, p_settings.turnTime, p_deltaTime, maxSpeed);
		m_pitch = Damp(m_pitch, targetPitch.value_or(0.0f), m_pitchVelocity, p_settings.turnTime, p_deltaTime, maxSpeed);
	}

	if (std::abs(m_yaw) > maxYaw)
	{
		m_yaw = std::clamp(m_yaw, -maxYaw, maxYaw);
		m_yawVelocity = 0.0f;
	}
}

void OvCore::Animation::LookAtSolver::Reset()
{
	m_yaw = m_pitch = m_yawVelocity = m_pitchVelocity = 0.0f;
	m_bodyYaw.reset();
}

float OvCore::Animation::LookAtSolver::GetYaw() const
{
	return m_yaw;
}

float OvCore::Animation::LookAtSolver::GetPitch() const
{
	return m_pitch;
}

bool OvCore::Animation::LookAtSolver::IsAtRest() const
{
	return
		std::abs(m_yaw) <= kSettledAngle &&
		std::abs(m_pitch) <= kSettledAngle &&
		std::abs(m_yawVelocity) <= kSettledSpeed &&
		std::abs(m_pitchVelocity) <= kSettledSpeed;
}

OvMaths::FQuaternion OvCore::Animation::LookAtSolver::Rotation(float p_yaw, float p_pitch)
{
	// A turn about +X by a positive angle brings +Z down: up is the opposite way
	return AngleAxis({ 0.0f, 1.0f, 0.0f }, p_yaw) * AngleAxis({ 1.0f, 0.0f, 0.0f }, -p_pitch);
}

void OvCore::Animation::LookAtSolver::Distribute(
	const std::vector<FQuaternion>& p_globals,
	const std::vector<FQuaternion>& p_locals,
	const std::vector<float>& p_parts,
	float p_yaw,
	float p_pitch,
	std::vector<FQuaternion>& p_result
)
{
	const size_t count = std::min({ p_globals.size(), p_locals.size(), p_parts.size() });
	p_result.assign(p_locals.begin(), p_locals.begin() + static_cast<std::ptrdiff_t>(count));

	float total = 0.0f;
	for (size_t i = 0; i < count; ++i)
	{
		total += std::max(p_parts[i], 0.0f);
	}

	if (total <= 0.0f)
	{
		return;
	}

	// Bone i's global becomes look(share so far) * its pose's global; its parent's, look(share before) * the pose's:
	// so its local is inverse(look(before) * parent) * look(so far) * global, the parent's global being the pose's
	// global times its local's inverse
	float before = 0.0f;
	FQuaternion lookBefore = FQuaternion::Identity;

	for (size_t i = 0; i < count; ++i)
	{
		const float soFar = before + std::max(p_parts[i], 0.0f) / total;
		const FQuaternion lookSoFar = Rotation(p_yaw * soFar, p_pitch * soFar);
		const FQuaternion parent = p_globals[i] * FQuaternion::Conjugate(FQuaternion::Normalize(p_locals[i]));

		p_result[i] = FQuaternion::Normalize(FQuaternion::Conjugate(lookBefore * parent) * (lookSoFar * p_globals[i]));

		before = soFar;
		lookBefore = lookSoFar;
	}
}
