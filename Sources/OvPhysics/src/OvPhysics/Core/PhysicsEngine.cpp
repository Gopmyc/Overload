/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <utility>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollideShape.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <OvDebug/Logger.h>

#include <OvPhysics/Core/PhysicsEngine.h>
#include <OvPhysics/Entities/PhysicalObject.h>
#include <OvPhysics/Entities/Ragdoll.h>
#include <OvPhysics/Tools/Conversion.h>

using namespace OvPhysics::Tools;
using namespace OvPhysics::Entities;

std::map<std::pair<PhysicalObject*, PhysicalObject*>, bool> OvPhysics::Core::PhysicsEngine::m_collisionEvents;

namespace
{
	constexpr float kFixedTimeStep = 1.0f / 60.0f;
	constexpr int kMaxSubSteps = 10;
	constexpr uint32_t kMaxBodies = 65536;
	constexpr uint32_t kBodyMutexCount = 0; // 0 lets Jolt pick a default value
	constexpr uint32_t kMaxBodyPairs = 65536;
	constexpr uint32_t kMaxContactConstraints = 20480;
	constexpr uint32_t kObjectLayerCount = OvPhysics::Settings::PhysicsSettings::kLayerCount;
	constexpr uint32_t kBroadPhaseLayerCount = 1;
	constexpr size_t kTempAllocatorSize = 10 * 1024 * 1024;

	using PhysicalObjectEvent = OvTools::Eventing::Event<PhysicalObject&> PhysicalObject::*;

	/**
	* Invokes the trigger event of the target if it is a trigger, or its collision event if none of the objects is a trigger
	*/
	void InvokeContactEvent(PhysicalObject& p_target, PhysicalObject& p_other, PhysicalObjectEvent p_triggerEvent, PhysicalObjectEvent p_collisionEvent)
	{
		if (p_target.IsTrigger())
		{
			(p_target.*p_triggerEvent).Invoke(p_other);
		}
		else if (!p_other.IsTrigger())
		{
			(p_target.*p_collisionEvent).Invoke(p_other);
		}
	}
}

namespace
{
	/**
	* Every layer is in the one broad phase layer: which layers collide is decided by the pair filter alone, so that it
	* can change at run time
	*/
	class SingleBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer) const override
		{
			return true;
		}
	};

	/**
	* A ragdoll's bodies carry no physical object
	*/
	bool IsRagdollBody(const JPH::Body& p_body)
	{
		return p_body.GetUserData() == 0;
	}

	/**
	* A query only reports physical objects, and triggers only when asked
	*/
	class QueryBodyFilter final : public JPH::BodyFilter
	{
	public:
		explicit QueryBodyFilter(bool p_includeTriggers) :
			m_includeTriggers(p_includeTriggers)
		{
		}

		bool ShouldCollideLocked(const JPH::Body& p_body) const override
		{
			return !IsRagdollBody(p_body) && (m_includeTriggers || !p_body.IsSensor());
		}

	private:
		const bool m_includeTriggers;
	};

	/**
	* A query only sees the layers of its mask
	*/
	class LayerMaskFilter final : public JPH::ObjectLayerFilter
	{
	public:
		explicit LayerMaskFilter(uint16_t p_layerMask) :
			m_layerMask(p_layerMask)
		{
		}

		bool ShouldCollide(JPH::ObjectLayer p_layer) const override
		{
			return p_layer < OvPhysics::Settings::PhysicsSettings::kLayerCount && (m_layerMask >> p_layer & 1) != 0;
		}

	private:
		const uint16_t m_layerMask;
	};

	OvPhysics::Entities::PhysicalObject* ToPhysicalObject(uint64_t p_userData)
	{
		return reinterpret_cast<OvPhysics::Entities::PhysicalObject*>(p_userData);
	}

	/**
	* Keeps each object's nearest hit, nearest object first
	*/
	std::vector<OvPhysics::Entities::PhysicsHit> KeepNearestPerObject(std::vector<OvPhysics::Entities::PhysicsHit> p_hits)
	{
		std::stable_sort(p_hits.begin(), p_hits.end(), [](const auto& p_left, const auto& p_right)
		{
			return p_left.distance < p_right.distance;
		});

		std::vector<OvPhysics::Entities::PhysicsHit> nearest;
		std::unordered_set<OvPhysics::Entities::PhysicalObject*> seen;

		for (const auto& hit : p_hits)
		{
			if (hit.object && seen.insert(hit.object).second)
			{
				nearest.push_back(hit);
			}
		}

		return nearest;
	}

	std::vector<OvPhysics::Entities::PhysicalObject*> CollideShape(
		const JPH::PhysicsSystem& p_physicsSystem,
		const JPH::Shape& p_shape,
		JPH::RMat44Arg p_transform,
		const OvPhysics::Settings::QuerySettings& p_settings)
	{
		JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;

		p_physicsSystem.GetNarrowPhaseQuery().CollideShape(
			&p_shape,
			JPH::Vec3::sOne(),
			p_transform,
			JPH::CollideShapeSettings(),
			JPH::RVec3::sZero(),
			collector,
			{},
			LayerMaskFilter(p_settings.layerMask),
			QueryBodyFilter(p_settings.includeTriggers)
		);

		const JPH::BodyInterface& bodyInterface = p_physicsSystem.GetBodyInterface();

		std::vector<OvPhysics::Entities::PhysicalObject*> objects;
		std::unordered_set<OvPhysics::Entities::PhysicalObject*> seen;

		for (const JPH::CollideShapeResult& hit : collector.mHits)
		{
			if (auto* object = ToPhysicalObject(bodyInterface.GetUserData(hit.mBodyID2)); object && seen.insert(object).second)
			{
				objects.push_back(object);
			}
		}

		return objects;
	}

	/**
	* A direction made of finite numbers and not too short to normalize
	*/
	bool IsUsableDirection(const JPH::Vec3& p_direction)
	{
		const float lengthSquared = p_direction.LengthSq();
		return std::isfinite(lengthSquared) && lengthSquared > 1e-12f;
	}
}

