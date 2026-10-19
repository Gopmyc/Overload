/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <numbers>
#include <string>

#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Ragdoll/Ragdoll.h>
#include <Jolt/Skeleton/Skeleton.h>

#include <OvDebug/Logger.h>

#include <OvPhysics/Entities/Ragdoll.h>
#include <OvPhysics/Tools/Conversion.h>

using namespace OvPhysics::Tools;

namespace
{
	constexpr float kMinimumRadius = 0.005f;
	constexpr float kMinimumHalfHeight = 0.001f;
	constexpr float kMinimumMass = 0.1f;
	constexpr float kMinimumAxisLength = 0.0001f;
	constexpr float kFriction = 0.8f;
	constexpr float kRestitution = 0.0f;

	/**
	* A capsule along the segment between two points of the body's space
	*/
	JPH::Ref<JPH::Shape> CreatePartShape(const OvPhysics::Settings::RagdollPartSettings& p_part)
	{
		const JPH::Vec3 start = Conversion::ToJoltVector3(p_part.capsuleStart);
		const JPH::Vec3 end = Conversion::ToJoltVector3(p_part.capsuleEnd);
		const JPH::Vec3 axis = end - start;
		const float length = axis.Length();
		const float radius = std::max(p_part.radius, kMinimumRadius);
		const float halfHeight = std::max(length * 0.5f - radius, kMinimumHalfHeight);

		// Jolt's capsules stand along Y
		const JPH::Quat rotation = length > kMinimumAxisLength ?
			JPH::Quat::sFromTo(JPH::Vec3::sAxisY(), axis / length) :
			JPH::Quat::sIdentity();

		const JPH::RotatedTranslatedShapeSettings settings((start + end) * 0.5f, rotation, new JPH::CapsuleShape(halfHeight, radius));
		const JPH::ShapeSettings::ShapeResult result = settings.Create();

		return result.IsValid() ? result.Get() : JPH::Ref<JPH::Shape>{};
	}

	/**
	* The joint between a part and its parent, at the part's origin: the part twists about its own capsule's axis and
	* swings within a cone around it, starting from the pose the ragdoll is created in
	*/
	JPH::Ref<JPH::SwingTwistConstraintSettings> CreatePartConstraint(const OvPhysics::Settings::RagdollPartSettings& p_part)
	{
		const JPH::Vec3 axis = Conversion::ToJoltVector3(p_part.capsuleEnd - p_part.capsuleStart);
		const JPH::Vec3 localTwist = axis.Length() > kMinimumAxisLength ? axis.Normalized() : JPH::Vec3::sAxisY();
		const JPH::Vec3 twist = Conversion::ToJoltQuaternion(p_part.rotation) * localTwist;
		const JPH::Vec3 plane = twist.GetNormalizedPerpendicular();
		const JPH::RVec3 position = Conversion::ToJoltVector3(p_part.position);
		const float swing = std::clamp(p_part.swingLimit, 0.0f, std::numbers::pi_v<float>);
		const float twistLimit = std::clamp(p_part.twistLimit, 0.0f, std::numbers::pi_v<float>);

		JPH::Ref<JPH::SwingTwistConstraintSettings> settings = new JPH::SwingTwistConstraintSettings();
		settings->mSpace = JPH::EConstraintSpace::WorldSpace;
		settings->mPosition1 = settings->mPosition2 = position;
		settings->mTwistAxis1 = settings->mTwistAxis2 = twist;
		settings->mPlaneAxis1 = settings->mPlaneAxis2 = plane;
		settings->mNormalHalfConeAngle = swing;
		settings->mPlaneHalfConeAngle = swing;
		settings->mTwistMinAngle = -twistLimit;
		settings->mTwistMaxAngle = twistLimit;
		return settings;
	}
}

struct OvPhysics::Entities::Ragdoll::Implementation
{
	explicit Implementation(JPH::PhysicsSystem& p_system) : system(p_system) {}

	JPH::PhysicsSystem& system;
	JPH::Ref<JPH::RagdollSettings> settings;
	JPH::Ref<JPH::Ragdoll> ragdoll;
};

