/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

#include <OvMaths/FMatrix4.h>
#include <OvMaths/FVector3.h>
#include <OvRendering/Animation/SkeletalData.h>
#include <OvRendering/Geometry/BoundingSphere.h>

/**
* Levels of detail of a model, with no scene and no GPU: which level a distance picks, where the distance is measured
* from, and whether a lighter model can share the pose of the model it stands for
*/
namespace OvCore::Rendering::LodSelection
{
	/**
	* The levels a model renderer takes: its own model, and up to 7 lighter ones
	*/
	constexpr uint32_t kMaxLevels = 8;

	/**
	* The share of a boundary the distance must pass it by to change level, so a model does not flicker between two
	*/
	constexpr float kMargin = 0.1f;

	/**
	* Returns the level for a distance. Level n (from 1) starts at p_starts[n - 1] times p_bias, the starts rising; level
	* 0, the model itself, is nearer than the first start. Leaving p_previous needs the distance past the boundary by
	* p_margin (a share of it)
	* @param p_distance
	* @param p_starts (the distance each lighter level starts at, rising)
	* @param p_bias (above 1 keeps the detail further away, below 1 lets it go nearer)
	* @param p_previous (the level chosen last)
	* @param p_margin
	*/
	uint32_t SelectLevel(float p_distance, std::span<const float> p_starts, float p_bias, uint32_t p_previous, float p_margin = kMargin);

	/**
	* Returns the distance from a point to the centre of a model's bounds, carried into the world: a model's pivot can
	* stand far from its geometry
	* @param p_point
	* @param p_world
	* @param p_bounds (in the model's space)
	*/
	float DistanceToBounds(const OvMaths::FVector3& p_point, const OvMaths::FMatrix4& p_world, const OvRendering::Geometry::BoundingSphere& p_bounds);

	/**
	* Says why a level cannot be drawn with the pose of the model it stands for, or nothing when it can: a model with
	* bones takes levels with the same bones, in the same order
	* @param p_model (the skeleton of the renderer's own model)
	* @param p_level (the skeleton of the lighter model)
	*/
	std::optional<std::string> CompareBones(const std::optional<OvRendering::Animation::Skeleton>& p_model, const std::optional<OvRendering::Animation::Skeleton>& p_level);
}