/**
* Collects the pairs of bodies in contact during the simulation (called from the simulation threads)
*/
class OvPhysics::Core::PhysicsEngine::ContactListener final : public JPH::ContactListener
{
public:
	/**
	* Only called between two simulation steps
	* @param p_ignore
	*/
	void SetRagdollIgnoresDynamicBodies(bool p_ignore)
	{
		m_ragdollIgnoresDynamicBodies = p_ignore;
	}

	/**
	* When the settings ask it, a ragdoll falls on the world but goes through the dynamic bodies: it never pushes nor
	* trips a living body
	*/
	JPH::ValidateResult OnContactValidate(const JPH::Body& p_body1, const JPH::Body& p_body2, JPH::RVec3Arg, const JPH::CollideShapeResult&) override
	{
		if (m_ragdollIgnoresDynamicBodies)
		{
			const bool isRagdoll1 = IsRagdollBody(p_body1);
			const bool isRagdoll2 = IsRagdollBody(p_body2);

			if (isRagdoll1 != isRagdoll2 && (isRagdoll1 ? p_body2 : p_body1).IsDynamic())
			{
				return JPH::ValidateResult::RejectAllContactsForThisBodyPair;
			}
		}

		return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	void OnContactAdded(const JPH::Body& p_body1, const JPH::Body& p_body2, const JPH::ContactManifold&, JPH::ContactSettings&) override
	{
		AddContact(p_body1, p_body2);
	}

	void OnContactPersisted(const JPH::Body& p_body1, const JPH::Body& p_body2, const JPH::ContactManifold&, JPH::ContactSettings&) override
	{
		AddContact(p_body1, p_body2);
	}

	/**
	* Returns the collected pairs of bodies in contact and clears them
	*/
	std::vector<std::pair<JPH::BodyID, JPH::BodyID>> ConsumeContacts()
	{
		std::scoped_lock lock(m_mutex);
		return std::exchange(m_contacts, {});
	}

private:
	void AddContact(const JPH::Body& p_body1, const JPH::Body& p_body2)
	{
		// Only physical objects raise collision events, and a ragdoll's bodies are none
		if (p_body1.GetUserData() == 0 || p_body2.GetUserData() == 0)
		{
			return;
		}

		std::scoped_lock lock(m_mutex);
		m_contacts.emplace_back(p_body1.GetID(), p_body2.GetID());
	}

private:
	bool m_ragdollIgnoresDynamicBodies = false;
	std::mutex m_mutex;
	std::vector<std::pair<JPH::BodyID, JPH::BodyID>> m_contacts;
};

OvPhysics::Core::PhysicsEngine::PhysicsEngine(const Settings::PhysicsSettings & p_settings)
{
	JPH::RegisterDefaultAllocator();

	m_factory = std::make_unique<JPH::Factory>();
	JPH::Factory::sInstance = m_factory.get();
	JPH::RegisterTypes();

	m_tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(kTempAllocatorSize);
	m_jobSystem = std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers);

