/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <limits>
#include <vector>

#include <OvMaths/FQuaternion.h>
#include <OvMaths/FVector3.h>
#include <OvRendering/Geometry/BoundingSphere.h>

#include <OvCore/Terrain/TerrainMesher.h>

/**
* Models scattered over a height field (grass, flowers, stones), with no scene and no GPU: where each one stands, how
* it turns and how big it is, chunk by chunk, the same for a seed whatever the order the chunks are scattered in
*/
namespace OvCore::Terrain
{
	/**
	* The most instances a square metre takes
	*/
	constexpr float kMaxScatterDensity = 100.0f;

	/**
	* How a layer of models covers a height field
	*/
	struct ScatterSettings
	{
		float density = 1.0f;			// Instances per square metre where the density map weighs 1
		float minScale = 1.0f;
		float maxScale = 1.0f;
		bool randomYaw = true;			// A random turn about the vertical
		float slopeAlignment = 0.0f;	// 0: upright; 1: along the ground's normal
		float maxSlope = 90.0f;			// Degrees from the horizontal beyond which nothing stands
		float minHeight = -10000.0f;	// In the field's space, below which nothing stands
		float maxHeight = 10000.0f;
		uint32_t seed = 0;
	};

	/**
	* A model placed on the field, in the field's own space
	*/
	struct ScatterInstance
	{
		OvMaths::FVector3 position;
		OvMaths::FQuaternion rotation;
		float scale = 1.0f;
	};

	/**
	* Returns the weight, from 0 to 1, of a density map at a point of the field: the map is laid like the heights (its
	* rows along +Z, its columns along +X), stretched over the whole field and read bilinearly. An empty map weighs 1
	* everywhere
	* @param p_map (p_mapSize × p_mapSize weights)
	* @param p_mapSize
	* @param p_u (0 to 1 along X)
	* @param p_v (0 to 1 along Z)
	*/
	float SampleDensity(const std::vector<float>& p_map, uint32_t p_mapSize, float p_u, float p_v);

	/**
	* Scatters one chunk of a height field. A grid of cells of 1 / sqrt(density) metres, anchored at the field's first
	* corner, gives one candidate per cell at a random point of it; a cell belongs to the chunk its corner lies in, so
	* chunks never share or miss one. A candidate stays with the probability of the density map under it, then where the
	* slope and the height allow. Each cell draws its random numbers from the seed and its own coordinates
	* @param p_heights
	* @param p_layout (how the field is cut into chunks)
	* @param p_chunk
	* @param p_settings
	* @param p_densityMap (empty: 1 everywhere)
	* @param p_densityMapSize
	* @param p_instances (cleared, then filled)
	*/
	void ScatterChunk(
		const std::vector<float>& p_heights,
		const TerrainLayout& p_layout,
		uint32_t p_chunk,
		const ScatterSettings& p_settings,
		const std::vector<float>& p_densityMap,
		uint32_t p_densityMapSize,
		std::vector<ScatterInstance>& p_instances
	);

	/**
	* Returns a sphere holding every instance's model, in the field's space, or a sphere of radius 0 for no instance
	* @param p_instances
	* @param p_modelBounds (the bounds of the model an instance draws, in the model's space)
	*/
	OvRendering::Geometry::BoundingSphere BoundInstances(const std::vector<ScatterInstance>& p_instances, const OvRendering::Geometry::BoundingSphere& p_modelBounds);
}
