/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <format>
#include <numbers>
#include <sstream>

#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CLookAt.h>
#include <OvCore/ECS/Components/CSkinnedMeshRenderer.h>
#include <OvCore/Helpers/GUIDrawer.h>
#include <OvCore/Helpers/Serializer.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvRendering/Animation/SkeletalData.h>
#include <OvUI/Plugins/DataDispatcher.h>
#include <OvUI/Styling/Style.h>
#include <OvUI/Widgets/Texts/TextColored.h>

namespace
{
	using OvMaths::FQuaternion;
	using OvMaths::FVector3;

	constexpr float kRadiansToDegrees = 180.0f / std::numbers::pi_v<float>;

	FVector3 ToModelSpace(const OvCore::ECS::Components::CTransform& p_transform, const FVector3& p_world)
	{
		const FVector3 scale = p_transform.GetWorldScale();
		const FVector3 local = FQuaternion::Conjugate(p_transform.GetWorldRotation()) * (p_world - p_transform.GetWorldPosition());

		return {
			std::abs(scale.x) > 0.000001f ? local.x / scale.x : 0.0f,
			std::abs(scale.y) > 0.000001f ? local.y / scale.y : 0.0f,
			std::abs(scale.z) > 0.000001f ? local.z / scale.z : 0.0f
		};
	}

	std::string Trim(const std::string& p_text)
	{
		const auto first = p_text.find_first_not_of(" \t");
		const auto last = p_text.find_last_not_of(" \t");
		return first == std::string::npos ? std::string{} : p_text.substr(first, last - first + 1);
	}
}

OvCore::ECS::Components::CLookAt::CLookAt(ECS::Actor& p_owner) : AComponent(p_owner)
{
}

std::string OvCore::ECS::Components::CLookAt::GetName()
{
	return "Look At";
}

std::string OvCore::ECS::Components::CLookAt::GetTypeName()
{
	return std::string{ ComponentTraits<CLookAt>::Name };
}

void OvCore::ECS::Components::CLookAt::SetBones(const std::vector<BonePart>& p_bones)
{
	m_bones = p_bones;
	m_bonesText = FormatBones();
}

const std::vector<OvCore::ECS::Components::CLookAt::BonePart>& OvCore::ECS::Components::CLookAt::GetBones() const
{
	return m_bones;
}

void OvCore::ECS::Components::CLookAt::SetTarget(ECS::Actor* p_target)
{
	m_targetGUID = p_target ? p_target->GetGUID() : 0;
	m_targetPosition.reset();
}

uint64_t OvCore::ECS::Components::CLookAt::GetTargetGUID() const
{
	return m_targetGUID;
}

void OvCore::ECS::Components::CLookAt::SetTargetPosition(const FVector3& p_position)
{
	m_targetGUID = 0;
	m_targetPosition = p_position;
}

std::optional<OvMaths::FVector3> OvCore::ECS::Components::CLookAt::GetTargetPosition() const
{
	return m_targetPosition;
}

void OvCore::ECS::Components::CLookAt::ClearTarget()
{
	m_targetGUID = 0;
	m_targetPosition.reset();
}

bool OvCore::ECS::Components::CLookAt::HasTarget() const
{
	return m_targetGUID != 0 || m_targetPosition.has_value();
}

void OvCore::ECS::Components::CLookAt::SetTargetOffset(const FVector3& p_offset)
{
	m_targetOffset = p_offset;
}

const OvMaths::FVector3& OvCore::ECS::Components::CLookAt::GetTargetOffset() const
{
	return m_targetOffset;
}

void OvCore::ECS::Components::CLookAt::SetSettings(const Animation::LookAtSettings& p_settings)
{
	m_settings = p_settings;
	m_settings.maxYaw = std::clamp(m_settings.maxYaw, 0.0f, 180.0f);
	m_settings.minPitch = std::clamp(m_settings.minPitch, -90.0f, 90.0f);
	m_settings.maxPitch = std::clamp(m_settings.maxPitch, -90.0f, 90.0f);
	m_settings.turnTime = std::max(m_settings.turnTime, 0.0f);
	m_settings.maxTurnSpeed = std::max(m_settings.maxTurnSpeed, 0.0f);
}

const OvCore::Animation::LookAtSettings& OvCore::ECS::Components::CLookAt::GetSettings() const
{
	return m_settings;
}

