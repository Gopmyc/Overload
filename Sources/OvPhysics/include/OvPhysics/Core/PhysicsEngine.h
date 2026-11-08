/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <OvMaths/FQuaternion.h>
#include <OvPhysics/Entities/CharacterController.h>
#include <OvPhysics/Entities/PhysicalObject.h>
#include <OvPhysics/Entities/PhysicsHit.h>
#include <OvPhysics/Entities/Ragdoll.h>
#include <OvPhysics/Entities/RaycastHit.h>
#include <OvPhysics/Settings/PhysicsSettings.h>
#include <OvPhysics/Settings/QuerySettings.h>
#include <OvPhysics/Settings/RagdollPartSettings.h>

namespace JPH
{
	class BodyID;
	class BroadPhaseLayerInterfaceTable;
	class Factory;
	class JobSystem;
	class ObjectLayerPairFilterTable;
	class ObjectVsBroadPhaseLayerFilter;
	class PhysicsSystem;
	class TempAllocator;
}

namespace OvPhysics::Core
{
	/**
	* Main class of OvPhysics, it handles the creation of the physical world. It must be created
	* before any PhysicalObject to ensure PhysicalObject consideration
	*/
	class PhysicsEngine
	{
	public:
		/**
		* Creates the PhysicsEngine
		* @param p_settings
		*/
		PhysicsEngine(const Settings::PhysicsSettings& p_settings);

		/**
		* Destructor
		*/
		virtual ~PhysicsEngine();

		/**
		* Simulate the physics. This method call is decomposed in 3 things:
		* - Pre-Update (Apply FTransforms to physical bodies, called every Update call)
		* - Simulation (Move the character controllers, then simulate the bodies, called 60 times per seconds)
		* - Post-Update (Apply the simulation results, physical bodies transforms, to FTransforms)
		* This methods returns true if the call invoked a physics simulation
		*/
		bool Update(float p_deltaTime);

		/**
		* Moves the transforms of the interpolated objects between their last two physics poses, by the time gathered
		* toward the next step. Called once a frame, after the fixed update and before the update: the fixed update
		* sees the poses of the physics, the rest of the frame the interpolated ones
		*/
		void Interpolate();

		/* Casts a ray against all Physical Object in the Scene and returns information on what was hit
		 * @param p_origin
		 * @param p_end
		 */
		std::optional<Entities::RaycastHit> Raycast(OvMaths::FVector3 p_origin, OvMaths::FVector3 p_direction, float p_distance);

		/**
		* Casts a ray and returns the objects it meets, nearest first, each once, with the point, the normal and the
		* distance where the ray meets it. A ray that starts inside an object meets it at distance 0
		* @param p_origin
		* @param p_direction (need not be normalized)
		* @param p_distance
		* @param p_settings
		*/
		std::vector<Entities::PhysicsHit> CastRay(const OvMaths::FVector3& p_origin, const OvMaths::FVector3& p_direction, float p_distance, const Settings::QuerySettings& p_settings = {}) const;

		/**
		* Sweeps a sphere and returns the objects it meets, nearest first, each once. An object the sphere overlaps at
		* its origin is met at distance 0
		* @param p_origin
		* @param p_radius
		* @param p_direction (need not be normalized)
		* @param p_distance
		* @param p_settings
		*/
		std::vector<Entities::PhysicsHit> CastSphere(const OvMaths::FVector3& p_origin, float p_radius, const OvMaths::FVector3& p_direction, float p_distance, const Settings::QuerySettings& p_settings = {}) const;

		/**
		* Returns the objects that overlap a sphere, each once, in no particular order
		* @param p_center
		* @param p_radius
		* @param p_settings
		*/
		std::vector<Entities::PhysicalObject*> OverlapSphere(const OvMaths::FVector3& p_center, float p_radius, const Settings::QuerySettings& p_settings = {}) const;

		/**
		* Returns the objects that overlap a box, each once, in no particular order
		* @param p_center
		* @param p_halfExtents
		* @param p_rotation
		* @param p_settings
		*/
		std::vector<Entities::PhysicalObject*> OverlapBox(const OvMaths::FVector3& p_center, const OvMaths::FVector3& p_halfExtents, const OvMaths::FQuaternion& p_rotation, const Settings::QuerySettings& p_settings = {}) const;

