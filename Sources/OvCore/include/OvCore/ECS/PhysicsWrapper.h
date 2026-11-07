/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <optional>
#include <vector>

#include <OvPhysics/Entities/RaycastHit.h>
#include <OvPhysics/Settings/QuerySettings.h>

#include <OvMaths/FQuaternion.h>
#include <OvMaths/FVector3.h>

#include "OvCore/ECS/Components/CPhysicalObject.h"

namespace OvCore::ECS
{
	/**
	* Simple class that contains wrappers for OvPhysics in an ECS style
	*/
	class PhysicsWrapper
	{
	public:
		/**
		* Simple data structure that wraps the physics RaycastHit with physics components
		*/
		struct RaycastHit
		{
			Components::CPhysicalObject* FirstResultObject = nullptr;
			std::vector<Components::CPhysicalObject*> ResultObjects;
		};

		/* Casts a ray against all Physical Object in the Scene and returns information on what was hit
		 * @param p_origin
		 * @param p_end
		 */
		static std::optional<RaycastHit> Raycast(OvMaths::FVector3 p_origin, OvMaths::FVector3 p_direction, float p_distance);

		/**
		* Where a ray or a cast shape met a physical object
		*/
		struct Hit
		{
			Components::CPhysicalObject* Object = nullptr;
			OvMaths::FVector3 Point;
			OvMaths::FVector3 Normal;
			float Distance = 0.0f;
		};

		/**
		* Casts a ray and returns the objects it meets, nearest first, each once (see PhysicsEngine::CastRay)
		* @param p_origin
		* @param p_direction
		* @param p_distance
		* @param p_settings
		*/
		static std::vector<Hit> CastRay(const OvMaths::FVector3& p_origin, const OvMaths::FVector3& p_direction, float p_distance, const OvPhysics::Settings::QuerySettings& p_settings = {});

		/**
		* Sweeps a sphere and returns the objects it meets, nearest first, each once (see PhysicsEngine::CastSphere)
		* @param p_origin
		* @param p_radius
		* @param p_direction
		* @param p_distance
		* @param p_settings
		*/
		static std::vector<Hit> CastSphere(const OvMaths::FVector3& p_origin, float p_radius, const OvMaths::FVector3& p_direction, float p_distance, const OvPhysics::Settings::QuerySettings& p_settings = {});

		/**
		* Returns the objects that overlap a sphere, each once, in no particular order
		* @param p_center
		* @param p_radius
		* @param p_settings
		*/
		static std::vector<Components::CPhysicalObject*> OverlapSphere(const OvMaths::FVector3& p_center, float p_radius, const OvPhysics::Settings::QuerySettings& p_settings = {});

		/**
		* Returns the objects that overlap a box, each once, in no particular order
		* @param p_center
		* @param p_halfExtents
		* @param p_rotation
		* @param p_settings
		*/
		static std::vector<Components::CPhysicalObject*> OverlapBox(const OvMaths::FVector3& p_center, const OvMaths::FVector3& p_halfExtents, const OvMaths::FQuaternion& p_rotation, const OvPhysics::Settings::QuerySettings& p_settings = {});
	};
}