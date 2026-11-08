/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cfloat>
#include <cmath>

#include <Jolt/Jolt.h>

#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/ShapeFilter.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <OvPhysics/Entities/CharacterController.h>
#include <OvPhysics/Tools/Conversion.h>

using OvPhysics::Tools::Conversion;

namespace
{
	// Below this speed away from the ground, a character on the ground stays on it
	constexpr float kSeparationSpeed = 0.1f;

	// Falling slower than this while touching something, a character is held up by it
	constexpr float kRestingSpeed = 0.01f;
	constexpr float kMinimumMass = 0.001f;

	/**
	* A character's sweeps go through its own body, and through ragdolls when the settings ask it
	*/
	class CharacterBodyFilter final : public JPH::BodyFilter
	{
	public:
		CharacterBodyFilter(const JPH::BodyID& p_ownBody, bool p_ignoreRagdolls) :
			m_ownBody(p_ownBody),
			m_ignoreRagdolls(p_ignoreRagdolls)
		{
		}

		bool ShouldCollide(const JPH::BodyID& p_body) const override
		{
			return p_body != m_ownBody;
		}

		bool ShouldCollideLocked(const JPH::Body& p_body) const override
		{
			// A ragdoll's bodies carry no physical object
			return !(m_ignoreRagdolls && p_body.GetUserData() == 0);
		}

	private:
		const JPH::BodyID m_ownBody;
		const bool m_ignoreRagdolls;
	};

	JPH::Plane MakeSupportingVolume(const JPH::Vec3& p_up, float p_halfHeight)
	{
		// Only what touches the lower half-sphere of the capsule can hold the character
		return JPH::Plane(p_up, p_halfHeight);
	}

	OvPhysics::Entities::CharacterController::EGroundState ToGroundState(JPH::CharacterBase::EGroundState p_state)
	{
		using EGroundState = OvPhysics::Entities::CharacterController::EGroundState;

		switch (p_state)
		{
		case JPH::CharacterBase::EGroundState::OnGround:		return EGroundState::ON_GROUND;
		case JPH::CharacterBase::EGroundState::OnSteepGround:	return EGroundState::ON_STEEP_GROUND;
		case JPH::CharacterBase::EGroundState::NotSupported:	return EGroundState::NOT_SUPPORTED;
		default:												return EGroundState::IN_AIR;
		}
	}

	bool IsFinite(const OvMaths::FVector3& p_vector)
	{
		return std::isfinite(p_vector.x) && std::isfinite(p_vector.y) && std::isfinite(p_vector.z);
	}
}

OvPhysics::Entities::CharacterController::CharacterController(OvMaths::FTransform& p_transform, float p_radius, float p_height) :
	PhysicalObject(p_transform),
	m_radius(p_radius),
	m_height(p_height)
{
	m_mass = 70.0f;
	Init();
}

OvPhysics::Entities::CharacterController::~CharacterController()
{
	if (m_character)
	{
		m_character->Release();
		m_character = nullptr;
	}
}

void OvPhysics::Entities::CharacterController::SetRadius(float p_radius)
{
	m_radius = p_radius;
	RecreateBody();
	m_shapeChanged = true;
}

void OvPhysics::Entities::CharacterController::SetHeight(float p_height)
{
	m_height = p_height;
	RecreateBody();
	m_shapeChanged = true;
}

float OvPhysics::Entities::CharacterController::GetRadius() const
{
	return m_radius;
}

float OvPhysics::Entities::CharacterController::GetHeight() const
{
	return m_height;
}

void OvPhysics::Entities::CharacterController::SetMaxSlopeAngle(float p_degrees)
{
	m_maxSlopeAngle = std::isfinite(p_degrees) ? std::clamp(p_degrees, 0.0f, 90.0f) : m_maxSlopeAngle;
	ApplySettings();
}

float OvPhysics::Entities::CharacterController::GetMaxSlopeAngle() const
{
	return m_maxSlopeAngle;
}

void OvPhysics::Entities::CharacterController::SetStepHeight(float p_height)
{
	m_stepHeight = std::isfinite(p_height) ? std::max(p_height, 0.0f) : m_stepHeight;
}

