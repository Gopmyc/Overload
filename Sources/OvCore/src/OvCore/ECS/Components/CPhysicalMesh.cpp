/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <string>
#include <utility>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CPhysicalMesh.h>

#include <OvPhysics/Entities/PhysicalMesh.h>

using namespace OvPhysics::Entities;

OvCore::ECS::Components::CPhysicalMesh::CPhysicalMesh(ECS::Actor & p_owner) :
	CPhysicalObject(p_owner)
{
	m_physicalObject = std::make_unique<PhysicalMesh>(p_owner.transform.GetFTransform());

	m_physicalObject->SetUserData<std::reference_wrapper<CPhysicalObject>>(*this);

	BindListener();
	Init();
}

std::string OvCore::ECS::Components::CPhysicalMesh::GetName()
{
	return "Physical Mesh";
}

std::string OvCore::ECS::Components::CPhysicalMesh::GetTypeName()
{
	return std::string{ComponentTraits<CPhysicalMesh>::Name};
}

bool OvCore::ECS::Components::CPhysicalMesh::SetTriangles(std::vector<float> p_positions, std::vector<uint32_t> p_indices)
{
	return GetPhysicalObjectAs<PhysicalMesh>().SetTriangles(std::move(p_positions), std::move(p_indices));
}

uint32_t OvCore::ECS::Components::CPhysicalMesh::GetTriangleCount() const
{
	return GetPhysicalObjectAs<PhysicalMesh>().GetTriangleCount();
}

void OvCore::ECS::Components::CPhysicalMesh::OnSerialize(tinyxml2::XMLDocument & p_doc, tinyxml2::XMLNode * p_node)
{
	CPhysicalObject::OnSerialize(p_doc, p_node);
}

void OvCore::ECS::Components::CPhysicalMesh::OnDeserialize(tinyxml2::XMLDocument & p_doc, tinyxml2::XMLNode * p_node)
{
	CPhysicalObject::OnDeserialize(p_doc, p_node);
}

void OvCore::ECS::Components::CPhysicalMesh::OnInspector(OvUI::Internal::WidgetContainer & p_root)
{
	CPhysicalObject::OnInspector(p_root);

	Helpers::GUIDrawer::DrawReadOnlyString(p_root, "Triangles", [this]() { return std::to_string(GetTriangleCount()); });
}