	m_broadPhaseLayerInterface = std::make_unique<JPH::BroadPhaseLayerInterfaceTable>(kObjectLayerCount, kBroadPhaseLayerCount);
	for (uint16_t layer = 0; layer < kObjectLayerCount; ++layer)
	{
		m_broadPhaseLayerInterface->MapObjectToBroadPhaseLayer(layer, JPH::BroadPhaseLayer(0));
	}

	m_objectLayerPairFilter = std::make_unique<JPH::ObjectLayerPairFilterTable>(kObjectLayerCount);
	m_objectVsBroadPhaseLayerFilter = std::make_unique<SingleBroadPhaseLayerFilter>();
	m_contactListener = std::make_unique<ContactListener>();

	ApplyLayerSettings(p_settings);

	m_physicsSystem = std::make_unique<JPH::PhysicsSystem>();
	m_physicsSystem->Init(kMaxBodies, kBodyMutexCount, kMaxBodyPairs, kMaxContactConstraints, *m_broadPhaseLayerInterface, *m_objectVsBroadPhaseLayerFilter, *m_objectLayerPairFilter);
	m_physicsSystem->SetContactListener(m_contactListener.get());
	m_physicsSystem->SetGravity(Conversion::ToJoltVector3(p_settings.gravity));

	// Friction and restitution of two bodies in contact are combined by multiplication
	m_physicsSystem->SetCombineFriction([](const JPH::Body& p_body1, const JPH::SubShapeID&, const JPH::Body& p_body2, const JPH::SubShapeID&)
	{
		return p_body1.GetFriction() * p_body2.GetFriction();
	});

	m_physicsSystem->SetCombineRestitution([](const JPH::Body& p_body1, const JPH::SubShapeID&, const JPH::Body& p_body2, const JPH::SubShapeID&)
	{
		return p_body1.GetRestitution() * p_body2.GetRestitution();
	});

	ListenToPhysicalObjects();
}

OvPhysics::Core::PhysicsEngine::~PhysicsEngine()
{
	// A body made or destroyed after this engine is gone must not reach it
	PhysicalObject::CreatedEvent -= m_createdListener;
	PhysicalObject::DestroyedEvent -= m_destroyedListener;

	m_physicsSystem.reset();

	JPH::UnregisterTypes();
	JPH::Factory::sInstance = nullptr;
}

void OvPhysics::Core::PhysicsEngine::PreUpdate()
{
	std::for_each(m_physicalObjects.begin(), m_physicalObjects.end(), std::mem_fn(&PhysicalObject::UpdateBodyTransform));

	ResetCollisionEvents();
}

void OvPhysics::Core::PhysicsEngine::PostUpdate()
{
	for (const auto& [body1, body2] : m_contactListener->ConsumeContacts())
	{
		CollisionCallback(body1, body2);
	}

	std::for_each(m_physicalObjects.begin(), m_physicalObjects.end(), std::mem_fn(&PhysicalObject::UpdateFTransform));

	CheckCollisionStopEvents();
}

bool OvPhysics::Core::PhysicsEngine::Update(float p_deltaTime)
{
	PreUpdate();

	m_accumulatedTime += p_deltaTime;

	if (m_accumulatedTime < kFixedTimeStep)
	{
		return false;
	}

	// Excess steps are dropped to avoid a spiral of death when the simulation can't keep up
	const int stepCount = static_cast<int>(m_accumulatedTime / kFixedTimeStep);
	const int simulatedStepCount = std::min(stepCount, kMaxSubSteps);
	m_accumulatedTime -= stepCount * kFixedTimeStep;

	m_physicsSystem->Update(simulatedStepCount * kFixedTimeStep, simulatedStepCount, m_tempAllocator.get(), m_jobSystem.get());

	PostUpdate();
	return true;
}

