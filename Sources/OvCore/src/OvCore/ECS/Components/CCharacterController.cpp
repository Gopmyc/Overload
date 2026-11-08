/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CCharacterController.h>

#include <OvPhysics/Settings/PhysicsSettings.h>

using OvPhysics::Entities::CharacterController;

OvCore::ECS::Components::CCharacterController::CCharacterController(ECS::Actor& p_owner) :
	CPhysicalObject(p_owner)
{
	m_physicalObject = std::make_unique<CharacterController>(p_owner.transform.GetFTransform());

	m_physicalObject->SetUserData<std::reference_wrapper<CPhysicalObject>>(*this);

	BindListener();
	Init();
}

std::string OvCore::ECS::Components::CCharacterController::GetName()
{
	return "Character Controller";
}

std::string OvCore::ECS::Components::CCharacterController::GetTypeName()
{
	return std::string{ ComponentTraits<CCharacterController>::Name };
}

CharacterController& OvCore::ECS::Components::CCharacterController::GetController() const
{
	return GetPhysicalObjectAs<CharacterController>();
}

void OvCore::ECS::Components::CCharacterController::SetRadius(float p_radius)
{
	GetController().SetRadius(p_radius);
}

void OvCore::ECS::Components::CCharacterController::SetHeight(float p_height)
{
	GetController().SetHeight(p_height);
}

float OvCore::ECS::Components::CCharacterController::GetRadius() const
{
	return GetController().GetRadius();
}

float OvCore::ECS::Components::CCharacterController::GetHeight() const
{
	return GetController().GetHeight();
}

void OvCore::ECS::Components::CCharacterController::SetMaxSlopeAngle(float p_degrees)
{
	GetController().SetMaxSlopeAngle(p_degrees);
}

float OvCore::ECS::Components::CCharacterController::GetMaxSlopeAngle() const
{
	return GetController().GetMaxSlopeAngle();
}

void OvCore::ECS::Components::CCharacterController::SetStepHeight(float p_height)
{
	GetController().SetStepHeight(p_height);
}

float OvCore::ECS::Components::CCharacterController::GetStepHeight() const
{
	return GetController().GetStepHeight();
}

void OvCore::ECS::Components::CCharacterController::SetGroundSnapDistance(float p_distance)
{
	GetController().SetGroundSnapDistance(p_distance);
}

float OvCore::ECS::Components::CCharacterController::GetGroundSnapDistance() const
{
	return GetController().GetGroundSnapDistance();
}

void OvCore::ECS::Components::CCharacterController::SetGravityFactor(float p_factor)
{
	GetController().SetGravityFactor(p_factor);
}

float OvCore::ECS::Components::CCharacterController::GetGravityFactor() const
{
	return GetController().GetGravityFactor();
}

void OvCore::ECS::Components::CCharacterController::SetPushStrength(float p_strength)
{
	GetController().SetPushStrength(p_strength);
}

float OvCore::ECS::Components::CCharacterController::GetPushStrength() const
{
	return GetController().GetPushStrength();
}

OvMaths::FVector3 OvCore::ECS::Components::CCharacterController::GetRealVelocity() const
{
	return GetController().GetRealVelocity();
}

OvCore::ECS::Components::CCharacterController::EGroundState OvCore::ECS::Components::CCharacterController::GetGroundState() const
{
	return GetController().GetGroundState();
}

bool OvCore::ECS::Components::CCharacterController::IsGrounded() const
{
	return GetController().IsGrounded();
}

OvMaths::FVector3 OvCore::ECS::Components::CCharacterController::GetGroundNormal() const
{
	return GetController().GetGroundNormal();
}

OvMaths::FVector3 OvCore::ECS::Components::CCharacterController::GetGroundPoint() const
{
	return GetController().GetGroundPoint();
}

OvMaths::FVector3 OvCore::ECS::Components::CCharacterController::GetGroundVelocity() const
{
	return GetController().GetGroundVelocity();
}

OvCore::ECS::Components::CPhysicalObject* OvCore::ECS::Components::CCharacterController::GetGroundObject() const
{
	if (auto* ground = GetController().GetGroundObject())
	{
		return &ground->GetUserData<std::reference_wrapper<CPhysicalObject>>().get();
	}

	return nullptr;
}

