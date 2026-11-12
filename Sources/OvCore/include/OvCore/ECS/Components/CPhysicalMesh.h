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
	* Represent a static physical object shaped by a mesh of triangles (a cave, a rock): each triangle collides from its
	* front only, the side its counter-clockwise winding faces
	*/
	class CPhysicalMesh : public CPhysicalObject
	{
	public:
		/**
		* Constructor
		* @param p_owner
		*/
		CPhysicalMesh(ECS::Actor& p_owner);

		/**
		* Returns the name of the component
		*/
		std::string GetName() override;

		/**
		* Returns the type name of the component
		*/
		virtual std::string GetTypeName() override;

		/**
		* Defines the triangles, in the actor's space: three floats a vertex (x, y, z), three vertex indices a triangle.
		* No triangles collide with nothing. Returns false, and changes nothing, when the arrays are not whole vertices
		* and triangles or an index has no vertex
		* @param p_positions
		* @param p_indices
		*/
		bool SetTriangles(std::vector<float> p_positions, std::vector<uint32_t> p_indices);

		/**
		* Returns the number of triangles
		*/
		uint32_t GetTriangleCount() const;

		/**
		* Serialize the component (the triangles are not: they are made at run time)
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
	struct ComponentTraits<OvCore::ECS::Components::CPhysicalMesh>
	{
		static constexpr std::string_view Name = "class OvCore::ECS::Components::CPhysicalMesh";
	};
}
