/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>

#include <OvMaths/FQuaternion.h>
#include <OvMaths/FVector3.h>

namespace OvPhysics::Settings
{
	/**
	* One body of a ragdoll: a capsule standing at the joint to its parent, and how far it may turn away from it
	*/
	struct RagdollPartSettings
	{
		/* Index of the part it hangs from, lower than its own, or -1 for the root */
		int32_t parent = -1;

		/* Where the body stands, at the joint to its parent (world space) */
		OvMaths::FVector3 position = OvMaths::FVector3::Zero;
		OvMaths::FQuaternion rotation = OvMaths::FQuaternion::Identity;

		/* Ends of the capsule's axis, in the body's own space */
		OvMaths::FVector3 capsuleStart = OvMaths::FVector3::Zero;
		OvMaths::FVector3 capsuleEnd = OvMaths::FVector3::Zero;

		float radius = 0.05f;
		float mass = 1.0f;

		/* Radians the part may swing away from its parent, and twist about its own axis either way */
		float swingLimit = 0.5f;
		float twistLimit = 0.3f;
	};
}