std::optional<RaycastHit> OvPhysics::Core::PhysicsEngine::Raycast(OvMaths::FVector3 p_origin, OvMaths::FVector3 p_direction, float p_distance)
{
	if (p_direction == OvMaths::FVector3::Zero)
		return {};

	const JPH::RRayCast ray(Tools::Conversion::ToJoltVector3(p_origin), Tools::Conversion::ToJoltVector3(p_direction * p_distance));
	const JPH::NarrowPhaseQuery& narrowPhaseQuery = m_physicsSystem->GetNarrowPhaseQuery();
	const JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

	// Only physical objects are reported, triggers included: a ragdoll's bodies are none
	const QueryBodyFilter bodyFilter(true);

	RaycastHit resultHit;

	// Try to get First Hit
	JPH::RayCastResult closestHit;

	if (narrowPhaseQuery.CastRay(ray, closestHit, {}, {}, bodyFilter))
	{
		// Get First Hit
		resultHit.FirstResultObject = reinterpret_cast<OvPhysics::Entities::PhysicalObject*>(bodyInterface.GetUserData(closestHit.mBodyID));

		// Try to get all Hit
		JPH::AllHitCollisionCollector<JPH::CastRayCollector> allHitsCollector;
		narrowPhaseQuery.CastRay(ray, JPH::RayCastSettings(), allHitsCollector, {}, {}, bodyFilter);

		// Get all Hit
		for (const JPH::RayCastResult& hit : allHitsCollector.mHits)
			resultHit.ResultObjects.push_back(reinterpret_cast<OvPhysics::Entities::PhysicalObject*>(bodyInterface.GetUserData(hit.mBodyID)));

		return resultHit;
	}
	else
		return {};
}

std::vector<OvPhysics::Entities::PhysicsHit> OvPhysics::Core::PhysicsEngine::CastRay(const OvMaths::FVector3& p_origin, const OvMaths::FVector3& p_direction, float p_distance, const Settings::QuerySettings& p_settings) const
{
	const JPH::Vec3 direction = Conversion::ToJoltVector3(p_direction);

	if (!(p_distance > 0.0f) || !std::isfinite(p_distance) || !IsUsableDirection(direction))
	{
		return {};
	}

	const JPH::RRayCast ray(Conversion::ToJoltVector3(p_origin), direction.Normalized() * p_distance);

	// A convex body is solid: a ray that starts inside it meets it at once
	JPH::AllHitCollisionCollector<JPH::CastRayCollector> collector;
	m_physicsSystem->GetNarrowPhaseQuery().CastRay(
		ray,
		JPH::RayCastSettings(),
		collector,
		{},
		LayerMaskFilter(p_settings.layerMask),
		QueryBodyFilter(p_settings.includeTriggers)
	);

	std::vector<Entities::PhysicsHit> hits;
	hits.reserve(collector.mHits.size());

	for (const JPH::RayCastResult& hit : collector.mHits)
	{
		JPH::BodyLockRead lock(m_physicsSystem->GetBodyLockInterface(), hit.mBodyID);

		if (lock.Succeeded())
		{
			const JPH::Body& body = lock.GetBody();
			const JPH::RVec3 point = ray.GetPointOnRay(hit.mFraction);

			hits.push_back({
				ToPhysicalObject(body.GetUserData()),
				Conversion::ToOvVector3(point),
				Conversion::ToOvVector3(body.GetWorldSpaceSurfaceNormal(hit.mSubShapeID2, point)),
				hit.mFraction * p_distance
			});
		}
	}

	return KeepNearestPerObject(std::move(hits));
}