void OvCore::ECS::Components::CLookAt::SetWeight(float p_weight)
{
	m_weight = std::clamp(p_weight, 0.0f, 1.0f);
}

float OvCore::ECS::Components::CLookAt::GetWeight() const
{
	return m_weight;
}

float OvCore::ECS::Components::CLookAt::GetCurrentWeight() const
{
	return m_currentWeight;
}

void OvCore::ECS::Components::CLookAt::SetFadeTime(float p_fadeTime)
{
	m_fadeTime = std::max(p_fadeTime, 0.0f);
}

float OvCore::ECS::Components::CLookAt::GetFadeTime() const
{
	return m_fadeTime;
}

float OvCore::ECS::Components::CLookAt::GetYaw() const
{
	return m_solver.GetYaw() * m_currentWeight * kRadiansToDegrees;
}

float OvCore::ECS::Components::CLookAt::GetPitch() const
{
	return m_solver.GetPitch() * m_currentWeight * kRadiansToDegrees;
}

OvMaths::FVector3 OvCore::ECS::Components::CLookAt::GetLookDirection() const
{
	const FQuaternion look = Animation::LookAtSolver::Rotation(m_solver.GetYaw() * m_currentWeight, m_solver.GetPitch() * m_currentWeight);
	return owner.transform.GetWorldRotation() * (look * FVector3{ 0.0f, 0.0f, 1.0f });
}

bool OvCore::ECS::Components::CLookAt::IsAtRest() const
{
	return m_solver.IsAtRest();
}

void OvCore::ECS::Components::CLookAt::Reset()
{
	m_solver.Reset();
	m_currentWeight = m_weight;
}

const std::optional<std::string>& OvCore::ECS::Components::CLookAt::GetError() const
{
	return m_error;
}

void OvCore::ECS::Components::CLookAt::Apply(const SceneSystem::Scene& p_scene, float p_deltaTime)
{
	m_error.reset();

	auto* renderer = owner.GetComponent<CSkinnedMeshRenderer>();
	const auto* skeleton = renderer ? renderer->GetSkeleton() : nullptr;

	if (!skeleton)
	{
		m_error = "no skinned mesh on the actor";
		return;
	}

	if (m_bones.empty())
	{
		m_error = "no bones to turn";
		return;
	}

	// The bones, from the root to the tip
	struct Part { uint32_t bone; uint32_t node; size_t depth; float weight; };
	std::vector<Part> parts;
	parts.reserve(m_bones.size());

	for (const auto& bonePart : m_bones)
	{
		const auto bone = skeleton->FindBoneIndex(bonePart.bone);
		if (!bone || *bone >= skeleton->bones.size())
		{
			m_error = std::format("no bone '{}'", bonePart.bone);
			return;
		}

		const uint32_t node = skeleton->bones[*bone].nodeIndex;
		size_t depth = 0;
		for (int32_t parent = skeleton->nodes[node].parentIndex; parent >= 0 && depth <= skeleton->nodes.size(); parent = skeleton->nodes[static_cast<size_t>(parent)].parentIndex)
		{
			++depth;
		}

		parts.push_back({ *bone, node, depth, bonePart.weight });
	}

	std::stable_sort(parts.begin(), parts.end(), [](const Part& p_a, const Part& p_b) { return p_a.depth < p_b.depth; });

	for (size_t i = 1; i < parts.size(); ++i)
	{
		bool descends = false;
		for (int32_t node = skeleton->nodes[parts[i].node].parentIndex; node >= 0; node = skeleton->nodes[static_cast<size_t>(node)].parentIndex)
		{
			if (static_cast<uint32_t>(node) == parts[i - 1].node)
			{
				descends = true;
				break;
			}
		}

		if (!descends)
		{
			m_error = std::format("'{}' does not hang under '{}': the bones must make a chain", skeleton->nodes[parts[i].node].name, skeleton->nodes[parts[i - 1].node].name);
			return;
		}
	}

	// Where the target is, seen from the tip, in the model's space
	std::optional<FVector3> targetWorld;
	if (m_targetGUID != 0)
	{
		if (const auto* target = p_scene.FindActorByGUID(m_targetGUID))
		{
			targetWorld = target->transform.GetWorldPosition() + m_targetOffset;
		}
	}
	else if (m_targetPosition)
	{
		targetWorld = *m_targetPosition + m_targetOffset;
	}

	std::optional<FVector3> direction;
	const auto tip = renderer->GetBoneGlobalPosition(parts.back().bone);
	if (targetWorld && tip)
	{
		direction = ToModelSpace(owner.transform, *targetWorld) - *tip;
	}

	const FVector3 forward = owner.transform.GetWorldRotation() * FVector3{ 0.0f, 0.0f, 1.0f };
	m_solver.Step(std::atan2(forward.x, forward.z), direction, p_deltaTime, m_settings);

	if (p_deltaTime > 0.0f)
	{
		const float step = m_fadeTime > 0.0f ? p_deltaTime / m_fadeTime : 1.0f;
		m_currentWeight = m_currentWeight < m_weight ?
			std::min(m_currentWeight + step, m_weight) :
			std::max(m_currentWeight - step, m_weight);
	}

	const float yaw = m_solver.GetYaw() * m_currentWeight;
	const float pitch = m_solver.GetPitch() * m_currentWeight;

	if (std::abs(yaw) < 0.000001f && std::abs(pitch) < 0.000001f)
	{
		return;
	}

	// The pose is read whole before any bone is turned
	std::vector<FQuaternion> globals;
	std::vector<FQuaternion> locals;
	std::vector<float> weights;

	for (const auto& part : parts)
	{
		const auto global = renderer->GetBoneGlobalRotation(part.bone);
		const auto local = renderer->GetBoneLocalRotation(part.bone);
		if (!global || !local)
		{
			return;
		}

		globals.push_back(*global);
		locals.push_back(*local);
		weights.push_back(part.weight);
	}

	std::vector<FQuaternion> result;
	Animation::LookAtSolver::Distribute(globals, locals, weights, yaw, pitch, result);

	for (size_t i = 0; i < parts.size(); ++i)
	{
		renderer->SetConstraintRotation(parts[i].bone, result[i]);
	}
}

