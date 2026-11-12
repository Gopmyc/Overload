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
	* Static PhysicalObject shaped by a square grid of heights. The sample (x, z) stands at (x * spacing, height,
	* z * spacing) from the transform, and each cell is split along its (x, z)-(x + 1, z + 1) diagonal
	*/
	class PhysicalHeightField : public PhysicalObject
	{
	public:
		/**
		* PhysicalHeightField constructor (Internal transform management)
		*/
		PhysicalHeightField();

		/**
		* PhysicalHeightField constructor (External transform management)
		* @param p_transform
		*/
		PhysicalHeightField(OvMaths::FTransform& p_transform);

		/**
		* Destructor
		*/
		virtual ~PhysicalHeightField() override;

		/**
		* Defines the heights of the field, row-major: along +X first, then +Z. Empty heights collide with nothing, and a
		* height of +infinity is a hole: no triangle touching that sample collides. Returns false, and changes nothing,
		* when there are heights but not p_sampleCount * p_sampleCount of them
		* @param p_sampleCount (samples per side)
		* @param p_spacing (distance between two neighbouring samples)
		* @param p_heights
		*/
		bool SetHeights(uint32_t p_sampleCount, float p_spacing, std::vector<float> p_heights);

		/**
		* Returns the number of samples per side
		*/
		uint32_t GetSampleCount() const;

		/**
		* Returns the distance between two neighbouring samples
		*/
		float GetSpacing() const;

		/**
		* Returns the heights, row-major (empty if the field collides with nothing)
		*/
		const std::vector<float>& GetHeights() const;

	protected:
		virtual bool MustBeStatic() const override;

	private:
		virtual JPH::Shape* CreateShape(const OvMaths::FVector3& p_scale) const override;

	private:
		struct ShapeCache;

		uint32_t m_sampleCount = 0;
		float m_spacing = 1.0f;
		std::vector<float> m_heights;
		std::unique_ptr<ShapeCache> m_shapeCache;
	};
}