OvPhysics::Entities::Ragdoll::Ragdoll(
	JPH::PhysicsSystem& p_system,
	const std::vector<Settings::RagdollPartSettings>& p_parts,
	const OvMaths::FVector3& p_linearVelocity,
	uint16_t p_objectLayer,
	uint32_t p_groupID
) : m_implementation(std::make_unique<Implementation>(p_system))
{
	if (p_parts.empty())
	{
		return;
	}

	JPH::Ref<JPH::Skeleton> skeleton = new JPH::Skeleton();
	JPH::Ref<JPH::RagdollSettings> settings = new JPH::RagdollSettings();
	settings->mSkeleton = skeleton;
	settings->mParts.resize(p_parts.size());

	// The pose the ragdoll is created in, for the parts that already touch to be kept from colliding
	std::vector<JPH::Mat44> jointMatrices(p_parts.size());

	for (size_t index = 0; index < p_parts.size(); ++index)
	{
		const auto& part = p_parts[index];

		if (part.parent >= static_cast<int32_t>(index))
		{
			OVLOG_WARNING("Ragdoll: a part must come after the part it hangs from");
			return;
		}

		skeleton->AddJoint("Part" + std::to_string(index), part.parent);

		JPH::Ref<JPH::Shape> shape = CreatePartShape(part);
		if (!shape)
		{
			OVLOG_WARNING("Ragdoll: a part's capsule could not be made");
			return;
		}

		auto& body = settings->mParts[index];
		body.SetShape(shape);
		body.mPosition = Conversion::ToJoltVector3(part.position);
		body.mRotation = Conversion::ToJoltQuaternion(part.rotation);
		body.mMotionType = JPH::EMotionType::Dynamic;
		body.mMotionQuality = JPH::EMotionQuality::LinearCast;
		body.mObjectLayer = p_objectLayer;
		body.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
		body.mMassPropertiesOverride.mMass = std::max(part.mass, kMinimumMass);
		body.mFriction = kFriction;
		body.mRestitution = kRestitution;
		body.mLinearVelocity = Conversion::ToJoltVector3(p_linearVelocity);

		if (part.parent >= 0)
		{
			body.mToParent = CreatePartConstraint(part);
		}

		jointMatrices[index] = JPH::Mat44::sRotationTranslation(body.mRotation, body.mPosition);
	}

	if (!settings->Stabilize())
	{
		OVLOG_WARNING("Ragdoll: its masses could not be stabilized");
	}

	settings->DisableParentChildCollisions(jointMatrices.data());
	settings->CalculateBodyIndexToConstraintIndex();

	JPH::Ref<JPH::Ragdoll> ragdoll = settings->CreateRagdoll(p_groupID, 0, &m_implementation->system);
	if (!ragdoll)
	{
		OVLOG_WARNING("Ragdoll: no body left to make it");
		return;
	}

	ragdoll->AddToPhysicsSystem(JPH::EActivation::Activate);

	m_implementation->settings = settings;
	m_implementation->ragdoll = ragdoll;
}

OvPhysics::Entities::Ragdoll::~Ragdoll()
{
	if (m_implementation->ragdoll)
	{
		m_implementation->ragdoll->RemoveFromPhysicsSystem();
	}
}

bool OvPhysics::Entities::Ragdoll::IsValid() const
{
	return m_implementation->ragdoll != nullptr;
}

size_t OvPhysics::Entities::Ragdoll::GetPartCount() const
{
	return IsValid() ? m_implementation->ragdoll->GetBodyCount() : 0;
}

OvMaths::FVector3 OvPhysics::Entities::Ragdoll::GetPartPosition(size_t p_part) const
{
	if (p_part >= GetPartCount())
	{
		return OvMaths::FVector3::Zero;
	}

	const auto bodyID = m_implementation->ragdoll->GetBodyID(static_cast<int>(p_part));
	return Conversion::ToOvVector3(m_implementation->system.GetBodyInterface().GetPosition(bodyID));
}

OvMaths::FQuaternion OvPhysics::Entities::Ragdoll::GetPartRotation(size_t p_part) const
{
	if (p_part >= GetPartCount())
	{
		return OvMaths::FQuaternion::Identity;
	}

	const auto bodyID = m_implementation->ragdoll->GetBodyID(static_cast<int>(p_part));
	return Conversion::ToOvQuaternion(m_implementation->system.GetBodyInterface().GetRotation(bodyID));
}

void OvPhysics::Entities::Ragdoll::AddImpulse(size_t p_part, const OvMaths::FVector3& p_impulse)
{
	if (p_part >= GetPartCount())
	{
		return;
	}

	const auto bodyID = m_implementation->ragdoll->GetBodyID(static_cast<int>(p_part));
	m_implementation->system.GetBodyInterface().AddImpulse(bodyID, Conversion::ToJoltVector3(p_impulse));
}
