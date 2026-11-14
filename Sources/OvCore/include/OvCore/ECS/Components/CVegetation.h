/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <OvCore/ECS/Components/AComponent.h>
#include <OvCore/Rendering/InstancingDrawableDescriptor.h>
#include <OvCore/Terrain/Scatter.h>
#include <OvCore/Terrain/TerrainMesher.h>
#include <OvMaths/FMatrix4.h>
#include <OvMaths/FVector3.h>
#include <OvRendering/Geometry/BoundingSphere.h>
#include <OvTools/Eventing/Event.h>

namespace OvRendering::Resources { class Model; }
namespace OvCore::Resources { class Material; }

namespace OvCore::ECS::Components
{
	class CPhysicalHeightField;

	/**
	* Covers the height field of its actor (see CPhysicalHeightField) with models scattered in layers: grass, flowers,
	* stones. Each layer is drawn chunk by chunk of the field, every instance of a chunk in one instanced draw
	*/
	class CVegetation : public AComponent
	{
	public:
		/**
		* The most layers a vegetation takes
		*/
		static constexpr uint32_t kMaxLayers = 8;

		/**
		* A kind of model scattered over the field, and how
		*/
		struct Layer
		{
			std::string name;
			OvRendering::Resources::Model* model = nullptr;
			OvCore::Resources::Material* material = nullptr; // Its shader declares INSTANCING (Standard does)
			Terrain::ScatterSettings scatter;
			float drawDistance = 60.0f;		// From the camera to a chunk, beyond which its instances are not drawn
			bool castShadows = false;
			std::string densityMap;			// An image of the project, or weights a script gives (SetDensityMap)
		};

		/**
		* A chunk of a layer to draw this frame
		*/
		struct LayerDraw
		{
			const OvRendering::Resources::Model* model = nullptr;
			OvCore::Resources::Material* material = nullptr;
			const Rendering::PreparedInstance* instances = nullptr;
			uint32_t count = 0;
			OvRendering::Geometry::BoundingSphere bounds; // In the world
			bool castShadows = false;
		};

		/**
		* Constructor
		* @param p_owner
		*/
		CVegetation(ECS::Actor& p_owner);

		/**
		* Returns the name of the component
		*/
		std::string GetName() override;

		/**
		* Returns the type name of the component
		*/
		virtual std::string GetTypeName() override;

		/**
		* Adds a layer, and returns its index, or nothing when kMaxLayers are there already
		* @param p_layer
		*/
		std::optional<uint32_t> AddLayer(const Layer& p_layer);

		/**
		* Removes a layer: the ones after it move down by one
		* @param p_index
		*/
		void RemoveLayer(uint32_t p_index);

		/**
		* Returns the number of layers
		*/
		uint32_t GetLayerCount() const;

		/**
		* Returns a layer, or nothing for an unknown index
		* @param p_index
		*/
		std::optional<Layer> GetLayer(uint32_t p_index) const;

		/**
		* Replaces a layer's settings: it is scattered again
		* @param p_index
		* @param p_layer
		*/
		void SetLayer(uint32_t p_index, const Layer& p_layer);

		/**
		* Sets a layer's density map from weights, from 0 to 1, laid like the heights: p_size × p_size, row-major along X
		* then Z. Its image density map is left. Says why it refuses, or nothing
		* @param p_index
		* @param p_size (2 to 4096, or 0 to clear the map)
		* @param p_weights
		*/
		std::optional<std::string> SetDensityMap(uint32_t p_index, uint32_t p_size, std::span<const float> p_weights);

		/**
		* Returns what keeps the vegetation or one of its layers from being drawn (no height field, no heights, a layer
		* without a model or with a material that cannot draw instances, a density map that cannot be read), or nothing
		*/
		std::optional<std::string> GetError();

		/**
		* Returns the instances in the chunks within each layer's reach, as the main camera saw them last (before the
		* frustum culls whole chunks), of every layer or of one
		* @param p_layer
		*/
		uint32_t GetInstanceCount(std::optional<uint32_t> p_layer = std::nullopt) const;

		/**
		* Scatters the chunks within reach of a camera, once, and gives the instanced draws: called by the frame builder
		* once a frame and view, where the GPU is ready
		* @param p_viewPosition (nothing: every chunk)
		* @param p_mainView (the view of the scene's main camera, whose counts are reported)
		* @param p_draws
		*/
		void PrepareDraw(const std::optional<OvMaths::FVector3>& p_viewPosition, bool p_mainView, std::vector<LayerDraw>& p_draws);

		/**
		* Lets go of a model about to be unloaded: the layers drawing it lose it
		* @param p_model
		*/
		void ForgetModel(const OvRendering::Resources::Model* p_model);

		/**
		* Lets go of a material about to be unloaded: the layers drawing with it lose it
		* @param p_material
		*/
		void ForgetMaterial(const OvCore::Resources::Material* p_material);

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
		struct LayerChunk
		{
			bool built = false;
			std::vector<Rendering::PreparedInstance> instances;
			OvRendering::Geometry::BoundingSphere bounds;
		};

		struct LayerState
		{
			std::vector<LayerChunk> chunks;
			std::vector<float> scriptMap;	// Weights a script gave
			uint32_t scriptMapSize = 0;
			std::vector<float> imageMap;	// Read from the layer's density map image
			uint32_t imageMapSize = 0;
			std::string loadedImageMap;
			std::optional<std::string> error;
			uint32_t drawnCount = 0;
		};

		CPhysicalHeightField* GetField() const;
		void Refresh();
		void RefreshLayer(uint32_t p_index);
		void BuildChunk(uint32_t p_index, uint32_t p_chunk);
		void BuildLayerWidgets(OvUI::Internal::WidgetContainer& p_container);

	private:
		std::array<Layer, kMaxLayers> m_layers{};
		std::array<LayerState, kMaxLayers> m_states{};
		uint32_t m_layerCount = 0;
		uint32_t m_revision = 0; // Of the layers as set, for the inspector
		uint32_t m_inspectedRevision = 0;
		OvTools::Eventing::Event<> m_layerChangedEvent; // A model or material picked in the inspector

		// The field the chunks were scattered on, and where it stood
		bool m_dirty = true;
		const CPhysicalHeightField* m_builtField = nullptr;
		uint64_t m_builtHeights = 0;
		uint32_t m_builtChunkSize = 0;
		OvMaths::FMatrix4 m_builtWorld;
		Terrain::TerrainLayout m_layout;
		std::vector<Terrain::TerrainBox> m_chunkBoxes; // In the field's space

		std::array<bool, kMaxLayers> m_layerDirty{};
	};

	template<>
	struct ComponentTraits<OvCore::ECS::Components::CVegetation>
	{
		static constexpr std::string_view Name = "class OvCore::ECS::Components::CVegetation";
	};
}