		/**
		* Defines the world gravity to apply
		* @param p_gravity
		*/
		void SetGravity(const OvMaths::FVector3& p_gravity);

		/**
		* Returns the current world gravity
		*/
		OvMaths::FVector3 GetGravity() const;

		/**
		* Replaces the layer names, the collision matrix and the ragdoll rules with those of the settings (the gravity
		* is left as it is), from the next simulation step. The ragdolls already made keep their layer
		* @param p_settings
		*/
		void ApplyLayerSettings(const Settings::PhysicsSettings& p_settings);

		/**
		* Returns true if the bodies of the two layers collide
		* @param p_layer1
		* @param p_layer2
		*/
		bool DoLayersCollide(uint16_t p_layer1, uint16_t p_layer2) const;

		/**
		* Makes the bodies of the two layers collide or go through each other, from the next simulation step
		* @param p_layer1
		* @param p_layer2
		* @param p_collide
		*/
		void SetLayersCollide(uint16_t p_layer1, uint16_t p_layer2, bool p_collide);

		/**
		* Returns the index of the layer of the given name, if any
		* @param p_name
		*/
		std::optional<uint16_t> FindLayer(const std::string& p_name) const;

		/**
		* Returns the name of the layer (empty when it has none or when the index is out of range)
		* @param p_layer
		*/
		const std::string& GetLayerName(uint16_t p_layer) const;

		/**
		* Creates a ragdoll in the simulation, or returns nullptr when it cannot be made. Its parts are on the settings'
		* ragdoll layer, and raycasts never see them. When the settings ask it, they go through the dynamic bodies
		* @param p_parts (a parent always comes before its children)
		* @param p_linearVelocity
		*/
		std::unique_ptr<Entities::Ragdoll> CreateRagdoll(const std::vector<Settings::RagdollPartSettings>& p_parts, const OvMaths::FVector3& p_linearVelocity);

	private:
		void PreUpdate();
		void UpdateCharacters(float p_deltaTime);
		void PostUpdate();

		void ListenToPhysicalObjects();

		void Consider(OvPhysics::Entities::PhysicalObject& p_toConsider);
		void Unconsider(OvPhysics::Entities::PhysicalObject& p_toUnconsider);

		void ResetCollisionEvents();
		void CheckCollisionStopEvents();

		void CollisionCallback(const JPH::BodyID& p_body1, const JPH::BodyID& p_body2);

	private:
		class ContactListener;

		/* Jolt world */
		std::unique_ptr<JPH::Factory> m_factory;
		std::unique_ptr<JPH::TempAllocator> m_tempAllocator;
		std::unique_ptr<JPH::JobSystem> m_jobSystem;
		std::unique_ptr<JPH::BroadPhaseLayerInterfaceTable> m_broadPhaseLayerInterface;
		std::unique_ptr<JPH::ObjectLayerPairFilterTable> m_objectLayerPairFilter;
		std::unique_ptr<JPH::ObjectVsBroadPhaseLayerFilter> m_objectVsBroadPhaseLayerFilter;
		std::unique_ptr<ContactListener> m_contactListener;
		std::unique_ptr<JPH::PhysicsSystem> m_physicsSystem;
		float m_accumulatedTime = 0.0f;
		uint32_t m_ragdollCount = 0;
		OvTools::Eventing::ListenerID m_createdListener = 0;
		OvTools::Eventing::ListenerID m_destroyedListener = 0;
		Settings::PhysicsSettings m_settings;

		static std::map<std::pair<Entities::PhysicalObject*, Entities::PhysicalObject*>, bool> m_collisionEvents;
		std::vector<std::reference_wrapper<Entities::PhysicalObject>> m_physicalObjects;
		std::vector<Entities::CharacterController*> m_characters;
		std::vector<std::pair<JPH::BodyID, JPH::BodyID>> m_characterContacts;
	};
}