std::string OvCore::ECS::Components::CLookAt::FormatBones() const
{
	std::string text;
	for (const auto& part : m_bones)
	{
		text += (text.empty() ? "" : ", ") + part.bone + ":" + std::format("{:g}", part.weight);
	}
	return text;
}

void OvCore::ECS::Components::CLookAt::ParseBones(const std::string& p_text)
{
	std::vector<BonePart> bones;
	std::stringstream stream(p_text);
	std::string entry;

	while (std::getline(stream, entry, ','))
	{
		entry = Trim(entry);
		if (entry.empty())
		{
			continue;
		}

		BonePart part;
		const auto colon = entry.find(':');
		part.bone = Trim(entry.substr(0, colon));

		if (colon != std::string::npos)
		{
			try { part.weight = std::stof(entry.substr(colon + 1)); }
			catch (...) { part.weight = 1.0f; }
		}

		if (!part.bone.empty())
		{
			bones.push_back(part);
		}
	}

	m_bones = bones;
}

void OvCore::ECS::Components::CLookAt::OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	using OvCore::Helpers::Serializer;

	tinyxml2::XMLNode* bonesNode = p_doc.NewElement("bones");
	p_node->InsertEndChild(bonesNode);
	for (const auto& part : m_bones)
	{
		tinyxml2::XMLElement* boneNode = p_doc.NewElement("bone");
		boneNode->SetAttribute("name", part.bone.c_str());
		boneNode->SetAttribute("weight", part.weight);
		bonesNode->InsertEndChild(boneNode);
	}

	Serializer::SerializeFloat(p_doc, p_node, "max_yaw", m_settings.maxYaw);
	Serializer::SerializeFloat(p_doc, p_node, "min_pitch", m_settings.minPitch);
	Serializer::SerializeFloat(p_doc, p_node, "max_pitch", m_settings.maxPitch);
	Serializer::SerializeFloat(p_doc, p_node, "turn_time", m_settings.turnTime);
	Serializer::SerializeFloat(p_doc, p_node, "max_turn_speed", m_settings.maxTurnSpeed);
	Serializer::SerializeFloat(p_doc, p_node, "weight", m_weight);
	Serializer::SerializeFloat(p_doc, p_node, "fade_time", m_fadeTime);
	Serializer::SerializeVec3(p_doc, p_node, "target_offset", m_targetOffset);
}