float OvPhysics::Entities::CharacterController::GetStepHeight() const
{
	return m_stepHeight;
}

void OvPhysics::Entities::CharacterController::SetGroundSnapDistance(float p_distance)
{
	m_groundSnapDistance = std::isfinite(p_distance) ? std::max(p_distance, 0.0f) : m_groundSnapDistance;
}

float OvPhysics::Entities::CharacterController::GetGroundSnapDistance() const
{
	return m_groundSnapDistance;
}

void OvPhysics::Entities::CharacterController::SetGravityFactor(float p_factor)
{
	m_gravityFactor = std::isfinite(p_factor) ? p_factor : m_gravityFactor;
}

float OvPhysics::Entities::CharacterController::GetGravityFactor() const
{
	return m_gravityFactor;
}

void OvPhysics::Entities::CharacterController::SetPushStrength(float p_strength)
{
	m_pushStrength = std::isfinite(p_strength) ? std::max(p_strength, 0.0f) : m_pushStrength;
	ApplySettings();
}

float OvPhysics::Entities::CharacterController::GetPushStrength() const
{
	return m_pushStrength;
}

void OvPhysics::Entities::CharacterController::AddForce(const OvMaths::FVector3& p_force)
{
	if (IsFinite(p_force))
	{
		m_force += p_force;
	}
}

void OvPhysics::Entities::CharacterController::AddImpulse(const OvMaths::FVector3& p_impulse)
{
	if (IsFinite(p_impulse))
	{
		m_velocity += p_impulse / std::max(m_mass, kMinimumMass);
	}
}

void OvPhysics::Entities::CharacterController::ClearForces()
{
	m_force = OvMaths::FVector3::Zero;
}

OvMaths::FVector3 OvPhysics::Entities::CharacterController::GetLinearVelocity() const
{
	return m_velocity;
}

void OvPhysics::Entities::CharacterController::SetLinearVelocity(const OvMaths::FVector3& p_linearVelocity)
{
	if (IsFinite(p_linearVelocity))
	{
		m_velocity = p_linearVelocity;
	}
}

OvMaths::FVector3 OvPhysics::Entities::CharacterController::GetRealVelocity() const
{
	return m_realVelocity;
}

OvPhysics::Entities::CharacterController::EGroundState OvPhysics::Entities::CharacterController::GetGroundState() const
{
	return m_character ? ToGroundState(m_character->GetGroundState()) : EGroundState::IN_AIR;
}

bool OvPhysics::Entities::CharacterController::IsGrounded() const
{
	return GetGroundState() == EGroundState::ON_GROUND;
}

OvMaths::FVector3 OvPhysics::Entities::CharacterController::GetGroundNormal() const
{
	return m_character && GetGroundState() != EGroundState::IN_AIR ? Conversion::ToOvVector3(m_character->GetGroundNormal()) : OvMaths::FVector3::Zero;
}

OvMaths::FVector3 OvPhysics::Entities::CharacterController::GetGroundPoint() const
{
	return m_character ? Conversion::ToOvVector3(m_character->GetGroundPosition()) : OvMaths::FVector3::Zero;
}

OvMaths::FVector3 OvPhysics::Entities::CharacterController::GetGroundVelocity() const
{
	return m_character ? Conversion::ToOvVector3(m_character->GetGroundVelocity()) : OvMaths::FVector3::Zero;
}

OvPhysics::Entities::PhysicalObject* OvPhysics::Entities::CharacterController::GetGroundObject() const
{
	if (!m_character || GetGroundState() == EGroundState::IN_AIR || m_character->GetGroundBodyID().IsInvalid())
	{
		return nullptr;
	}

	return reinterpret_cast<PhysicalObject*>(m_bodyInterface->GetUserData(m_character->GetGroundBodyID()));
}

bool OvPhysics::Entities::CharacterController::IsCharacter() const
{
	return true;
}

