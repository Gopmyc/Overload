/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <string>
#include <utility>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CPhysicalHeightField.h>

#include <OvPhysics/Entities/PhysicalHeightField.h>

using namespace OvPhysics::Entities;

OvCore::ECS::Components::CPhysicalHeightField::CPhysicalHeightField(ECS::Actor & p_owner) :
	CPhysicalObject(p_owner)
{
	m_physicalObject = std::make_unique<PhysicalHeightField>(p_owner.transform.GetFTransform());

	m_physicalObject->SetUserData<std::reference_wrapper<CPhysicalObject>>(*this);

	BindListener();
	Init();
}

std::string OvCore::ECS::Components::CPhysicalHeightField::GetName()
{
	return "Physical Height Field";
}

std::string OvCore::ECS::Components::CPhysicalHeightField::GetTypeName()
{
	return std::string{ComponentTraits<CPhysicalHeightField>::Name};
}

bool OvCore::ECS::Components::CPhysicalHeightField::SetHeights(uint32_t p_sampleCount, float p_spacing, std::vector<float> p_heights)
{
	return GetPhysicalObjectAs<PhysicalHeightField>().SetHeights(p_sampleCount, p_spacing, std::move(p_heights));
}

uint32_t OvCore::ECS::Components::CPhysicalHeightField::GetSampleCount() const
{
	return GetPhysicalObjectAs<PhysicalHeightField>().GetSampleCount();
}

float OvCore::ECS::Components::CPhysicalHeightField::GetSpacing() const
{
	return GetPhysicalObjectAs<PhysicalHeightField>().GetSpacing();
}

void OvCore::ECS::Components::CPhysicalHeightField::OnSerialize(tinyxml2::XMLDocument & p_doc, tinyxml2::XMLNode * p_node)
{
	CPhysicalObject::OnSerialize(p_doc, p_node);

	Helpers::Serializer::SerializeInt(p_doc, p_node, "sample_count", static_cast<int>(GetSampleCount()));
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "spacing", GetSpacing());
}

void OvCore::ECS::Components::CPhysicalHeightField::OnDeserialize(tinyxml2::XMLDocument & p_doc, tinyxml2::XMLNode * p_node)
{
	CPhysicalObject::OnDeserialize(p_doc, p_node);

	SetHeights(
		static_cast<uint32_t>(std::max(Helpers::Serializer::DeserializeInt(p_doc, p_node, "sample_count"), 0)),
		Helpers::Serializer::DeserializeFloat(p_doc, p_node, "spacing"),
		{}
	);
}

void OvCore::ECS::Components::CPhysicalHeightField::OnInspector(OvUI::Internal::WidgetContainer & p_root)
{
	CPhysicalObject::OnInspector(p_root);

	Helpers::GUIDrawer::DrawReadOnlyString(p_root, "Sample Count", [this]() { return std::to_string(GetSampleCount()); });
	Helpers::GUIDrawer::DrawReadOnlyString(p_root, "Spacing", [this]() { return std::to_string(GetSpacing()); });
}
