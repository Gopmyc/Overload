/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <OvDebug/Assertion.h>
#include <OvDebug/Logger.h>
#include <OvPhysics/Entities/PhysicalObject.h>
#include <OvPhysics/Tools/Conversion.h>

using namespace OvPhysics::Tools;
using namespace OvPhysics::Settings;

OvTools::Eventing::Event<OvPhysics::Entities::PhysicalObject&>	OvPhysics::Entities::PhysicalObject::CreatedEvent;
OvTools::Eventing::Event<OvPhysics::Entities::PhysicalObject&>	OvPhysics::Entities::PhysicalObject::DestroyedEvent;

namespace
{
	constexpr float kMinimumMass = 0.0000001f;

	JPH::EMotionQuality ToMotionQuality(OvPhysics::Entities::PhysicalObject::ECollisionDetectionMode p_mode)
	{
		switch (p_mode)
		{
		case OvPhysics::Entities::PhysicalObject::ECollisionDetectionMode::CONTINUOUS:
			return JPH::EMotionQuality::LinearCast;
		default:
			return JPH::EMotionQuality::Discrete;
		}
	}

	JPH::EAllowedDOFs ToAllowedDOFs(const OvMaths::FVector3& p_linearFactor, const OvMaths::FVector3& p_angularFactor)
	{
		JPH::EAllowedDOFs result = JPH::EAllowedDOFs::None;

		if (p_linearFactor.x != 0.0f)
		{
			result |= JPH::EAllowedDOFs::TranslationX;
		}

		if (p_linearFactor.y != 0.0f)
		{
			result |= JPH::EAllowedDOFs::TranslationY;
		}

		if (p_linearFactor.z != 0.0f)
		{
			result |= JPH::EAllowedDOFs::TranslationZ;
		}

		if (p_angularFactor.x != 0.0f)
		{
			result |= JPH::EAllowedDOFs::RotationX;
		}

		if (p_angularFactor.y != 0.0f)
		{
			result |= JPH::EAllowedDOFs::RotationY;
		}

		if (p_angularFactor.z != 0.0f)
		{
			result |= JPH::EAllowedDOFs::RotationZ;
		}

		return result;
	}

	bool IsSamePose(const OvMaths::FVector3& p_position, const OvMaths::FQuaternion& p_rotation, const OvMaths::FVector3& p_otherPosition, const OvMaths::FQuaternion& p_otherRotation)
	{
		return
			p_position.x == p_otherPosition.x && p_position.y == p_otherPosition.y && p_position.z == p_otherPosition.z &&
			p_rotation.x == p_otherRotation.x && p_rotation.y == p_otherRotation.y && p_rotation.z == p_otherRotation.z && p_rotation.w == p_otherRotation.w;
	}
}

OvPhysics::Entities::PhysicalObject::PhysicalObject() :
	m_transform(new OvMaths::FTransform()),
	m_internalTransform(true)
{
	CollisionStartEvent += [this](OvPhysics::Entities::PhysicalObject& otherPhysicalObject)
	{
		UpdateBodyTransform();
	};
}

OvPhysics::Entities::PhysicalObject::PhysicalObject(OvMaths::FTransform& p_transform) :
	m_transform(&p_transform),
	m_internalTransform(false)
{

}

OvPhysics::Entities::PhysicalObject::~PhysicalObject()
{
	DestroyBody();
	PhysicalObject::DestroyedEvent.Invoke(*this);

	if (m_internalTransform)
		delete m_transform;
}

void OvPhysics::Entities::PhysicalObject::Init()
{
	PhysicalObject::CreatedEvent.Invoke(*this);
	CreateBody({});
}

void OvPhysics::Entities::PhysicalObject::AddForce(const OvMaths::FVector3& p_force)
{
	if (m_body->IsDynamic())
	{
		m_body->AddForce(Conversion::ToJoltVector3(p_force));
	}
}

void OvPhysics::Entities::PhysicalObject::AddImpulse(const OvMaths::FVector3& p_impulse)
{
	if (m_body->IsDynamic())
	{
		m_body->AddImpulse(Conversion::ToJoltVector3(p_impulse));
	}
}

void OvPhysics::Entities::PhysicalObject::ClearForces()
{
	if (m_body->IsDynamic())
	{
		m_body->ResetForce();
		m_body->ResetTorque();
	}
}

float OvPhysics::Entities::PhysicalObject::GetMass() const
{
	return m_mass;
}

const OvPhysics::Entities::PhysicalObject::ECollisionDetectionMode& OvPhysics::Entities::PhysicalObject::GetCollisionDetectionMode() const
{
	return m_collisionMode;
}

float OvPhysics::Entities::PhysicalObject::GetBounciness() const
{
	return m_body->GetRestitution();
}

float OvPhysics::Entities::PhysicalObject::GetFriction() const
{
	return m_body->GetFriction();
}