std::vector<OvPhysics::Entities::PhysicsHit> OvPhysics::Core::PhysicsEngine::CastSphere(const OvMaths::FVector3& p_origin, float p_radius, const OvMaths::FVector3& p_direction, float p_distance, const Settings::QuerySettings& p_settings) const
{
	const JPH::Vec3 direction = Conversion::ToJoltVector3(p_direction);

	if (!(p_radius > 0.0f) || !std::isfinite(p_radius) || !(p_distance > 0.0f) || !std::isfinite(p_distance) || !IsUsableDirection(direction))
	{
		return {};
	}

	const JPH::Vec3 unitDirection = direction.Normalized();

	JPH::SphereShape sphere(p_radius);
	sphere.SetEmbedded();

	const JPH::RShapeCast cast = JPH::RShapeCast::sFromWorldTransform(
		&sphere,
		JPH::Vec3::sOne(),
		JPH::RMat44::sTranslation(Conversion::ToJoltVector3(p_origin)),
		unitDirection * p_distance
	);

	// An object the sphere starts in is met at once, at its deepest point
	JPH::ShapeCastSettings castSettings;
	castSettings.mReturnDeepestPoint = true;

	JPH::AllHitCollisionCollector<JPH::CastShapeCollector> collector;
	m_physicsSystem->GetNarrowPhaseQuery().CastShape(
		cast,
		castSettings,
		JPH::RVec3::sZero(),
		collector,
		{},
		LayerMaskFilter(p_settings.layerMask),
		QueryBodyFilter(p_settings.includeTriggers)
	);

	const JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();

	std::vector<Entities::PhysicsHit> hits;
	hits.reserve(collector.mHits.size());

	for (const JPH::ShapeCastResult& hit : collector.mHits)
	{
		// The penetration axis points into the object met
		const JPH::Vec3 normal = IsUsableDirection(hit.mPenetrationAxis) ? -hit.mPenetrationAxis.Normalized() : -unitDirection;

		hits.push_back({
			ToPhysicalObject(bodyInterface.GetUserData(hit.mBodyID2)),
			Conversion::ToOvVector3(hit.mContactPointOn2),
			Conversion::ToOvVector3(normal),
			hit.mFraction * p_distance
		});
	}

	return KeepNearestPerObject(std::move(hits));
}

std::vector<OvPhysics::Entities::PhysicalObject*> OvPhysics::Core::PhysicsEngine::OverlapSphere(const OvMaths::FVector3& p_center, float p_radius, const Settings::QuerySettings& p_settings) const
{
	if (!(p_radius > 0.0f) || !std::isfinite(p_radius))
	{
		return {};
	}

	JPH::SphereShape sphere(p_radius);
	sphere.SetEmbedded();

	return CollideShape(*m_physicsSystem, sphere, JPH::RMat44::sTranslation(Conversion::ToJoltVector3(p_center)), p_settings);
}

std::vector<OvPhysics::Entities::PhysicalObject*> OvPhysics::Core::PhysicsEngine::OverlapBox(const OvMaths::FVector3& p_center, const OvMaths::FVector3& p_halfExtents, const OvMaths::FQuaternion& p_rotation, const Settings::QuerySettings& p_settings) const
{
	constexpr float kMinHalfExtent = 1e-4f;

	const JPH::Vec3 halfExtents = Conversion::ToJoltVector3(p_halfExtents).Abs();
	if (!std::isfinite(halfExtents.ReduceMax()))
	{
		return {};
	}

	JPH::BoxShape box(JPH::Vec3::sMax(halfExtents, JPH::Vec3::sReplicate(kMinHalfExtent)));
	box.SetEmbedded();

	const JPH::Quat rotation = Conversion::ToJoltQuaternion(p_rotation);
	const float rotationLength = rotation.Length();
	const JPH::Quat unitRotation = std::isfinite(rotationLength) && rotationLength > 1e-6f ? rotation / rotationLength : JPH::Quat::sIdentity();

	return CollideShape(*m_physicsSystem, box, JPH::RMat44::sRotationTranslation(unitRotation, Conversion::ToJoltVector3(p_center)), p_settings);
}

void OvPhysics::Core::PhysicsEngine::SetGravity(const OvMaths::FVector3 & p_gravity)
{
	m_physicsSystem->SetGravity(Conversion::ToJoltVector3(p_gravity));
}

OvMaths::FVector3 OvPhysics::Core::PhysicsEngine::GetGravity() const
{
	return Conversion::ToOvVector3(m_physicsSystem->GetGravity());
}

