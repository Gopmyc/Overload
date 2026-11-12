/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <vector>

#include "OvCore/ECS/Components/AComponent.h"

#include "OvCore/ECS/Components/CPhysicalObject.h"

namespace OvCore::ECS { class Actor; }

namespace OvCore::ECS::Components
{
	/**
	* Represent a static physical object shaped by a square grid of heights (a terrain)
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
		* not p_sampleCount * p_sampleCount of them
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
		* Returns the heights, row-major along +X then +Z (empty before SetHeights)
		*/
		const std::vector<float>& GetHeights() const;

		/**
		* Serialize the component (the heights are not: they are made at run time)
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
	};

	template<>
	struct ComponentTraits<OvCore::ECS::Components::CPhysicalHeightField>
	{
		static constexpr std::string_view Name = "class OvCore::ECS::Components::CPhysicalHeightField";
	};
}
