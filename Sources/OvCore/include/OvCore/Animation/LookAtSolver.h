/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <optional>
#include <vector>

#include <OvMaths/FQuaternion.h>
#include <OvMaths/FVector3.h>

namespace OvCore::Animation
{
	/**
	* How far and how fast a look turns, in degrees and seconds
	*/
	struct LookAtSettings
	{
		float maxYaw = 70.0f;			// Either side of the body's front
		float minPitch = -30.0f;		// Down
		float maxPitch = 40.0f;			// Up
		float turnTime = 0.22f;			// Roughly how long a turn takes: the time of a critically damped spring
		float maxTurnSpeed = 0.0f;		// Degrees a second, 0 for no limit
	};

	/**
	* A look in a model's space, whose front is +Z and up +Y: a yaw about +Y, then a pitch up about the side axis, each
	* following its target on a critically damped spring and kept within its bounds. While it has a target, the look
	* holds its direction in the world as the body turns under it; without one, it goes back to the front.
	*/
	class LookAtSolver
	{
	public:
		/**
		* Moves the look on by a frame
		* @param p_bodyYaw (the world yaw of the model's front, radians: atan2(forward.x, forward.z))
		* @param p_target (the direction looked at, in the model's space, or nothing)
		* @param p_deltaTime
		* @param p_settings
		*/
		void Step(float p_bodyYaw, const std::optional<OvMaths::FVector3>& p_target, float p_deltaTime, const LookAtSettings& p_settings);

		/**
		* Puts the look back to the front, still
		*/
		void Reset();

		/**
		* Returns the yaw, radians, positive towards +X
		*/
		float GetYaw() const;

		/**
		* Returns the pitch, radians, positive up
		*/
		float GetPitch() const;

		/**
		* Returns true when the look is at the front and still
		*/
		bool IsAtRest() const;

		/**
		* Returns the rotation of a look in the model's space: the yaw about +Y after the pitch about +X
		* @param p_yaw (radians)
		* @param p_pitch (radians, positive up)
		*/
		static OvMaths::FQuaternion Rotation(float p_yaw, float p_pitch);

		/**
		* Shares a look out along a chain of bones, from the root to the tip, each turning by its part over its parent's:
		* the tip's global rotation becomes the full look times its pose's, each bone before it a share of the look by
		* the parts so far. Writes the bones' new local rotations
		* @param p_globals (the bones' global rotations in the pose, model space)
		* @param p_locals (the bones' local rotations in the pose)
		* @param p_parts (each bone's part of the look; they are taken over their sum)
		* @param p_yaw (radians)
		* @param p_pitch (radians)
		* @param p_result
		*/
		static void Distribute(
			const std::vector<OvMaths::FQuaternion>& p_globals,
			const std::vector<OvMaths::FQuaternion>& p_locals,
			const std::vector<float>& p_parts,
			float p_yaw,
			float p_pitch,
			std::vector<OvMaths::FQuaternion>& p_result
		);

		/**
		* A critically damped spring: moves a value towards a target over about p_smoothTime, its speed capped when
		* p_maxSpeed is above 0, and never past the target. Returns the new value and updates the speed
		* @param p_current
		* @param p_target
		* @param p_velocity
		* @param p_smoothTime
		* @param p_deltaTime
		* @param p_maxSpeed
		*/
		static float Damp(float p_current, float p_target, float& p_velocity, float p_smoothTime, float p_deltaTime, float p_maxSpeed);

	private:
		float m_yaw = 0.0f;
		float m_pitch = 0.0f;
		float m_yawVelocity = 0.0f;
		float m_pitchVelocity = 0.0f;
		std::optional<float> m_bodyYaw;
	};
}
