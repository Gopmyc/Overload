/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <OvCore/ECS/Components/AComponent.h>
#include <OvCore/Terrain/TerrainMesher.h>
#include <OvMaths/FVector3.h>
#include <OvRendering/Geometry/BoundingSphere.h>

namespace OvRendering::Resources { class IMesh; class Mesh; class Texture; }
namespace OvRendering::Data { class Material; }

namespace OvCore::ECS::Components
{
	class CPhysicalHeightField;

	/**
	* Draws the height field of its actor (see CPhysicalHeightField) as a terrain: square chunks, each at a level of
	* detail chosen by its distance to the camera, with skirts that hide the gaps between levels. Its colour comes from
	* a colour map, an image of the project or colours a script gives, laid on the albedo of its material
	*/
	class CTerrain : public AComponent
	{
	public:
		/**
		* A chunk to draw this frame, in the field's own space
		*/
		struct ChunkDraw
		{
			OvRendering::Resources::IMesh* mesh = nullptr;
			OvRendering::Geometry::BoundingSphere bounds;
		};

		/**
		* Constructor
		* @param p_owner
		*/
		CTerrain(ECS::Actor& p_owner);

		/**
		* Destructor
		*/
		~CTerrain();

		/**
		* Returns the name of the component
		*/
		std::string GetName() override;

		/**
		* Returns the type name of the component
		*/
		virtual std::string GetTypeName() override;

		/**
		* Sets the number of cells along a side of a chunk (1 to 256; 32 by default)
		* @param p_cells
		*/
		void SetChunkSize(uint32_t p_cells);

		/**
		* Returns the number of cells along a side of a chunk
		*/
		uint32_t GetChunkSize() const;

		/**
		* Sets the number of levels of detail (1 to 4; 4 by default), as many as a chunk's side can halve
		* @param p_count
		*/
		void SetLodCount(uint32_t p_count);

		/**
		* Returns the number of levels of detail asked for
		*/
		uint32_t GetLodCount() const;

		/**
		* Sets the distance from the camera at which a chunk leaves its full detail; each next level starts twice as
		* far (50 by default)
		* @param p_distance
		*/
		void SetLodDistance(float p_distance);

		/**
		* Returns the distance at which a chunk leaves its full detail
		*/
		float GetLodDistance() const;

		/**
		* Sets the material the terrain is drawn with (":Materials/Terrain.ovmat" by default). Its albedo map gets the
		* colour map, if any
		* @param p_path
		*/
		void SetMaterialPath(const std::string& p_path);

		/**
		* Returns the material the terrain is drawn with
		*/
		const std::string& GetMaterialPath() const;

		/**
		* Sets the colour map, an image of the project stretched over the terrain (its top row along the far edge, +Z).
		* Takes the place of colours a script gave
		* @param p_path
		*/
		void SetColorMap(const std::string& p_path);

		/**
		* Returns the colour map
		*/
		const std::string& GetColorMap() const;

		/**
		* Sets a colour per sample (red, green and blue from 0 to 1, row-major along +X then +Z, as the heights), laid
		* as the colour map. Empty colours remove them. Returns false, and changes nothing, unless there are
		* p_sampleCount * p_sampleCount colours of 3 values
		* @param p_sampleCount
		* @param p_colors
		*/
		bool SetColors(uint32_t p_sampleCount, const std::vector<float>& p_colors);

		/**
		* Returns true when colours a script gave make the colour map
		*/
		bool HasColors() const;

		/**
		* Returns why the terrain draws nothing (no height field on the actor, no heights, no material), or nothing
		*/
		std::optional<std::string> GetError() const;

		/**
		* Returns the number of chunks
		*/
		uint32_t GetChunkCount();

		/**
		* Returns the level of detail the main camera drew a chunk at last, or nothing for an unknown chunk
		* @param p_chunk
		*/
		std::optional<uint32_t> GetChunkLod(uint32_t p_chunk);

		/**
		* Returns the triangles of the surface at the levels the main camera drew last, every chunk counted (the skirts
		* left out)
		*/
		uint32_t GetTriangleCount() const;

		/**
		* Chooses each chunk's level for a camera and gives the meshes to draw, built on demand: called by the frame
		* builder once a frame and view, where the GPU is ready
		* @param p_viewPosition (nothing: the levels drawn last)
		* @param p_mainView (the view of the scene's main camera, whose levels are reported; another view, such as the
		* editor's scene view, keeps levels of its own)
		* @param p_draws
		*/
		void PrepareDraw(const std::optional<OvMaths::FVector3>& p_viewPosition, bool p_mainView, std::vector<ChunkDraw>& p_draws);

		/**
		* Returns the material to draw with, its albedo map the colour map, or nothing
		*/
		OvRendering::Data::Material* ResolveMaterial();

		/**
		* Serialize the component
		* @param p_doc
		* @param p_node
		*/
		virtual void OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node) override;

		/**
		* Deserialize the component
		* @param p_doc
		* @param p_node
		*/
		virtual void OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node) override;

		/**
		* Defines how the component should be drawn in the inspector
		* @param p_root
		*/
		virtual void OnInspector(OvUI::Internal::WidgetContainer& p_root) override;

	private:
		struct Chunk
		{
			Terrain::TerrainBox box;
			float skirtDepth = 0.0f;
			uint32_t lod = 0; // The main view's
			uint32_t otherLod = 0; // Another view's
			std::array<std::unique_ptr<OvRendering::Resources::Mesh>, Terrain::kMaxLodCount> meshes;
			std::array<uint32_t, Terrain::kMaxLodCount> triangles{};
		};

		CPhysicalHeightField* GetField() const;
		void Refresh();
		void ReleaseColorTexture();

	private:
		uint32_t m_chunkSize = 32;
		uint32_t m_lodCount = 4;
		float m_lodDistance = 50.0f;
		std::string m_materialPath;
		std::string m_colorMap;

		// Colours a script gave, as RGBA bytes with their rows from the far edge, as an image loaded for the GPU
		uint32_t m_colorSize = 0;
		std::vector<uint8_t> m_colors;

		// Runtime, made again when the field, its heights or the chunk settings change
		bool m_dirty = true;
		uint64_t m_builtRevision = 0;
		const CPhysicalHeightField* m_builtField = nullptr;
		Terrain::TerrainLayout m_layout;
		std::vector<Chunk> m_chunks;
		uint32_t m_triangleCount = 0;

		std::unique_ptr<OvRendering::Data::Material> m_material;
		std::string m_builtMaterialPath;
		uint64_t m_materialRevision = 0;
		OvRendering::Resources::Texture* m_colorTexture = nullptr; // Made from m_colors
		bool m_colorTextureDirty = false;
		bool m_materialDirty = true;
	};

	template<>
	struct ComponentTraits<OvCore::ECS::Components::CTerrain>
	{
		static constexpr std::string_view Name = "class OvCore::ECS::Components::CTerrain";
	};
}
