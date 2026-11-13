/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <vector>

#include <OvMaths/FVector3.h>
#include <OvRendering/Geometry/Vertex.h>

namespace OvCore::Terrain
{
	/**
	* The largest number of levels of detail of a terrain chunk
	*/
	constexpr uint32_t kMaxLodCount = 4;

	/**
	* How a height field is cut into square chunks of cells, each drawn at a level of detail: level n keeps one sample
	* in 2^n along each side (and always the chunk's last row and column, so neighbours at the same level meet)
	*/
	struct TerrainLayout
	{
		uint32_t sampleCount = 0;		// Samples per side of the field
		float spacing = 1.0f;			// Between two neighbouring samples
		uint32_t chunkCells = 32;		// Cells per side of a chunk (the last row and column of chunks may have fewer)
		uint32_t chunksPerSide = 0;
		uint32_t lodCount = 1;			// At most kMaxLodCount, and as many as a chunk's side can halve

		/**
		* Returns the layout of a field, with the chunk size brought to 1..256 cells and the level count to what it
		* allows
		* @param p_sampleCount
		* @param p_spacing
		* @param p_chunkCells
		* @param p_lodCount
		*/
		static TerrainLayout Make(uint32_t p_sampleCount, float p_spacing, uint32_t p_chunkCells, uint32_t p_lodCount);

		/**
		* Returns the number of chunks (none for a field of fewer than 2 samples per side)
		*/
		uint32_t GetChunkCount() const;

		/**
		* Returns the first and last sample of a chunk along X and Z
		* @param p_chunk (index, row-major along X then Z)
		* @param p_x0
		* @param p_x1
		* @param p_z0
		* @param p_z1
		*/
		void GetChunkSamples(uint32_t p_chunk, uint32_t& p_x0, uint32_t& p_x1, uint32_t& p_z0, uint32_t& p_z1) const;
	};

	/**
	* A box, in the field's own space
	*/
	struct TerrainBox
	{
		OvMaths::FVector3 min;
		OvMaths::FVector3 max;
	};

	/**
	* The mesh of a chunk at a level of detail: its surface, then a skirt hanging under each edge that hides the gaps
	* between neighbours at different levels
	*/
	struct ChunkGeometry
	{
		std::vector<OvRendering::Geometry::Vertex> vertices;
		std::vector<uint32_t> indices;
		uint32_t surfaceTriangles = 0;	// The triangles of the surface, the skirts left out
	};

	/**
	* Returns how deep the skirts of a chunk hang: twice the largest gap, along its edges, between its full detail and
	* its coarsest level, plus a tenth of a cell
	* @param p_heights (row-major, along +X then +Z)
	* @param p_layout
	* @param p_chunk
	*/
	float ComputeSkirtDepth(const std::vector<float>& p_heights, const TerrainLayout& p_layout, uint32_t p_chunk);

	/**
	* Returns the box of a chunk, its skirts included, in the field's own space
	* @param p_heights
	* @param p_layout
	* @param p_chunk
	* @param p_skirtDepth
	*/
	TerrainBox ComputeChunkBox(const std::vector<float>& p_heights, const TerrainLayout& p_layout, uint32_t p_chunk, float p_skirtDepth);

	/**
	* Builds the mesh of a chunk at a level of detail, in the field's own space. Normals come from the full detail, so
	* the light does not jump between levels; texture coordinates span the whole field, with a pixel centre on each
	* sample for a texture of one pixel per sample, its first row at the far edge (+Z), as an image loaded for the GPU
	* @param p_heights
	* @param p_layout
	* @param p_chunk
	* @param p_lod
	* @param p_skirtDepth
	* @param p_geometry
	*/
	void BuildChunk(const std::vector<float>& p_heights, const TerrainLayout& p_layout, uint32_t p_chunk, uint32_t p_lod, float p_skirtDepth, ChunkGeometry& p_geometry);

	/**
	* Returns the level of detail for a distance: level n up to p_firstDistance * 2^n, the last level beyond. Moving
	* from the previous level needs the distance past the boundary by p_margin (a share of it), so a chunk does not
	* flicker between two levels
	* @param p_distance
	* @param p_firstDistance
	* @param p_lodCount
	* @param p_previous
	* @param p_margin
	*/
	uint32_t SelectLod(float p_distance, float p_firstDistance, uint32_t p_lodCount, uint32_t p_previous, float p_margin = 0.1f);

	/**
	* Returns the distance from a point to a box (0 inside)
	* @param p_point
	* @param p_min
	* @param p_max
	*/
	float DistanceToBox(const OvMaths::FVector3& p_point, const OvMaths::FVector3& p_min, const OvMaths::FVector3& p_max);
}
