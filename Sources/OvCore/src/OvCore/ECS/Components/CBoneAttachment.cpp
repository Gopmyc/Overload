/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CBoneAttachment.h>
#include <OvCore/ECS/Components/CSkinnedMeshRenderer.h>
#include <OvCore/Helpers/GUIDrawer.h>
#include <OvCore/Helpers/Serializer.h>
#include <OvCore/SceneSystem/Scene.h>

OvCore::ECS::Components::CBoneAttachment::CBoneAttachment(ECS::Actor& p_owner) :
	AComponent(p_owner)
{
}

std::string OvCore::ECS::Components::CBoneAttachment::GetName()
{
	return "Bone Attachment";
}

std::string OvCore::ECS::Components::CBoneAttachment::GetTypeName()
{
	return std::string{ComponentTraits<CBoneAttachment>::Name};
}

void OvCore::ECS::Components::CBoneAttachment::SetBone(const std::string& p_bone)
{
	m_bone = p_bone;
}

const std::string& OvCore::ECS::Components::CBoneAttachment::GetBone() const
{
	return m_bone;
}

void OvCore::ECS::Components::CBoneAttachment::SetOffsetPosition(const OvMaths::FVector3& p_position)
{
	m_offsetPosition = p_position;
}

const OvMaths::FVector3& OvCore::ECS::Components::CBoneAttachment::GetOffsetPosition() const
{
	return m_offsetPosition;
}

void OvCore::ECS::Components::CBoneAttachment::SetOffsetRotation(const OvMaths::FQuaternion& p_rotation)
{
	m_offsetRotation = p_rotation;
}

const OvMaths::FQuaternion& OvCore::ECS::Components::CBoneAttachment::GetOffsetRotation() const
{
	return m_offsetRotation;
}

void OvCore::ECS::Components::CBoneAttachment::SetOffsetScale(const OvMaths::FVector3& p_scale)
{
	m_offsetScale = p_scale;
}

const OvMaths::FVector3& OvCore::ECS::Components::CBoneAttachment::GetOffsetScale() const
{
	return m_offsetScale;
}

void OvCore::ECS::Components::CBoneAttachment::SetTarget(ECS::Actor* p_target)
{
	m_targetGUID = p_target ? p_target->GetGUID() : 0;
}

uint64_t OvCore::ECS::Components::CBoneAttachment::GetTargetGUID() const
{
	return m_targetGUID;
}

bool OvCore::ECS::Components::CBoneAttachment::IsAttached() const
{
	return m_attached;
}

OvCore::ECS::Components::CSkinnedMeshRenderer* OvCore::ECS::Components::CBoneAttachment::FindRenderer(const SceneSystem::Scene& p_scene) const
{
	if (m_targetGUID != 0)
	{
		auto target = p_scene.FindActorByGUID(m_targetGUID);
		return target ? target->GetComponent<CSkinnedMeshRenderer>() : nullptr;
	}

	for (auto actor = owner.GetParent(); actor; actor = actor->GetParent())
	{
		if (auto renderer = actor->GetComponent<CSkinnedMeshRenderer>())
		{
			return renderer;
		}
	}

	return nullptr;
}

void OvCore::ECS::Components::CBoneAttachment::Apply(const SceneSystem::Scene& p_scene)
{
	m_attached = false;

	if (m_bone.empty())
	{
		return;
	}

	const auto renderer = FindRenderer(p_scene);
	const auto node = renderer ? renderer->FindNodeIndex(m_bone) : std::nullopt;

	OvMaths::FVector3 nodePosition;
	OvMaths::FQuaternion nodeRotation;
	OvMaths::FVector3 nodeScale;

	if (!node || !renderer->GetNodeGlobalTransform(*node, nodePosition, nodeRotation, nodeScale))
	{
		return;
	}

	// The bone in the world: the skinned actor's transform, then the bone's pose in its model, then the offset
	auto& mesh = renderer->owner.transform;
	const auto meshScale = mesh.GetWorldScale();
	const auto boneScale = meshScale * nodeScale;
	const auto bonePosition = mesh.GetWorldPosition() + mesh.GetWorldRotation() * (meshScale * nodePosition);
	const auto boneRotation = mesh.GetWorldRotation() * nodeRotation;

	owner.transform.GetFTransform().GenerateMatricesWorld(
		bonePosition + boneRotation * (boneScale * m_offsetPosition),
		boneRotation * m_offsetRotation,
		boneScale * m_offsetScale
	);

	m_attached = true;
}

void OvCore::ECS::Components::CBoneAttachment::OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	OvCore::Helpers::Serializer::SerializeString(p_doc, p_node, "bone", m_bone);
	OvCore::Helpers::Serializer::SerializeVec3(p_doc, p_node, "offset_position", m_offsetPosition);
	OvCore::Helpers::Serializer::SerializeQuat(p_doc, p_node, "offset_rotation", m_offsetRotation);
	OvCore::Helpers::Serializer::SerializeVec3(p_doc, p_node, "offset_scale", m_offsetScale);
}

void OvCore::ECS::Components::CBoneAttachment::OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	OvCore::Helpers::Serializer::DeserializeString(p_doc, p_node, "bone", m_bone);
	OvCore::Helpers::Serializer::DeserializeVec3(p_doc, p_node, "offset_position", m_offsetPosition);
	OvCore::Helpers::Serializer::DeserializeQuat(p_doc, p_node, "offset_rotation", m_offsetRotation);
	OvCore::Helpers::Serializer::DeserializeVec3(p_doc, p_node, "offset_scale", m_offsetScale);
}

void OvCore::ECS::Components::CBoneAttachment::OnInspector(OvUI::Internal::WidgetContainer& p_root)
{
	using namespace OvCore::Helpers;

	GUIDrawer::DrawString(p_root, "Bone", m_bone);
	GUIDrawer::DrawVec3(p_root, "Offset Position", m_offsetPosition, 0.01f);
	GUIDrawer::DrawQuat(p_root, "Offset Rotation", m_offsetRotation, 1.0f);
	GUIDrawer::DrawVec3(p_root, "Offset Scale", m_offsetScale, 0.01f, 0.0f);
}