OvMaths::FVector3 OvPhysics::Entities::PhysicalObject::GetLinearVelocity() const
{
	return Conversion::ToOvVector3(m_body->GetLinearVelocity());
}

OvMaths::FVector3 OvPhysics::Entities::PhysicalObject::GetAngularVelocity() const
{
	return Conversion::ToOvVector3(m_body->GetAngularVelocity());
}

OvMaths::FVector3 OvPhysics::Entities::PhysicalObject::GetLinearFactor() const
{
	return m_linearFactor;
}

OvMaths::FVector3 OvPhysics::Entities::PhysicalObject::GetAngularFactor() const
{
	return m_angularFactor;
}

bool OvPhysics::Entities::PhysicalObject::IsTrigger() const
{
	return m_trigger;
}

bool OvPhysics::Entities::PhysicalObject::IsKinematic() const
{
	return m_kinematic;
}

bool OvPhysics::Entities::PhysicalObject::IsStatic() const
{
	return m_static || MustBeStatic();
}

bool OvPhysics::Entities::PhysicalObject::MustBeStatic() const
{
	return false;
}

OvMaths::FTransform& OvPhysics::Entities::PhysicalObject::GetTransform()
{
	return *m_transform;
}

void OvPhysics::Entities::PhysicalObject::SetMass(float p_mass)
{
	m_mass = p_mass;
	RecreateBody();
}

void OvPhysics::Entities::PhysicalObject::SetCollisionDetectionMode(ECollisionDetectionMode p_mode)
{
	m_collisionMode = p_mode;
	m_bodyInterface->SetMotionQuality(m_body->GetID(), ToMotionQuality(m_collisionMode));
}

void OvPhysics::Entities::PhysicalObject::SetBounciness(float p_bounciness)
{
	m_body->SetRestitution(p_bounciness);
}

void OvPhysics::Entities::PhysicalObject::SetFriction(float p_friction)
{
	m_body->SetFriction(p_friction);
}

void OvPhysics::Entities::PhysicalObject::SetLinearVelocity(const OvMaths::FVector3 & p_linearVelocity)
{
	if (m_body->IsDynamic())
	{
		m_body->SetLinearVelocityClamped(Conversion::ToJoltVector3(p_linearVelocity));
	}
}

void OvPhysics::Entities::PhysicalObject::SetAngularVelocity(const OvMaths::FVector3 & p_angularVelocity)
{
	if (m_body->IsDynamic())
	{
		m_body->SetAngularVelocityClamped(Conversion::ToJoltVector3(p_angularVelocity));
	}
}

void OvPhysics::Entities::PhysicalObject::SetLinearFactor(const OvMaths::FVector3 & p_linearFactor)
{
	m_linearFactor = p_linearFactor;
	RecreateBody();
}

void OvPhysics::Entities::PhysicalObject::SetAngularFactor(const OvMaths::FVector3 & p_angularFactor)
{
	m_angularFactor = p_angularFactor;
	RecreateBody();
}

void OvPhysics::Entities::PhysicalObject::SetTrigger(bool p_trigger)
{
	m_body->SetIsSensor(p_trigger);
	m_trigger = p_trigger;
}

void OvPhysics::Entities::PhysicalObject::SetKinematic(bool p_kinematic)
{
	m_kinematic = p_kinematic;

	if (m_kinematic)
	{
		ClearForces();
		SetLinearVelocity(OvMaths::FVector3::Zero);
		SetAngularVelocity(OvMaths::FVector3::Zero);
	}

	RecreateBody();
}

void OvPhysics::Entities::PhysicalObject::SetStatic(bool p_static)
{
	if (m_static != p_static)
	{
		m_static = p_static;
		RecreateBody();
	}
}

void OvPhysics::Entities::PhysicalObject::SetEnabled(bool p_enabled)
{
	m_enabled = p_enabled;

	if (!m_enabled)
		Unconsider();
	else
		Consider();
}

bool OvPhysics::Entities::PhysicalObject::IsEnabled() const
{
	return m_enabled;
}

void OvPhysics::Entities::PhysicalObject::UpdateBodyTransform()
{
	const OvMaths::FVector3& position = m_transform->GetWorldPosition();
	const OvMaths::FQuaternion& rotation = m_transform->GetWorldRotation();

	// The body is only moved when its transform was, which most static bodies never are
	if (!IsSamePose(position, rotation, m_bodyPosition, m_bodyRotation))
	{
		m_bodyPosition = position;
		m_bodyRotation = rotation;

		m_bodyInterface->SetPositionAndRotation(
			m_body->GetID(),
			Conversion::ToJoltVector3(position),
			Conversion::ToJoltQuaternion(rotation),
			JPH::EActivation::DontActivate
		);
	}

	if (OvMaths::FVector3::Distance(m_transform->GetWorldScale(), m_previousScale) >= 0.01f)
	{
		RecreateBody();
	}
}

