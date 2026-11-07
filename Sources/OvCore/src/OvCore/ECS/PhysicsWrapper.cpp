/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include "OvCore/ECS/PhysicsWrapper.h"

#include "OvCore/Global/ServiceLocator.h"

#include <OvPhysics/Core/PhysicsEngine.h>

std::optional<OvCore::ECS::PhysicsWrapper::RaycastHit> OvCore::ECS::PhysicsWrapper::Raycast(OvMaths::FVector3 p_origin, OvMaths::FVector3 p_direction, float p_distance)
{
	if (auto result = OVSERVICE(OvPhysics::Core::PhysicsEngine).Raycast(p_origin, p_direction, p_distance))
	{
		RaycastHit finalResult;

		finalResult.FirstResultObject = std::addressof(result.value().FirstResultObject->GetUserData<std::reference_wrapper<Components::CPhysicalObject>>().get());
		for (auto object : result.value().ResultObjects)
			finalResult.ResultObjects.push_back(std::addressof(object->GetUserData<std::reference_wrapper<Components::CPhysicalObject>>().get()));

		return finalResult;
	}
	else
		return {};
}

namespace
{
	OvCore::ECS::Components::CPhysicalObject* ToComponent(OvPhysics::Entities::PhysicalObject* p_object)
	{
		return std::addressof(p_object->GetUserData<std::reference_wrapper<OvCore::ECS::Components::CPhysicalObject>>().get());
	}

	std::vector<OvCore::ECS::PhysicsWrapper::Hit> ToComponentHits(const std::vector<OvPhysics::Entities::PhysicsHit>& p_hits)
	{
		std::vector<OvCore::ECS::PhysicsWrapper::Hit> hits;
		hits.reserve(p_hits.size());

		for (const auto& hit : p_hits)
		{
			hits.push_back({ ToComponent(hit.object), hit.point, hit.normal, hit.distance });
		}

		return hits;
	}

	std::vector<OvCore::ECS::Components::CPhysicalObject*> ToComponents(const std::vector<OvPhysics::Entities::PhysicalObject*>& p_objects)
	{
		std::vector<OvCore::ECS::Components::CPhysicalObject*> components;
		components.reserve(p_objects.size());

		for (auto* object : p_objects)
		{
			components.push_back(ToComponent(object));
		}

		return components;
	}
}

std::vector<OvCore::ECS::PhysicsWrapper::Hit> OvCore::ECS::PhysicsWrapper::CastRay(const OvMaths::FVector3& p_origin, const OvMaths::FVector3& p_direction, float p_distance, const OvPhysics::Settings::QuerySettings& p_settings)
{
	return ToComponentHits(OVSERVICE(OvPhysics::Core::PhysicsEngine).CastRay(p_origin, p_direction, p_distance, p_settings));
}

std::vector<OvCore::ECS::PhysicsWrapper::Hit> OvCore::ECS::PhysicsWrapper::CastSphere(const OvMaths::FVector3& p_origin, float p_radius, const OvMaths::FVector3& p_direction, float p_distance, const OvPhysics::Settings::QuerySettings& p_settings)
{
	return ToComponentHits(OVSERVICE(OvPhysics::Core::PhysicsEngine).CastSphere(p_origin, p_radius, p_direction, p_distance, p_settings));
}

std::vector<OvCore::ECS::Components::CPhysicalObject*> OvCore::ECS::PhysicsWrapper::OverlapSphere(const OvMaths::FVector3& p_center, float p_radius, const OvPhysics::Settings::QuerySettings& p_settings)
{
	return ToComponents(OVSERVICE(OvPhysics::Core::PhysicsEngine).OverlapSphere(p_center, p_radius, p_settings));
}

std::vector<OvCore::ECS::Components::CPhysicalObject*> OvCore::ECS::PhysicsWrapper::OverlapBox(const OvMaths::FVector3& p_center, const OvMaths::FVector3& p_halfExtents, const OvMaths::FQuaternion& p_rotation, const OvPhysics::Settings::QuerySettings& p_settings)
{
	return ToComponents(OVSERVICE(OvPhysics::Core::PhysicsEngine).OverlapBox(p_center, p_halfExtents, p_rotation, p_settings));
}