void OvCore::ECS::Components::CLookAt::OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	using OvCore::Helpers::Serializer;

	m_bones.clear();
	if (tinyxml2::XMLElement* bonesNode = p_node->FirstChildElement("bones"))
	{
		for (auto boneNode = bonesNode->FirstChildElement("bone"); boneNode; boneNode = boneNode->NextSiblingElement("bone"))
		{
			const char* name = boneNode->Attribute("name");
			if (name && *name)
			{
				m_bones.push_back({ name, boneNode->FloatAttribute("weight", 1.0f) });
			}
		}
	}
	m_bonesText = FormatBones();

	Animation::LookAtSettings settings = m_settings;
	Serializer::DeserializeFloat(p_doc, p_node, "max_yaw", settings.maxYaw);
	Serializer::DeserializeFloat(p_doc, p_node, "min_pitch", settings.minPitch);
	Serializer::DeserializeFloat(p_doc, p_node, "max_pitch", settings.maxPitch);
	Serializer::DeserializeFloat(p_doc, p_node, "turn_time", settings.turnTime);
	Serializer::DeserializeFloat(p_doc, p_node, "max_turn_speed", settings.maxTurnSpeed);
	SetSettings(settings);

	float weight = m_weight;
	float fadeTime = m_fadeTime;
	Serializer::DeserializeFloat(p_doc, p_node, "weight", weight);
	Serializer::DeserializeFloat(p_doc, p_node, "fade_time", fadeTime);
	SetWeight(weight);
	SetFadeTime(fadeTime);
	m_currentWeight = m_weight;

	Serializer::DeserializeVec3(p_doc, p_node, "target_offset", m_targetOffset);
}

void OvCore::ECS::Components::CLookAt::OnInspector(OvUI::Internal::WidgetContainer& p_root)
{
	using namespace OvCore::Helpers;

	// As typed: "spine_02:0.2, neck_01:0.3, head:0.5", each bone and its part of the look
	GUIDrawer::DrawString(p_root, "Bones", [this] { return m_bonesText; }, [this](std::string p_text) { m_bonesText = p_text; ParseBones(p_text); });
	GUIDrawer::DrawScalar<float>(p_root, "Max Yaw", [this] { return m_settings.maxYaw; }, [this](float p_value) { auto s = m_settings; s.maxYaw = p_value; SetSettings(s); }, 1.0f, 0.0f, 180.0f);
	GUIDrawer::DrawScalar<float>(p_root, "Min Pitch", [this] { return m_settings.minPitch; }, [this](float p_value) { auto s = m_settings; s.minPitch = p_value; SetSettings(s); }, 1.0f, -90.0f, 90.0f);
	GUIDrawer::DrawScalar<float>(p_root, "Max Pitch", [this] { return m_settings.maxPitch; }, [this](float p_value) { auto s = m_settings; s.maxPitch = p_value; SetSettings(s); }, 1.0f, -90.0f, 90.0f);
	GUIDrawer::DrawScalar<float>(p_root, "Turn Time", [this] { return m_settings.turnTime; }, [this](float p_value) { auto s = m_settings; s.turnTime = p_value; SetSettings(s); }, 0.01f, 0.0f, 10.0f);
	GUIDrawer::DrawScalar<float>(p_root, "Max Turn Speed", [this] { return m_settings.maxTurnSpeed; }, [this](float p_value) { auto s = m_settings; s.maxTurnSpeed = p_value; SetSettings(s); }, 1.0f, 0.0f, 3600.0f);
	GUIDrawer::DrawScalar<float>(p_root, "Weight", [this] { return m_weight; }, [this](float p_value) { SetWeight(p_value); }, 0.01f, 0.0f, 1.0f);
	GUIDrawer::DrawScalar<float>(p_root, "Fade Time", [this] { return m_fadeTime; }, [this](float p_value) { SetFadeTime(p_value); }, 0.01f, 0.0f, 10.0f);
	GUIDrawer::DrawVec3(p_root, "Target Offset", m_targetOffset, 0.01f);

	auto& status = p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>();
	status.AddPlugin<OvUI::Plugins::DataDispatcher<std::string>>().RegisterGatherer([this, &status]
	{
		status.color = m_error ? OVUI_STYLE(Danger) : OVUI_STYLE(TextDisabled);
		return m_error ? *m_error : std::format("Yaw {:.1f}, pitch {:.1f}", GetYaw(), GetPitch());
	});
}
