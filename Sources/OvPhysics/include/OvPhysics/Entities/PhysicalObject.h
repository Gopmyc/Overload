/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <any>
#include <cstdint>
#include <memory>

#include <OvMaths/FTransform.h>
#include <OvTools/Eventing/Event.h>
#include <OvPhysics/Settings/BodySettings.h>

namespace OvPhysics::Core { class PhysicsEngine; }

namespace JPH
{
	class Body;
	class BodyInterface;
	class Shape;
}

namespace OvPhysics::Entities
{
	/**
	* Base class for any entity that is physical
	*/
	class PhysicalObject
	{
	public:
		friend class OvPhysics::Core::PhysicsEngine;

		/**
		* Defines some collision detection modes that a physical object can have
		*/
		enum class ECollisionDetectionMode
		{
			DISCRETE,
			CONTINUOUS
		};

		/**
		* PhysicalObject constructor (Internal transform management)
		*/
		PhysicalObject();

		/**
		* PhysicalObject constructor (External transform management)
		* @param p_shapeType
		* @param p_transform
		*/
		PhysicalObject(OvMaths::FTransform& p_transform);

		/**
		* PhysicalObject destructor (Free memory if the transform is internally managed)
		*/
		virtual ~PhysicalObject();

		/**
		* Add a force to the physical object
		* @param p_force
		*/
		virtual void AddForce(const OvMaths::FVector3& p_force);

		/**
		* Add an impulse to the physical object
		* @param p_impulse
		*/
		virtual void AddImpulse(const OvMaths::FVector3& p_impulse);

		/**
		* Clear forces
		*/
		virtual void ClearForces();

		/**
		* Returns the mass of the physical object
		*/
		float GetMass() const;

		/**
		* Returns the collision detection mode of the physical object
		*/
		const ECollisionDetectionMode& GetCollisionDetectionMode() const;

		/**
		* Returns the bounciness of the physical object
		*/
		float GetBounciness() const;

		/**
		* Returns the friction of the physical object
		*/
		float GetFriction()	const;

		/**
		* Returns the linear velocity of the physical object
		*/
		virtual OvMaths::FVector3 GetLinearVelocity() const;

		/**
		* Returns the angular of the physical object
		*/
		OvMaths::FVector3 GetAngularVelocity() const;

		/**
		* Returns the linear factor of the physical object
		*/
		OvMaths::FVector3 GetLinearFactor()	const;

		/**
		* Returns the angular factor of the physical object
		*/
		OvMaths::FVector3 GetAngularFactor() const;

		/**
		* Returns true if the physical object is a trigger
		*/
		bool IsTrigger() const;

		/**
		* Returns true if the physical object is kinematic
		*/
		bool IsKinematic() const;

		/**
		* Returns true if the physical object is static: it never moves by itself, never sleeps nor wakes up, and its
		* body is only moved when its transform changes
		*/
		bool IsStatic() const;

		/**
		* Returns true if the physical object is a character controller: its body is kinematic, never static nor a
		* trigger, and the controller moves it
		*/
		virtual bool IsCharacter() const;

		/**
		* Defines if the transform of the physical object is interpolated between its last two physics steps, so that
		* it moves smoothly at any frame rate. Only a body the physics moves (a dynamic body, a character controller)
		* is interpolated
		* @param p_interpolated
		*/
		void SetInterpolated(bool p_interpolated);

		/**
		* Returns true if the transform of the physical object is interpolated between its last two physics steps
		*/
		bool IsInterpolated() const;

		/**
		* Returns a reference to the transform of the physical object
		*/
		OvMaths::FTransform& GetTransform();

		/**
		* Defines a new mass for the physical object
		* @param p_mass
		*/
		void SetMass(float p_mass);

		/**
		* Defines a new collision detection mode for the physical object
		* @param p_mode
		*/
		void SetCollisionDetectionMode(ECollisionDetectionMode p_mode);

		/**
		* Defines a new bounciness for the physical object
		* @param p_bounciness
		*/
		void SetBounciness(float p_bounciness);

		/**
		* Defines a new friction for the physical object
		* @param p_mass
		*/
		void SetFriction(float p_friction);

		/**
		* Defines a new linear velocity for the physical object
		* @param p_linearVelocity
		*/
		virtual void SetLinearVelocity(const OvMaths::FVector3& p_linearVelocity);

		/**
		* Defines a new angular velocity for the physical object
		* @param p_angularVelocity
		*/
		void SetAngularVelocity(const OvMaths::FVector3& p_angularVelocity);

		/**
		* Defines a new linear factor for the physical object
		* @param p_linearFactor
		*/
		void SetLinearFactor(const OvMaths::FVector3& p_linearFactor);

		/**
		* Defines a new angular factor for the physical object
		* @param p_angularFactor
		*/
		void SetAngularFactor(const OvMaths::FVector3& p_angularFactor);

		/**
		* Defines if the physical object should be a trigger or not
		* @param p_trigger
		*/
		void SetTrigger(bool p_trigger);