JPH::Shape* OvPhysics::Entities::CharacterController::CreateShape(const OvMaths::FVector3& p_scale) const
{
	const float radius = std::max(m_radius * std::max(p_scale.x, p_scale.z), kMinimumShapeRadius);
	const float halfHeight = m_height * p_scale.y * 0.5f;

	if (halfHeight > 0.0f)
	{
		return new JPH::CapsuleShape(halfHeight, radius);
	}

	return new JPH::SphereShape(radius);
}

float OvPhysics::Entities::CharacterController::GetHalfHeight() const
{
	return std::max(m_height * std::abs(m_previousScale.y) * 0.5f, 0.0f);
}

void OvPhysics::Entities::CharacterController::ApplySettings()
{
	if (m_character)
	{
		m_character->SetMaxSlopeAngle(JPH::DegreesToRadians(m_maxSlopeAngle));
		m_character->SetMaxStrength(m_pushStrength);
		m_character->SetMass(std::max(m_mass, kMinimumMass));
	}
}

void OvPhysics::Entities::CharacterController::Attach(JPH::PhysicsSystem& p_physicsSystem)
{
	if (m_character)
	{
		return;
	}

	const OvMaths::FVector3 scale = m_transform->GetWorldScale();

	JPH::CharacterVirtualSettings settings;
	settings.mShape = CreateShape({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) });
	settings.mUp = JPH::Vec3::sAxisY();
	settings.mSupportingVolume = MakeSupportingVolume(settings.mUp, std::max(m_height * std::abs(scale.y) * 0.5f, 0.0f));
	settings.mMaxSlopeAngle = JPH::DegreesToRadians(m_maxSlopeAngle);
	settings.mMass = std::max(m_mass, kMinimumMass);
	settings.mMaxStrength = m_pushStrength;

	// Smoother over the inner edges of meshes and height fields
	settings.mEnhancedInternalEdgeRemoval = true;

	m_physicsSystem = &p_physicsSystem;
	m_character = new JPH::CharacterVirtual(&settings, Conversion::ToJoltVector3(m_transform->GetWorldPosition()), JPH::Quat::sIdentity(), &p_physicsSystem);
	m_character->AddRef();
	m_teleported = true;
}

void OvPhysics::Entities::CharacterController::UpdateBodyTransform()
{
	const OvMaths::FVector3 position = m_transform->GetWorldPosition();

	// The transform was moved by something else than the controller: the character is teleported there
	if (position.x != m_bodyPosition.x || position.y != m_bodyPosition.y || position.z != m_bodyPosition.z)
	{
		m_bodyPosition = position;
		m_bodyInterface->SetPosition(m_body->GetID(), Conversion::ToJoltVector3(position), JPH::EActivation::DontActivate);

		if (m_character)
		{
			m_character->SetPosition(Conversion::ToJoltVector3(position));
		}

		m_teleported = true;
	}

	if (OvMaths::FVector3::Distance(m_transform->GetWorldScale(), m_previousScale) >= 0.01f)
	{
		RecreateBody();
		m_shapeChanged = true;
	}
}

void OvPhysics::Entities::CharacterController::UpdateFTransform()
{
	if (!m_character || !IsEnabled())
	{
		return;
	}

	const OvMaths::FVector3 position = Conversion::ToOvVector3(m_character->GetPosition());

	// The rotation is the actor's own: only the position follows the character
	if (m_transform->HasParent())
	{
		m_transform->SetWorldPosition(position);
	}
	else
	{
		m_transform->SetLocalPosition(position);
	}

	m_bodyPosition = m_transform->GetWorldPosition();
}