void OvPhysics::Core::PhysicsEngine::ApplyLayerSettings(const Settings::PhysicsSettings& p_settings)
{
	m_settings.layerNames = p_settings.layerNames;
	m_settings.collisionMasks = p_settings.collisionMasks;
	m_settings.ragdollLayer = std::min(p_settings.ragdollLayer, static_cast<uint16_t>(kObjectLayerCount - 1));
	m_settings.ragdollIgnoresDynamicBodies = p_settings.ragdollIgnoresDynamicBodies;

	// Two layers collide only if each one's mask names the other
	for (uint16_t layer = 0; layer < kObjectLayerCount; ++layer)
	{
		for (uint16_t other = layer; other < kObjectLayerCount; ++other)
		{
			const bool collide =
				(m_settings.collisionMasks[layer] >> other & 1) != 0 &&
				(m_settings.collisionMasks[other] >> layer & 1) != 0;

			SetLayersCollide(layer, other, collide);
		}
	}

	m_contactListener->SetRagdollIgnoresDynamicBodies(m_settings.ragdollIgnoresDynamicBodies);
}

bool OvPhysics::Core::PhysicsEngine::DoLayersCollide(uint16_t p_layer1, uint16_t p_layer2) const
{
	if (p_layer1 >= kObjectLayerCount || p_layer2 >= kObjectLayerCount)
	{
		return false;
	}

	return m_objectLayerPairFilter->ShouldCollide(p_layer1, p_layer2);
}

void OvPhysics::Core::PhysicsEngine::SetLayersCollide(uint16_t p_layer1, uint16_t p_layer2, bool p_collide)
{
	if (p_layer1 >= kObjectLayerCount || p_layer2 >= kObjectLayerCount)
	{
		return;
	}

	if (p_collide)
	{
		m_objectLayerPairFilter->EnableCollision(p_layer1, p_layer2);
	}
	else
	{
		m_objectLayerPairFilter->DisableCollision(p_layer1, p_layer2);
	}
}

std::optional<uint16_t> OvPhysics::Core::PhysicsEngine::FindLayer(const std::string& p_name) const
{
	if (p_name.empty())
	{
		return std::nullopt;
	}

	for (uint16_t layer = 0; layer < kObjectLayerCount; ++layer)
	{
		if (m_settings.layerNames[layer] == p_name)
		{
			return layer;
		}
	}

	return std::nullopt;
}

const std::string& OvPhysics::Core::PhysicsEngine::GetLayerName(uint16_t p_layer) const
{
	static const std::string kNoName;
	return p_layer < kObjectLayerCount ? m_settings.layerNames[p_layer] : kNoName;
}

std::unique_ptr<Ragdoll> OvPhysics::Core::PhysicsEngine::CreateRagdoll(const std::vector<Settings::RagdollPartSettings>& p_parts, const OvMaths::FVector3& p_linearVelocity)
{
	// Each ragdoll has a collision group of its own, within which its parts only collide where they do not touch
	auto ragdoll = std::make_unique<Ragdoll>(*m_physicsSystem, p_parts, p_linearVelocity, m_settings.ragdollLayer, ++m_ragdollCount);
	return ragdoll->IsValid() ? std::move(ragdoll) : nullptr;
}

void OvPhysics::Core::PhysicsEngine::ListenToPhysicalObjects()
{
	m_createdListener = PhysicalObject::CreatedEvent += std::bind(static_cast<void(PhysicsEngine::*)(PhysicalObject&)>(&PhysicsEngine::Consider), this, std::placeholders::_1);
	m_destroyedListener = PhysicalObject::DestroyedEvent += std::bind(static_cast<void(PhysicsEngine::*)(PhysicalObject&)>(&PhysicsEngine::Unconsider), this, std::placeholders::_1);
}

void OvPhysics::Core::PhysicsEngine::Consider(PhysicalObject& p_toConsider)
{
	p_toConsider.m_bodyInterface = &m_physicsSystem->GetBodyInterface();
	m_physicalObjects.push_back(std::ref(p_toConsider));
}