void OvCore::ECS::Components::CCharacterController::OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	// A body's other settings mean nothing to a character: they are left out
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "mass", GetMass());
	Helpers::Serializer::SerializeInt(p_doc, p_node, "collision_layer", static_cast<int>(GetCollisionLayer()));
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "radius", GetRadius());
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "height", GetHeight());
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "max_slope_angle", GetMaxSlopeAngle());
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "step_height", GetStepHeight());
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "ground_snap_distance", GetGroundSnapDistance());
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "gravity_factor", GetGravityFactor());
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "push_strength", GetPushStrength());
}

void OvCore::ECS::Components::CCharacterController::OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	// A setting missing from the file keeps its default
	const auto readFloat = [&](const char* p_name, float p_value, auto p_setter)
	{
		Helpers::Serializer::DeserializeFloat(p_doc, p_node, p_name, p_value);
		(this->*p_setter)(p_value);
	};

	readFloat("mass", GetMass(), &CCharacterController::SetMass);
	readFloat("radius", GetRadius(), &CCharacterController::SetRadius);
	readFloat("height", GetHeight(), &CCharacterController::SetHeight);
	readFloat("max_slope_angle", GetMaxSlopeAngle(), &CCharacterController::SetMaxSlopeAngle);
	readFloat("step_height", GetStepHeight(), &CCharacterController::SetStepHeight);
	readFloat("ground_snap_distance", GetGroundSnapDistance(), &CCharacterController::SetGroundSnapDistance);
	readFloat("gravity_factor", GetGravityFactor(), &CCharacterController::SetGravityFactor);
	readFloat("push_strength", GetPushStrength(), &CCharacterController::SetPushStrength);

	int layer = static_cast<int>(GetCollisionLayer());
	Helpers::Serializer::DeserializeInt(p_doc, p_node, "collision_layer", layer);
	SetCollisionLayer(static_cast<uint16_t>(std::clamp(layer, 0, OvPhysics::Settings::PhysicsSettings::kLayerCount - 1)));
}

void OvCore::ECS::Components::CCharacterController::OnInspector(OvUI::Internal::WidgetContainer& p_root)
{
	Helpers::GUIDrawer::DrawScalar<float>(p_root, "Radius", std::bind(&CCharacterController::GetRadius, this), std::bind(&CCharacterController::SetRadius, this, std::placeholders::_1), 0.01f, 0.f, 100000.f);
	Helpers::GUIDrawer::DrawScalar<float>(p_root, "Height", std::bind(&CCharacterController::GetHeight, this), std::bind(&CCharacterController::SetHeight, this, std::placeholders::_1), 0.01f, 0.f, 100000.f);
	Helpers::GUIDrawer::DrawScalar<float>(p_root, "Max Slope Angle", std::bind(&CCharacterController::GetMaxSlopeAngle, this), std::bind(&CCharacterController::SetMaxSlopeAngle, this, std::placeholders::_1), 0.5f, 0.f, 90.f);
	Helpers::GUIDrawer::DrawScalar<float>(p_root, "Step Height", std::bind(&CCharacterController::GetStepHeight, this), std::bind(&CCharacterController::SetStepHeight, this, std::placeholders::_1), 0.01f, 0.f, 100.f);
	Helpers::GUIDrawer::DrawScalar<float>(p_root, "Ground Snap Distance", std::bind(&CCharacterController::GetGroundSnapDistance, this), std::bind(&CCharacterController::SetGroundSnapDistance, this, std::placeholders::_1), 0.01f, 0.f, 100.f);
	Helpers::GUIDrawer::DrawScalar<float>(p_root, "Gravity Factor", std::bind(&CCharacterController::GetGravityFactor, this), std::bind(&CCharacterController::SetGravityFactor, this, std::placeholders::_1), 0.05f, -100.f, 100.f);
	Helpers::GUIDrawer::DrawScalar<float>(p_root, "Mass", std::bind(&CCharacterController::GetMass, this), std::bind(&CCharacterController::SetMass, this, std::placeholders::_1), 0.1f, 0.f, 10000.f);
	Helpers::GUIDrawer::DrawScalar<float>(p_root, "Push Strength", std::bind(&CCharacterController::GetPushStrength, this), std::bind(&CCharacterController::SetPushStrength, this, std::placeholders::_1), 1.f, 0.f, 100000.f);

	DrawCollisionLayer(p_root);
}
