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
	* How a group of chains hangs and swings
	*/
	struct SpringSettings
	{
		float stiffness = 150.0f;	// How hard a joint is drawn back to its rest, 1/s²
		float damping = 8.0f;		// How fast its swing dies, 1/s
		float gravity = 0.3f;		// A share of 9.81 m/s², what the rest pose already bears of it left out
		float radius = 0.025f;		// A joint's thickness against the colliders and the ground, metres
		float maxAngle = 45.0f;		// The most a segment strays from its rest, degrees
		float link = 0.0f;			// How firmly neighbouring chains keep their spacing, 0 to 1
	};

	/**
	* A capsule the joints stay out of, in world space: its axis at the start of the frame and at its end
	*/
	struct SpringCapsule
	{
		OvMaths::FVector3 start0;
		OvMaths::FVector3 end0;
		OvMaths::FVector3 start1;
		OvMaths::FVector3 end1;
		float radius = 0.0f;
	};

	/**
	* One chain of bones, each a segment from its joint to the next
	*/
	struct SpringChain
	{
		// Set up from the rest, model units
		std::vector<float> lengths;					// Each bone's length (the last one past its joint)
		std::vector<OvMaths::FQuaternion> binds;	// Each bone's rest rotation relative to its parent
		std::vector<OvMaths::FVector3> axes;		// Each bone's direction in its own frame, towards the next joint

		// Where it hangs from: its first joint and the rotation of what it hangs from, at the frame's start and end
		OvMaths::FVector3 anchor0;
		OvMaths::FVector3 anchor1;
		OvMaths::FQuaternion parent0;
		OvMaths::FQuaternion parent1;

		// Simulated, world space: each bone's end, where it was a sub-step before, and its rotation
		std::vector<OvMaths::FVector3> positions;
		std::vector<OvMaths::FVector3> previous;
		std::vector<OvMaths::FQuaternion> rotations;

		// The spacing at rest between each joint and the same joint of the next chain of the group, model units
		std::vector<float> linkRest;

		// The body's own move over the current sub-step
		OvMaths::FVector3 bodyMove;
	};

	/**
	* A group of chains on one setting, side by side (a cape's columns), and the colliders it meets
	*/
	struct SpringGroup
	{
		SpringSettings settings;
		std::vector<SpringChain> chains;
		float previousStep = 0.0f;
	};

	/**
	* Hair and cloth hanging from a body as chains of joints, in world space, so that the body's turns and starts swing
	* them. Each frame is cut into equal sub-steps (at most 8, one per 1/120 s) over which the anchors, the parents'
	* rotations and the colliders move straight from where they were to where they are. Each joint is a Verlet particle:
	* - carried on by its own speed, but slowed by the damping towards the speed of the body it hangs from, not towards
	*   stillness: walking on, it hangs as at rest; starting off, it lags, and stopping, it swings on;
	* - drawn back towards its rest, local to its segment (the parent segment's rotation as simulated, times the bone's
	*   rest rotation), by the stiffness;
	* - weighed by the gravity, less what its rest already bears: the model is drawn hanging as it hangs, so only the
	*   pull along the rest direction is left, which holds a hanging chain to its rest and tips a standing one off it;
	* - kept at its segment's length from the joint above, out of the capsules by the radius;
	* - never more than the maximum angle off its rest, and never under the ground, which wins;
	* - held at its spacing from the same joint of the next chain by the link (0 to 1), a cape so staying one cloth.
	*/
	class SpringSolver
	{
	public:
		/**
		* Sets where a chain hangs from this frame: its first joint, and the rotation of what it hangs from (world)
		* @param p_chain
		* @param p_anchor
		* @param p_parentRotation
		*/
		static void SetAnchor(SpringChain& p_chain, const OvMaths::FVector3& p_anchor, const OvMaths::FQuaternion& p_parentRotation);

		/**
		* Returns how far a chain's anchor moved since the last frame
		* @param p_chain
		*/
		static float GetAnchorMove(const SpringChain& p_chain);

		/**
		* Puts a chain at rest from its current anchor, still: a first frame, a wake, a teleport
		* @param p_chain
		* @param p_scale (the body's)
		*/
		static void Reset(SpringChain& p_chain, float p_scale);

		/**
		* Measures the spacing at rest between neighbouring chains of a group (after Reset)
		* @param p_group
		* @param p_scale
		*/
		static void MeasureLinks(SpringGroup& p_group, float p_scale);

		/**
		* Moves a group on by a frame
		* @param p_group
		* @param p_capsules
		* @param p_ground (the height of the ground, or nothing)
		* @param p_scale (the body's)
		* @param p_deltaTime
		*/
		static void Step(SpringGroup& p_group, const std::vector<SpringCapsule>& p_capsules, std::optional<float> p_ground, float p_scale, float p_deltaTime);

		/**
		* Returns a chain's bone rotation relative to its parent
		* @param p_chain
		* @param p_index
		*/
		static OvMaths::FQuaternion GetLocal(const SpringChain& p_chain, size_t p_index);
	};
}