void OvPhysics::Entities::PhysicalObject::UpdateFTransform()
{
	if (!m_body->IsDynamic())
	{
		return;
	}

	const OvMaths::FVector3 position = Conversion::ToOvVector3(m_body->GetPosition());
	const OvMaths::FQuaternion rotation = Conversion::ToOvQuaternion(m_body->GetRotation());

	// World setters recompute the local transform (and its scale) from the matrices, so they are only used when required
	if (m_transform->HasParent())
	{
		m_transform->SetWorldPosition(position);
		m_transform->SetWorldRotation(rotation);
	}
	else
	{
		m_transform->SetLocalPosition(position);
		m_transform->SetLocalRotation(rotation);
	}

	m_bodyPosition = m_transform->GetWorldPosition();
	m_bodyRotation = m_transform->GetWorldRotation();
}

void OvPhysics::Entities::PhysicalObject::RecreateBody()
{
	CreateBody(DestroyBody());
}

void OvPhysics::Entities::PhysicalObject::Consider()
{
	if (!m_considered)
	{
		m_considered = true;
		m_bodyInterface->AddBody(m_body->GetID(), m_body->IsStatic() ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
	}
}

void OvPhysics::Entities::PhysicalObject::Unconsider()
{
	if (m_considered)
	{
		m_considered = false;
		m_bodyInterface->RemoveBody(m_body->GetID());
	}
}

void OvPhysics::Entities::PhysicalObject::CreateBody(const Settings::BodySettings & p_bodySettings)
{
	OVASSERT(m_bodyInterface != nullptr, "A PhysicsEngine must exist before creating a PhysicalObject");

	m_previousScale = m_transform->GetWorldScale();
	m_bodyPosition = m_transform->GetWorldPosition();
	m_bodyRotation = m_transform->GetWorldRotation();
	m_linearFactor = p_bodySettings.linearFactor;
	m_angularFactor = p_bodySettings.angularFactor;

	// Jolt cannot simulate a dynamic body without any degree of freedom, such a body is simulated as kinematic
	const JPH::EAllowedDOFs allowedDOFs = ToAllowedDOFs(m_linearFactor, m_angularFactor);
	const bool isStatic = IsStatic();
	const bool isDynamic = !isStatic && !m_kinematic && allowedDOFs != JPH::EAllowedDOFs::None;

	JPH::BodyCreationSettings bodyCreationSettings(
		CreateShape({ std::abs(m_previousScale.x), std::abs(m_previousScale.y), std::abs(m_previousScale.z) }),
		Conversion::ToJoltVector3(m_bodyPosition),
		Conversion::ToJoltQuaternion(m_bodyRotation),
		isStatic ? JPH::EMotionType::Static : isDynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Kinematic,
		kObjectLayer
	);

	bodyCreationSettings.mAllowedDOFs = isDynamic ? allowedDOFs : JPH::EAllowedDOFs::All;

	// A static body has no mass
	if (!isStatic)
	{
		bodyCreationSettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
		bodyCreationSettings.mMassPropertiesOverride.mMass = std::max(kMinimumMass, m_mass);
	}

	bodyCreationSettings.mIsSensor = p_bodySettings.isTrigger;
	bodyCreationSettings.mMotionQuality = ToMotionQuality(m_collisionMode);
	bodyCreationSettings.mAllowSleeping = false; // TODO: Avoid using always active
	bodyCreationSettings.mFriction = p_bodySettings.friction;
	bodyCreationSettings.mRestitution = p_bodySettings.restitution;
	bodyCreationSettings.mLinearDamping = 0.0f;
	bodyCreationSettings.mAngularDamping = 0.0f;
	bodyCreationSettings.mUserData = reinterpret_cast<JPH::uint64>(this);

	if (isDynamic)
	{
		bodyCreationSettings.mLinearVelocity = Conversion::ToJoltVector3(p_bodySettings.linearVelocity);
		bodyCreationSettings.mAngularVelocity = Conversion::ToJoltVector3(p_bodySettings.angularVelocity);
	}

	m_body = m_bodyInterface->CreateBody(bodyCreationSettings);

	OVASSERT(m_body != nullptr, "Unable to create a physical body, the maximum number of bodies has been reached");

	if (m_enabled)
		Consider();
}

OvPhysics::Settings::BodySettings OvPhysics::Entities::PhysicalObject::DestroyBody()
{
	BodySettings result
	{
		Conversion::ToOvVector3(m_body->GetLinearVelocity()),
		Conversion::ToOvVector3(m_body->GetAngularVelocity()),
		m_linearFactor,
		m_angularFactor,
		GetBounciness(),
		GetFriction(),
		IsTrigger(),
		IsKinematic()
	};

	Unconsider();

	m_bodyInterface->DestroyBody(m_body->GetID());
	m_body = nullptr;

	return result;
}
