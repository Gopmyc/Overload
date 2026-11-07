/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <OvMaths/FVector3.h>

namespace OvPhysics::Entities
{
	class PhysicalObject;

	/**
	* Where a ray or a cast shape met a physical object
	*/
	struct PhysicsHit
	{
		PhysicalObject* object = nullptr;

		/**
		* The point met, in world space
		*/
		OvMaths::FVector3 point;

		/**
		* The surface normal of the object met at that point, pointing out of the object
		*/
		OvMaths::FVector3 normal;

		/**
		* How far the cast went from its origin before meeting the object
		*/
		float distance = 0.0f;
	};
}