		/**
		* Defines if the physical object should be kinematic or not
		* @param p_kinematic
		*/
		void SetKinematic(bool p_kinematic);

		/**
		* Defines if the physical object should be static or not (a static object ignores the kinematic setting)
		* @param p_static
		*/
		void SetStatic(bool p_static);

		/**
		* Puts the physical object on a collision layer (0 to PhysicsSettings::kLayerCount - 1): which layers collide
		* is set by the physics engine's settings. Out of range, the layer is clamped
		* @param p_layer
		*/
		void SetLayer(uint16_t p_layer);

		/**
		* Returns the collision layer of the physical object
		*/
		uint16_t GetLayer() const;

		/**
		* Defines if the physical object should be enabled or not
		* @param p_enabled
		*/
		void SetEnabled(bool p_enabled);

		/**
		* Returns true if the physical object is enabled
		*/
		bool IsEnabled() const;

		/**
		* Returns the user data associated to this physical object instance
		*/
		template <typename T> T		GetUserData() const		{ return std::any_cast<T>(m_userData); }

		/**
		* Defines the user data associated to this physical object instance
		* @param p_data
		*/
		template <typename T> void	SetUserData(T p_data)	{ m_userData = p_data; }

	protected:
		void Init();
		void RecreateBody();

		/**
		* Creates the collision shape of the physical object
		* @param p_scale
		* @return A new shape, owned by the body it is given to
		*/
		virtual JPH::Shape* CreateShape(const OvMaths::FVector3& p_scale) const = 0;

		/**
		* Returns true if the shape of the physical object can only be used by a static body
		*/
		virtual bool MustBeStatic() const;

		void Consider();
		void Unconsider();

		/* Needed by the physics engine */
		virtual void UpdateBodyTransform();
		virtual void UpdateFTransform();

		/**
		* Moves the transform between the last two physics poses, when it is interpolated and nothing else moved it
		* since the physics did
		* @param p_alpha (0 at the previous pose, 1 at the last one)
		*/
		virtual void ApplyInterpolation(float p_alpha);

		/**
		* Keeps the pose the physics gave at the end of a step, and the one before
		* @param p_position
		* @param p_rotation
		*/
		void RecordPhysicsPose(const OvMaths::FVector3& p_position, const OvMaths::FQuaternion& p_rotation);

		/**
		* Forgets the poses kept: the object starts again from where it is, without moving between two poses
		* @param p_position
		* @param p_rotation
		*/
		void ResetPhysicsPose(const OvMaths::FVector3& p_position, const OvMaths::FQuaternion& p_rotation);

	private:
		/* Internal */
		void CreateBody(const Settings::BodySettings& p_bodySettings);
		Settings::BodySettings DestroyBody();

	public:
		OvTools::Eventing::Event<PhysicalObject&>			CollisionStartEvent;
		OvTools::Eventing::Event<PhysicalObject&>			CollisionStayEvent;
		OvTools::Eventing::Event<PhysicalObject&>			CollisionStopEvent;

		OvTools::Eventing::Event<PhysicalObject&>			TriggerStartEvent;
		OvTools::Eventing::Event<PhysicalObject&>			TriggerStayEvent;
		OvTools::Eventing::Event<PhysicalObject&>			TriggerStopEvent;

	protected:
		/* Transform stuff */
		OvMaths::FTransform* const		m_transform;
		const bool						m_internalTransform;

		/* Settings */
		float					m_mass = 1.f;
		bool					m_kinematic = false;
		bool					m_static = false;
		uint16_t				m_layer = 0;
		bool					m_trigger = false;
		bool					m_enabled = true;
		bool					m_considered = false;
		bool					m_interpolated = false;
		ECollisionDetectionMode m_collisionMode = ECollisionDetectionMode::DISCRETE;
		OvMaths::FVector3		m_linearFactor = OvMaths::FVector3::One;
		OvMaths::FVector3		m_angularFactor = OvMaths::FVector3::One;

		/* Other */
		std::any m_userData;
		OvMaths::FVector3 m_previousScale = { 0.0f, 0.0f, 0.0f };
		OvMaths::FVector3 m_bodyPosition;
		OvMaths::FQuaternion m_bodyRotation;

		/* The last two poses the physics gave, for interpolation */
		bool m_hasPhysicsPose = false;
		OvMaths::FVector3 m_previousPhysicsPosition;
		OvMaths::FVector3 m_physicsPosition;
		OvMaths::FQuaternion m_previousPhysicsRotation;
		OvMaths::FQuaternion m_physicsRotation;
		static OvTools::Eventing::Event<PhysicalObject&>	CreatedEvent;
		static OvTools::Eventing::Event<PhysicalObject&>	DestroyedEvent;

		/* Jolt relatives */
		JPH::BodyInterface*		m_bodyInterface = nullptr;
		JPH::Body*				m_body = nullptr;

	protected:
		static constexpr float kMinimumShapeRadius = 0.001f;
	};
}