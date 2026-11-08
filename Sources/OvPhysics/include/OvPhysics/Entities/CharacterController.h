/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <utility>
#include <vector>

#include <OvPhysics/Entities/PhysicalObject.h>

namespace JPH
{
	class BodyID;
	class CharacterVirtual;
	class ObjectLayerPairFilter;
	class PhysicsSystem;
	class TempAllocator;
}

namespace OvPhysics::Entities
{
	/**
	* A character moved by its velocity rather than pushed by contacts, on Jolt's CharacterVirtual. It stands on any
	* ground no steeper than its maximum slope without sliding, climbs steps, keeps to the ground going down, and falls
	* under the world's gravity when nothing holds it. Its capsule is upright and centred on its transform, like a
	* PhysicalCapsule's, and stands for it to queries, triggers and rigid bodies
	*/
	class CharacterController : public PhysicalObject
	{
	public:
		/**
		* What holds the character
		*/
		enum class EGroundState
		{
			ON_GROUND,			// Ground it can stand on
			ON_STEEP_GROUND,	// Ground too steep to stand on: it slides down
			NOT_SUPPORTED,		// Something it touches but that cannot hold it
			IN_AIR				// Nothing
		};

		/**
		* Creates a character controller
		* @param p_transform
		* @param p_radius
		* @param p_height (the straight part of the capsule, as for a PhysicalCapsule)
		*/
		CharacterController(OvMaths::FTransform& p_transform, float p_radius = 0.5f, float p_height = 1.0f);

		/**
		* Destructor
		*/
		~CharacterController() override;

		/**
		* Defines the radius of the capsule
		* @param p_radius
		*/
		void SetRadius(float p_radius);

		/**
		* Defines the height of the straight part of the capsule
		* @param p_height
		*/
		void SetHeight(float p_height);

		/**
		* Returns the radius of the capsule
		*/
		float GetRadius() const;

		/**
		* Returns the height of the straight part of the capsule
		*/
		float GetHeight() const;

		/**
		* Defines the steepest ground the character stands on, in degrees (0 to 90)
		* @param p_degrees
		*/
		void SetMaxSlopeAngle(float p_degrees);

		/**
		* Returns the steepest ground the character stands on, in degrees
		*/
		float GetMaxSlopeAngle() const;

		/**
		* Defines the highest step the character climbs while it walks (0 climbs none)
		* @param p_height
		*/
		void SetStepHeight(float p_height);

		/**
		* Returns the highest step the character climbs
		*/
		float GetStepHeight() const;

		/**
		* Defines how far down the character follows the ground it walks off, instead of leaving it (0 never does)
		* @param p_distance
		*/
		void SetGroundSnapDistance(float p_distance);

		/**
		* Returns how far down the character follows the ground it walks off
		*/
		float GetGroundSnapDistance() const;

		/**
		* Defines the share of the world's gravity the character falls with (0 floats, 1 falls as a body does)
		* @param p_factor
		*/
		void SetGravityFactor(float p_factor);

		/**
		* Returns the share of the world's gravity the character falls with
		*/
		float GetGravityFactor() const;

		/**
		* Defines the strongest force the character pushes dynamic bodies with, in newtons
		* @param p_strength
		*/
		void SetPushStrength(float p_strength);

		/**
		* Returns the strongest force the character pushes dynamic bodies with, in newtons
		*/
		float GetPushStrength() const;

		/**
		* Adds a force, applied over the next physics step
		* @param p_force
		*/
		void AddForce(const OvMaths::FVector3& p_force) override;

		/**
		* Adds an impulse: the velocity changes at once by the impulse over the mass
		* @param p_impulse
		*/
		void AddImpulse(const OvMaths::FVector3& p_impulse) override;

		/**
		* Clears the forces not applied yet
		*/
		void ClearForces() override;

		/**
		* Returns the velocity the character moves at. Its horizontal part is the one it was given: walls never change
		* it, GetRealVelocity says how fast the character really goes. Its vertical part is the controller's: the
		* ground's while the character stands, growing with gravity while it falls; a ceiling ends a rise, and
		* something that holds the character up ends a fall
		*/
		OvMaths::FVector3 GetLinearVelocity() const override;

		/**
		* Defines the velocity the character moves at from the next physics step
		* @param p_linearVelocity
		*/
		void SetLinearVelocity(const OvMaths::FVector3& p_linearVelocity) override;

		/**
		* Returns how fast the character really moved during the last physics step, walls, steps and the ground it
		* stands on included
		*/
		OvMaths::FVector3 GetRealVelocity() const;

		/**
		* Returns what holds the character since the last physics step
		*/
		EGroundState GetGroundState() const;

		/**
		* Returns true if the character stands on ground no steeper than its maximum slope
		*/
		bool IsGrounded() const;

		/**
		* Returns the normal of the ground the character touches, (0, 0, 0) in the air
		*/
		OvMaths::FVector3 GetGroundNormal() const;

		/**
		* Returns the point where the character touches the ground
		*/
		OvMaths::FVector3 GetGroundPoint() const;

		/**
		* Returns the velocity of the ground the character touches
		*/
		OvMaths::FVector3 GetGroundVelocity() const;

		/**
		* Returns the physical object the character touches the ground on, nullptr in the air or on a ragdoll
		*/
		PhysicalObject* GetGroundObject() const;

		/**
		* Returns true
		*/
		bool IsCharacter() const override;

	protected:
		JPH::Shape* CreateShape(const OvMaths::FVector3& p_scale) const override;
		void UpdateBodyTransform() override;
		void UpdateFTransform() override;

	private:
		friend class OvPhysics::Core::PhysicsEngine;

		/**
		* Creates the character in the physics system (called by the physics engine when it considers the object)
		* @param p_physicsSystem
		*/
		void Attach(JPH::PhysicsSystem& p_physicsSystem);

		/**
		* Moves the character by its velocity over a physics step, then its body to where it went, before the bodies
		* are simulated. The bodies it touches are added to the contacts, the character's body first
		* @param p_deltaTime
		* @param p_layerPairFilter
		* @param p_ignoreRagdolls
		* @param p_allocator
		* @param p_contacts
		*/
		void Step(float p_deltaTime, const JPH::ObjectLayerPairFilter& p_layerPairFilter, bool p_ignoreRagdolls, JPH::TempAllocator& p_allocator, std::vector<std::pair<JPH::BodyID, JPH::BodyID>>& p_contacts);

		float GetHalfHeight() const;
		void ApplySettings();

	private:
		float m_radius;
		float m_height;
		float m_maxSlopeAngle = 50.0f;
		float m_stepHeight = 0.3f;
		float m_groundSnapDistance = 0.3f;
		float m_gravityFactor = 1.0f;
		float m_pushStrength = 100.0f;

		OvMaths::FVector3 m_velocity = OvMaths::FVector3::Zero;
		OvMaths::FVector3 m_realVelocity = OvMaths::FVector3::Zero;
		OvMaths::FVector3 m_force = OvMaths::FVector3::Zero;

		JPH::PhysicsSystem* m_physicsSystem = nullptr;
		JPH::CharacterVirtual* m_character = nullptr;
		bool m_shapeChanged = false;
		bool m_teleported = false;
	};
}