void OvPhysics::Core::PhysicsEngine::Unconsider(PhysicalObject& p_toUnconsider)
{
	{
		auto found = std::find_if(m_physicalObjects.begin(), m_physicalObjects.end(), [&p_toUnconsider](std::reference_wrapper<PhysicalObject> element)
		{
			return std::addressof(p_toUnconsider) == std::addressof(element.get());
		});

		if (found != m_physicalObjects.end())
			m_physicalObjects.erase(found);
	}

	{
		decltype(m_collisionEvents)::iterator iter = m_collisionEvents.begin();
		decltype(m_collisionEvents)::iterator endIter = m_collisionEvents.end();

		for (; iter != endIter; )
		{
			if (iter->first.first == std::addressof(p_toUnconsider) || iter->first.second == std::addressof(p_toUnconsider))
			{
				m_collisionEvents.erase(iter++);
			}
			else {
				++iter;
			}
		}
	}
}

void OvPhysics::Core::PhysicsEngine::ResetCollisionEvents()
{
	for (auto& element : m_collisionEvents)
		element.second = false;
}

void OvPhysics::Core::PhysicsEngine::CheckCollisionStopEvents()
{
	std::vector<std::pair<PhysicalObject*, PhysicalObject*>> stoppedPairs;

	for (const auto& [pair, touched] : m_collisionEvents)
	{
		if (!touched)
		{
			stoppedPairs.push_back(pair);
		}
	}

	// A pair is removed from the collision events when one of its objects is destroyed, possibly by a previous event
	for (const auto& pair : stoppedPairs)
	{
		const auto [object1, object2] = pair;

		if (m_collisionEvents.contains(pair))
		{
			InvokeContactEvent(*object1, *object2, &PhysicalObject::TriggerStopEvent, &PhysicalObject::CollisionStopEvent);
		}

		if (m_collisionEvents.contains(pair))
		{
			InvokeContactEvent(*object2, *object1, &PhysicalObject::TriggerStopEvent, &PhysicalObject::CollisionStopEvent);
		}

		m_collisionEvents.erase(pair);
	}
}

void OvPhysics::Core::PhysicsEngine::CollisionCallback(const JPH::BodyID& p_body1, const JPH::BodyID& p_body2)
{
	// A body destroyed by a previous collision event has no user data anymore
	const JPH::BodyInterface& bodyInterface = m_physicsSystem->GetBodyInterface();
	auto object1 = reinterpret_cast<PhysicalObject*>(bodyInterface.GetUserData(p_body1));
	auto object2 = reinterpret_cast<PhysicalObject*>(bodyInterface.GetUserData(p_body2));

	/* Contacts between two triggers don't generate any event */
	if (!object1 || !object2 || (object1->IsTrigger() && object2->IsTrigger()))
	{
		return;
	}

	const std::pair<PhysicalObject*, PhysicalObject*> pair{ object1, object2 };
	const auto found = m_collisionEvents.find(pair);
	const bool isNewPair = found == m_collisionEvents.end();

	/* The events of this pair have already been invoked during this update */
	if (!isNewPair && found->second)
	{
		return;
	}

	/* The pair is flagged before invoking its events, so that it gets removed if an event destroys one of its objects */
	m_collisionEvents[pair] = true;

	const auto invokeIfAlive = [&pair](PhysicalObject& p_target, PhysicalObject& p_other, PhysicalObjectEvent p_triggerEvent, PhysicalObjectEvent p_collisionEvent)
	{
		if (m_collisionEvents.contains(pair))
		{
			InvokeContactEvent(p_target, p_other, p_triggerEvent, p_collisionEvent);
		}
	};

	if (isNewPair)
	{
		invokeIfAlive(*object1, *object2, &PhysicalObject::TriggerStartEvent, &PhysicalObject::CollisionStartEvent);
		invokeIfAlive(*object2, *object1, &PhysicalObject::TriggerStartEvent, &PhysicalObject::CollisionStartEvent);
	}

	invokeIfAlive(*object1, *object2, &PhysicalObject::TriggerStayEvent, &PhysicalObject::CollisionStayEvent);
	invokeIfAlive(*object2, *object1, &PhysicalObject::TriggerStayEvent, &PhysicalObject::CollisionStayEvent);
}
