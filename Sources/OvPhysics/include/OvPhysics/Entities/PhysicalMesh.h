/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "OvPhysics/Entities/PhysicalObject.h"

namespace OvPhysics::Entities
{
	/**
	* Static PhysicalObject shaped by a mesh of triangles, in the transform's space. A triangle collides from its front
	* only, the side its counter-clockwise winding faces: rays and bodies coming from behind pass through it
	*/
	class PhysicalMesh : public PhysicalObject
	{
	public:
		/**
		* PhysicalMesh constructor (Internal transform management)
		*/
		PhysicalMesh();

		/**
		* PhysicalMesh constructor (External transform management)
		* @param p_transform
		*/
		PhysicalMesh(OvMaths::FTransform& p_transform);

		/**
		* Destructor
		*/
		virtual ~PhysicalMesh() override;

		/**
		* Defines the triangles: three floats a vertex (x, y, z), three vertex indices a triangle. No triangles collide
		* with nothing. Returns false, and changes nothing, when the arrays are not whole vertices and triangles or an
		* index has no vertex
		* @param p_positions
		* @param p_indices
		*/
		bool SetTriangles(std::vector<float> p_positions, std::vector<uint32_t> p_indices);

		/**
		* Returns the number of triangles
		*/
		uint32_t GetTriangleCount() const;

	protected:
		virtual bool MustBeStatic() const override;

	private:
		virtual JPH::Shape* CreateShape(const OvMaths::FVector3& p_scale) const override;

	private:
		struct ShapeCache;

		std::vector<float> m_positions;
		std::vector<uint32_t> m_indices;
		std::unique_ptr<ShapeCache> m_shapeCache;
	};
}
