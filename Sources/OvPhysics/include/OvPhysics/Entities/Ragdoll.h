/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <OvMaths/FQuaternion.h>
#include <OvMaths/FVector3.h>
#include <OvPhysics/Settings/RagdollPartSettings.h>

namespace JPH { class PhysicsSystem; }

namespace OvPhysics::Entities
{
	/**
	* A body made of capsules joined by swing-twist constraints, simulated from its creation to its destruction.
	* Its bodies carry no PhysicalObject: raycasts never see them and they raise no collision event
	*/
	class Ragdoll
	{
	public:
		/**
		* Creates the ragdoll and adds it to the simulation, every part moving at the given velocity
		* @param p_system
		* @param p_parts (a parent always comes before its children)
		* @param p_linearVelocity
		* @param p_objectLayer
		* @param p_groupID (unique to this ragdoll, its parts colliding with each other's through it)
		*/
		Ragdoll(
			JPH::PhysicsSystem& p_system,
			const std::vector<Settings::RagdollPartSettings>& p_parts,
			const OvMaths::FVector3& p_linearVelocity,
			uint16_t p_objectLayer,
			uint32_t p_groupID
		);

		/**
		* Removes the ragdoll from the simulation
		*/
		~Ragdoll();

		Ragdoll(const Ragdoll&) = delete;
		Ragdoll& operator=(const Ragdoll&) = delete;

		/**
		* Returns false when the parts could not be made into a ragdoll (none, a parent after its child, no body left)
		*/
		bool IsValid() const;

		/**
		* Returns the number of parts
		*/
		size_t GetPartCount() const;

		/**
		* Returns where a part's body stands (world space)
		* @param p_part
		*/
		OvMaths::FVector3 GetPartPosition(size_t p_part) const;

		/**
		* Returns how a part's body is turned (world space)
		* @param p_part
		*/
		OvMaths::FQuaternion GetPartRotation(size_t p_part) const;

		/**
		* Pushes a part, waking the ragdoll up
		* @param p_part
		* @param p_impulse
		*/
		void AddImpulse(size_t p_part, const OvMaths::FVector3& p_impulse);

	private:
		struct Implementation;
		std::unique_ptr<Implementation> m_implementation;
	};
}