void OvPhysics::Entities::CharacterController::Step(float p_deltaTime, const JPH::ObjectLayerPairFilter& p_layerPairFilter, bool p_ignoreRagdolls, JPH::TempAllocator& p_allocator, std::vector<std::pair<JPH::BodyID, JPH::BodyID>>& p_contacts)
{
	if (!m_character || !(p_deltaTime > 0.0f))
	{
		return;
	}

	const JPH::BroadPhaseLayerFilter broadPhaseFilter;
	const JPH::DefaultObjectLayerFilter layerFilter(p_layerPairFilter, m_layer);
	const CharacterBodyFilter bodyFilter(m_body->GetID(), p_ignoreRagdolls);
	const JPH::ShapeFilter shapeFilter;
	const JPH::Vec3 up = m_character->GetUp();

	if (m_shapeChanged)
	{
		const OvMaths::FVector3 scale = m_previousScale;
		const JPH::RefConst<JPH::Shape> shape = CreateShape({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) });

		// The new shape is taken whatever it overlaps: the character then gets out of what it overlaps
		m_character->SetShape(shape, FLT_MAX, broadPhaseFilter, layerFilter, bodyFilter, shapeFilter, p_allocator);
		m_character->SetSupportingVolume(MakeSupportingVolume(up, GetHalfHeight()));
		m_shapeChanged = false;
		m_teleported = true;
	}

	if (m_teleported)
	{
		m_character->RefreshContacts(broadPhaseFilter, layerFilter, bodyFilter, shapeFilter, p_allocator);
		m_teleported = false;
	}

	ApplySettings();

	// The ground may have moved since the last step
	m_character->UpdateGroundVelocity();

	JPH::Vec3 velocity = Conversion::ToJoltVector3(m_velocity) + Conversion::ToJoltVector3(m_force) * (p_deltaTime / std::max(m_mass, kMinimumMass));
	m_force = OvMaths::FVector3::Zero;

	const JPH::Vec3 groundVelocity = m_character->GetGroundVelocity();
	const bool standing =
		m_character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround &&
		(velocity - groundVelocity).Dot(up) < kSeparationSpeed;

	JPH::Vec3 stepVelocity;

	if (standing)
	{
		// The ground holds the character: it neither falls nor slides, and goes along with the ground
		velocity += (groundVelocity.Dot(up) - velocity.Dot(up)) * up;
		stepVelocity = velocity + groundVelocity - groundVelocity.Dot(up) * up;
	}
	else
	{
		velocity += m_physicsSystem->GetGravity() * (m_gravityFactor * p_deltaTime);
		stepVelocity = velocity;
	}

	JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
	updateSettings.mStickToFloorStepDown = -up * m_groundSnapDistance;
	updateSettings.mWalkStairsStepUp = up * m_stepHeight;

	const JPH::RVec3 previousPosition = m_character->GetPosition();

	m_character->SetLinearVelocity(stepVelocity);
	m_character->ExtendedUpdate(p_deltaTime, m_physicsSystem->GetGravity(), updateSettings, broadPhaseFilter, layerFilter, bodyFilter, shapeFilter, p_allocator);

	const JPH::Vec3 realVelocity = JPH::Vec3(m_character->GetPosition() - previousPosition) / p_deltaTime;
	m_realVelocity = Conversion::ToOvVector3(realVelocity);

	// The horizontal velocity is the one the character was given, walls or not. The vertical one is the
	// controller's: a ceiling ends a rise, and something that holds the character up ends its fall. A steep slope
	// does not: sliding down it turns the fall into the slide a body would have
	const float wanted = velocity.Dot(up);
	const float moved = realVelocity.Dot(up);
	float kept = wanted;

	if (wanted > 0.0f && moved < wanted)
	{
		kept = std::max(moved, 0.0f);
	}
	else if (wanted < 0.0f && moved > -kRestingSpeed && m_character->GetGroundState() != JPH::CharacterBase::EGroundState::InAir)
	{
		kept = 0.0f;
	}

	m_velocity = Conversion::ToOvVector3(velocity + (kept - wanted) * up);

	// The body follows, so that rigid bodies and queries meet the character where it is
	m_bodyInterface->MoveKinematic(m_body->GetID(), m_character->GetPosition(), JPH::Quat::sIdentity(), p_deltaTime);

	for (const JPH::CharacterContact& contact : m_character->GetActiveContacts())
	{
		if (contact.mHadCollision && !contact.mWasDiscarded && !contact.mIsSensorB && !contact.mBodyB.IsInvalid() && contact.mUserData != 0)
		{
			p_contacts.emplace_back(m_body->GetID(), contact.mBodyB);
		}
	}
}
