/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <OvPhysics/Entities/CharacterController.h>

#include "OvCore/ECS/Components/CPhysicalObject.h"

namespace OvCore::ECS { class Actor; }

namespace OvCore::ECS::Components
{
	/**
	* A character moved by its velocity on Jolt's CharacterVirtual: it stands on slopes it can walk without sliding,
	* climbs steps, keeps to the ground going down and falls under gravity. Its capsule is upright and centred on its
	* actor, as a PhysicalCapsule's, and only its position follows the physics: the actor's rotation is its own
	*/
	class CCharacterController : public CPhysicalObject
	{
	public:
		using EGroundState = OvPhysics::Entities::CharacterController::EGroundState;

		/**
		* Constructor
		* @param p_owner
		*/
		CCharacterController(ECS::Actor& p_owner);

		/**
		* Returns the name of the component
		*/
		std::string GetName() override;

		/**
		* Returns the type name of the component
		*/
		virtual std::string GetTypeName() override;

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
		* Defines the highest step the character climbs (0 climbs none)
		* @param p_height
		*/
		void SetStepHeight(float p_height);

		/**
		* Returns the highest step the character climbs
		*/
		float GetStepHeight() const;

		/**
		* Defines how far down the character follows the ground it walks off (0 never does)
		* @param p_distance
		*/
		void SetGroundSnapDistance(float p_distance);

		/**
		* Returns how far down the character follows the ground it walks off
		*/
		float GetGroundSnapDistance() const;

		/**
		* Defines the share of the world's gravity the character falls with
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
		* Returns how fast the character really moved during the last physics step
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
		CPhysicalObject* GetGroundObject() const;

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
		OvPhysics::Entities::CharacterController& GetController() const;
	};

	template<>
	struct ComponentTraits<OvCore::ECS::Components::CCharacterController>
	{
		static constexpr std::string_view Name = "class OvCore::ECS::Components::CCharacterController";
	};
}
