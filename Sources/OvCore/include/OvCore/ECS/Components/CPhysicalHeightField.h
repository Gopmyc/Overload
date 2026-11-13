/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <OvMaths/FVector3.h>

#include "OvCore/ECS/Components/AComponent.h"

#include "OvCore/ECS/Components/CPhysicalObject.h"

namespace OvCore::ECS { class Actor; }

namespace OvCore::ECS::Components
{
	/**
	* Represent a static physical object shaped by a square grid of heights (a terrain): made from a height map image
	* of the project, or from heights a script gives
	*/
	class CPhysicalHeightField : public CPhysicalObject
	{
	public:
		/**
		* Constructor
		* @param p_owner
		*/
		CPhysicalHeightField(ECS::Actor& p_owner);

		/**
		* Returns the name of the component
		*/
		std::string GetName() override;

		/**
		* Returns the type name of the component
		*/
		virtual std::string GetTypeName() override;

		/**
		* Defines the heights of the field, row-major: along +X first, then +Z. The sample (x, z) stands at
		* (x * p_spacing, height, z * p_spacing) from the actor, and each cell is split along its (x, z)-(x + 1, z + 1)
		* diagonal. Empty heights collide with nothing. Returns false, and changes nothing, when there are heights but
		* not p_sampleCount * p_sampleCount of them. Leaves the height map, if any: the field is the script's now
		* @param p_sampleCount (samples per side)
		* @param p_spacing (distance between two neighbouring samples)
		* @param p_heights
		*/
		bool SetHeights(uint32_t p_sampleCount, float p_spacing, std::vector<float> p_heights);

		/**
		* Makes the field from a height map, an image of the project (see Docs/Terrain.md): a pixel per sample, its
		* grey level from black to white giving height offset to height offset + height scale. An empty path leaves
		* the height map, and the field keeps its heights. A map that cannot be read empties the field, and says why
		* in GetHeightmapError
		* @param p_path
		*/
		void SetHeightmap(const std::string& p_path);

		/**
		* Returns the height map the field is made from, or an empty path
		*/
		const std::string& GetHeightmap() const;

		/**
		* Returns why the height map cannot be read, or nothing
		*/
		const std::optional<std::string>& GetHeightmapError() const;

		/**
		* Sets the height of a white pixel of the height map above a black one
		* @param p_scale
		*/
		void SetHeightScale(float p_scale);

		/**
		* Returns the height of a white pixel of the height map above a black one
		*/
		float GetHeightScale() const;

		/**
		* Sets the height of a black pixel of the height map
		* @param p_offset
		*/
		void SetHeightOffset(float p_offset);

		/**
		* Returns the height of a black pixel of the height map
		*/
		float GetHeightOffset() const;

		/**
		* Sets the distance between two neighbouring samples, the heights kept
		* @param p_spacing
		*/
		void SetSpacing(float p_spacing);

		/**
		* Returns a number that changes each time the heights or the spacing change
		*/
		uint64_t GetHeightsRevision() const;

		/**
		* Returns the height of the field's surface under a point of the world (its y), on the triangle the physics
		* uses, or nothing outside the field. Exact for an actor turned about Y only
		* @param p_x
		* @param p_z
		*/
		std::optional<float> GetHeightAt(float p_x, float p_z) const;

		/**
		* Returns the normal of the field's surface under a point of the world, or nothing outside the field
		* @param p_x
		* @param p_z
		*/
		std::optional<OvMaths::FVector3> GetNormalAt(float p_x, float p_z) const;

		/**
		* Returns the number of samples per side
		*/
		uint32_t GetSampleCount() const;

		/**
		* Returns the distance between two neighbouring samples
		*/
		float GetSpacing() const;

		/**
		* Returns the heights, row-major along +X then +Z (empty before SetHeights)
		*/
		const std::vector<float>& GetHeights() const;

		/**
		* Serialize the component (the heights are not: a height map makes them again, or a script)
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
		bool ApplyHeights(uint32_t p_sampleCount, float p_spacing, std::vector<float> p_heights);
		void LoadHeightmap();

	private:
		std::string m_heightmap;
		std::optional<std::string> m_heightmapError;
		float m_heightScale = 10.0f;
		float m_heightOffset = 0.0f;
		uint64_t m_heightsRevision = 0;
	};

	template<>
	struct ComponentTraits<OvCore::ECS::Components::CPhysicalHeightField>
	{
		static constexpr std::string_view Name = "class OvCore::ECS::Components::CPhysicalHeightField";
	};
}
