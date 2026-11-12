/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>

#include <OvCore/Navigation/NavMesh.h>
#include <OvMaths/FQuaternion.h>
#include <OvMaths/FVector3.h>

namespace OvCore::SceneSystem { class Scene; }

namespace OvCore::Navigation::SceneGeometry
{
	/**
	* Returns the triangles of a scene's static colliders, in the world: those of active actors, not triggers, in the
	* layers of the mask. What characters walk on and bump into
	* @param p_scene
	* @param p_layerMask (a bit per collision layer)
	*/
	NavGeometry Collect(SceneSystem::Scene& p_scene, uint32_t p_layerMask);

	/**
	* Adds a box, as large as its half extents each way
	* @param p_geometry
	* @param p_center
	* @param p_rotation
	* @param p_halfExtents
	*/
	void AddBox(NavGeometry& p_geometry, const OvMaths::FVector3& p_center, const OvMaths::FQuaternion& p_rotation, const OvMaths::FVector3& p_halfExtents);

	/**
	* Adds a capsule along its Y axis, a cylinder of the given height between two half spheres (a sphere for a
	* height of 0), slightly larger than the shape so that it holds it
	* @param p_geometry
	* @param p_center
	* @param p_rotation
	* @param p_radius
	* @param p_cylinderHeight
	*/
	void AddCapsule(NavGeometry& p_geometry, const OvMaths::FVector3& p_center, const OvMaths::FQuaternion& p_rotation, float p_radius, float p_cylinderHeight);

	/**
	* Adds a height field's grid, each cell cut along its (x, z)–(x + 1, z + 1) diagonal as Jolt does
	* @param p_geometry
	* @param p_world (the actor's world matrix)
	* @param p_sampleCount
	* @param p_spacing
	* @param p_heights
	*/
	void AddHeightField(NavGeometry& p_geometry, const OvMaths::FMatrix4& p_world, uint32_t p_sampleCount, float p_spacing, const std::vector<float>& p_heights);
}
