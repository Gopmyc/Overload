/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <format>
#include <memory>
#include <sstream>

#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CModelRenderer.h>
#include <OvCore/ECS/Components/CSkinnedMeshRenderer.h>
#include <OvCore/ECS/Components/CSpringBones.h>
#include <OvCore/Helpers/GUIDrawer.h>
#include <OvCore/Helpers/Serializer.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvRendering/Animation/SkeletalData.h>
#include <OvUI/Plugins/DataDispatcher.h>
#include <OvUI/Styling/Style.h>
#include <OvUI/Widgets/Buttons/Button.h>
#include <OvUI/Widgets/Layout/Columns.h>
#include <OvUI/Widgets/Layout/Group.h>
#include <OvUI/Widgets/Layout/TreeNode.h>
#include <OvUI/Widgets/Texts/TextColored.h>

namespace
{
	using OvMaths::FMatrix4;
	using OvMaths::FQuaternion;
	using OvMaths::FVector3;

	constexpr float kMinTeleport = 1.0f;
	constexpr float kTeleportSpeed = 25.0f;

	std::string Trim(const std::string& p_text)
	{
		const auto first = p_text.find_first_not_of(" \t");
		const auto last = p_text.find_last_not_of(" \t");
		return first == std::string::npos ? std::string{} : p_text.substr(first, last - first + 1);
	}

	std::vector<std::string> SplitNames(const std::string& p_text)
	{
		std::vector<std::string> names;
		std::stringstream stream(p_text);
		std::string entry;

		while (std::getline(stream, entry, ','))
		{
			if (entry = Trim(entry); !entry.empty())
			{
				names.push_back(entry);
			}
		}

		return names;
	}

	std::string JoinNames(const std::vector<std::string>& p_names)
	{
		std::string text;
		for (const auto& name : p_names)
		{
			text += (text.empty() ? "" : ", ") + name;
		}
		return text;
	}

	FVector3 Translation(const FMatrix4& p_matrix)
	{
		return { p_matrix.data[3], p_matrix.data[7], p_matrix.data[11] };
	}

	void SerializeSettings(tinyxml2::XMLElement* p_node, const OvCore::Animation::SpringSettings& p_settings)
	{
		p_node->SetAttribute("stiffness", p_settings.stiffness);
		p_node->SetAttribute("damping", p_settings.damping);
		p_node->SetAttribute("gravity", p_settings.gravity);
		p_node->SetAttribute("radius", p_settings.radius);
		p_node->SetAttribute("max_angle", p_settings.maxAngle);
		p_node->SetAttribute("link", p_settings.link);
	}

	OvCore::Animation::SpringSettings DeserializeSettings(const tinyxml2::XMLElement* p_node)
	{
		OvCore::Animation::SpringSettings settings;
		settings.stiffness = p_node->FloatAttribute("stiffness", settings.stiffness);
		settings.damping = p_node->FloatAttribute("damping", settings.damping);
		settings.gravity = p_node->FloatAttribute("gravity", settings.gravity);
		settings.radius = p_node->FloatAttribute("radius", settings.radius);
		settings.maxAngle = p_node->FloatAttribute("max_angle", settings.maxAngle);
		settings.link = p_node->FloatAttribute("link", settings.link);
		return settings;
	}

	OvCore::Animation::SpringSettings Sanitize(OvCore::Animation::SpringSettings p_settings)
	{
		p_settings.stiffness = std::max(p_settings.stiffness, 0.0f);
		p_settings.damping = std::max(p_settings.damping, 0.0f);
		p_settings.radius = std::max(p_settings.radius, 0.0f);
		p_settings.maxAngle = std::clamp(p_settings.maxAngle, 0.0f, 180.0f);
		p_settings.link = std::clamp(p_settings.link, 0.0f, 1.0f);
		return p_settings;
	}
}

OvCore::ECS::Components::CSpringBones::CSpringBones(ECS::Actor& p_owner) : AComponent(p_owner)
{
}

std::string OvCore::ECS::Components::CSpringBones::GetName()
{
	return "Spring Bones";
}

std::string OvCore::ECS::Components::CSpringBones::GetTypeName()
{
	return std::string{ ComponentTraits<CSpringBones>::Name };
}

void OvCore::ECS::Components::CSpringBones::SetGroup(const std::string& p_name, const Animation::SpringSettings& p_settings)
{
	const auto found = std::find_if(m_groups.begin(), m_groups.end(), [&](const Group& p_group) { return p_group.name == p_name; });

	if (found != m_groups.end())
	{
		found->settings = Sanitize(p_settings);

		// Settings alone need no rebuild: the solver reads them each step
		for (size_t i = 0; i < m_runtimeGroups.size() && i < m_groups.size(); ++i)
		{
			m_runtimeGroups[i].group.settings = m_groups[i].settings;
		}

		return;
	}

	m_groups.push_back({ p_name, Sanitize(p_settings), {}, {} });
	MarkDirty();
}

bool OvCore::ECS::Components::CSpringBones::RemoveGroup(const std::string& p_name)
{
	const auto found = std::find_if(m_groups.begin(), m_groups.end(), [&](const Group& p_group) { return p_group.name == p_name; });
	if (found == m_groups.end())
	{
		return false;
	}

	m_groups.erase(found);
	MarkDirty();
	return true;
}

const std::vector<OvCore::ECS::Components::CSpringBones::Group>& OvCore::ECS::Components::CSpringBones::GetGroups() const
{
	return m_groups;
}

const OvCore::ECS::Components::CSpringBones::Group* OvCore::ECS::Components::CSpringBones::FindGroup(const std::string& p_name) const
{
	const auto found = std::find_if(m_groups.begin(), m_groups.end(), [&](const Group& p_group) { return p_group.name == p_name; });
	return found != m_groups.end() ? &*found : nullptr;
}

bool OvCore::ECS::Components::CSpringBones::SetGroupChains(const std::string& p_group, const std::vector<std::string>& p_chains)
{
	auto group = std::find_if(m_groups.begin(), m_groups.end(), [&](const Group& p_candidate) { return p_candidate.name == p_group; });
	if (group == m_groups.end())
	{
		return false;
	}

	group->chains = p_chains;
	MarkDirty();
	return true;
}

bool OvCore::ECS::Components::CSpringBones::SetGroupColliders(const std::string& p_group, const std::vector<std::string>& p_colliders)
{
	auto group = std::find_if(m_groups.begin(), m_groups.end(), [&](const Group& p_candidate) { return p_candidate.name == p_group; });
	if (group == m_groups.end())
	{
		return false;
	}

	group->colliders = p_colliders;
	MarkDirty();
	return true;
}

void OvCore::ECS::Components::CSpringBones::SetCollider(const Collider& p_collider)
{
	Collider collider = p_collider;
	collider.radius = std::max(collider.radius, 0.0f);

	const auto found = std::find_if(m_colliders.begin(), m_colliders.end(), [&](const Collider& p_candidate) { return p_candidate.name == collider.name; });
	if (found != m_colliders.end())
	{
		*found = collider;
	}
	else
	{
		m_colliders.push_back(collider);
	}

	MarkDirty();
}

bool OvCore::ECS::Components::CSpringBones::RemoveCollider(const std::string& p_name)
{
	const auto found = std::find_if(m_colliders.begin(), m_colliders.end(), [&](const Collider& p_candidate) { return p_candidate.name == p_name; });
	if (found == m_colliders.end())
	{
		return false;
	}

	m_colliders.erase(found);
	MarkDirty();
	return true;
}

const std::vector<OvCore::ECS::Components::CSpringBones::Collider>& OvCore::ECS::Components::CSpringBones::GetColliders() const
{
	return m_colliders;
}

void OvCore::ECS::Components::CSpringBones::SetGround(bool p_enabled, float p_offset)
{
	m_groundEnabled = p_enabled;
	m_groundOffset = p_offset;
}

bool OvCore::ECS::Components::CSpringBones::IsGroundEnabled() const
{
	return m_groundEnabled;
}

float OvCore::ECS::Components::CSpringBones::GetGroundOffset() const
{
	return m_groundOffset;
}

void OvCore::ECS::Components::CSpringBones::SetSleepWhenHidden(bool p_value)
{
	m_sleepWhenHidden = p_value;
}

bool OvCore::ECS::Components::CSpringBones::GetSleepWhenHidden() const
{
	return m_sleepWhenHidden;
}

void OvCore::ECS::Components::CSpringBones::Reset()
{
	m_resetPending = true;
}

bool OvCore::ECS::Components::CSpringBones::IsAsleep() const
{
	return m_asleep;
}

uint32_t OvCore::ECS::Components::CSpringBones::GetChainCount() const
{
	uint32_t count = 0;
	for (const auto& group : m_runtimeGroups)
	{
		count += static_cast<uint32_t>(group.group.chains.size());
	}
	return count;
}

std::optional<OvMaths::FVector3> OvCore::ECS::Components::CSpringBones::GetJointPosition(uint32_t p_chain, uint32_t p_bone) const
{
	for (const auto& group : m_runtimeGroups)
	{
		if (p_chain < group.group.chains.size())
		{
			const auto& chain = group.group.chains[p_chain];
			return p_bone < chain.positions.size() ? std::optional<FVector3>{ chain.positions[p_bone] } : std::nullopt;
		}

		p_chain -= static_cast<uint32_t>(group.group.chains.size());
	}

	return std::nullopt;
}

const std::optional<std::string>& OvCore::ECS::Components::CSpringBones::GetError() const
{
	return m_error;
}

void OvCore::ECS::Components::CSpringBones::MarkDirty()
{
	m_dirty = true;
	m_resetPending = true;
}

bool OvCore::ECS::Components::CSpringBones::Build(const CSkinnedMeshRenderer& p_renderer)
{
	m_runtimeGroups.clear();
	m_runtimeColliders.clear();
	m_error.reset();
	m_resetPending = true;

	const auto* skeleton = p_renderer.GetSkeleton();
	if (!skeleton)
	{
		return false;
	}

	const auto& nodes = skeleton->nodes;

	// The rest pose in the model's space: the importer lists a node after its parent
	std::vector<FMatrix4> rest(nodes.size(), FMatrix4::Identity);
	for (size_t i = 0; i < nodes.size(); ++i)
	{
		const int32_t parent = nodes[i].parentIndex;
		rest[i] = parent >= 0 && static_cast<size_t>(parent) < i ? rest[static_cast<size_t>(parent)] * nodes[i].localBindTransform : nodes[i].localBindTransform;
	}

	// Each node's child bones
	std::vector<std::vector<uint32_t>> childBones(nodes.size());
	for (uint32_t bone = 0; bone < skeleton->bones.size(); ++bone)
	{
		const int32_t parent = nodes[skeleton->bones[bone].nodeIndex].parentIndex;
		if (parent >= 0)
		{
			childBones[static_cast<size_t>(parent)].push_back(bone);
		}
	}

	const auto fail = [this](std::string p_error)
	{
		if (!m_error)
		{
			m_error = std::move(p_error);
		}
	};

	// Colliders, by name
	std::vector<std::optional<size_t>> runtimeByCollider(m_colliders.size());
	for (size_t c = 0; c < m_colliders.size(); ++c)
	{
		const auto& collider = m_colliders[c];
		const bool away = collider.to.empty();
		const auto from = skeleton->FindBoneIndex(collider.from);
		const auto other = skeleton->FindBoneIndex(away ? collider.away : collider.to);

		if (!from || !other)
		{
			fail(std::format("collider '{}': no bone '{}'", collider.name, !from ? collider.from : (away ? collider.away : collider.to)));
			continue;
		}

		runtimeByCollider[c] = m_runtimeColliders.size();
		m_runtimeColliders.push_back({ *from, *other, away, collider.start, collider.end, collider.radius, {} });
	}

	for (const auto& group : m_groups)
	{
		RuntimeGroup runtime;
		runtime.group.settings = group.settings;

		for (const auto& root : group.chains)
		{
			const auto rootBone = skeleton->FindBoneIndex(root);
			if (!rootBone)
			{
				fail(std::format("group '{}': no bone '{}'", group.name, root));
				continue;
			}

			// Down the only child bone, bone after bone
			std::vector<uint32_t> bones{ *rootBone };
			for (size_t guard = 0; guard < skeleton->bones.size(); ++guard)
			{
				const auto& children = childBones[skeleton->bones[bones.back()].nodeIndex];
				if (children.size() != 1)
				{
					break;
				}
				bones.push_back(children.front());
			}

			if (bones.size() < 2)
			{
				fail(std::format("group '{}': the chain from '{}' has a single bone", group.name, root));
				continue;
			}

			Animation::SpringChain chain;
			for (size_t i = 0; i < bones.size(); ++i)
			{
				const uint32_t node = skeleton->bones[bones[i]].nodeIndex;
				chain.binds.push_back(FQuaternion::Normalize(nodes[node].bindRotation));

				if (i + 1 < bones.size())
				{
					const uint32_t next = skeleton->bones[bones[i + 1]].nodeIndex;
					chain.lengths.push_back(FVector3::Distance(Translation(rest[node]), Translation(rest[next])));

					const float localLength = FVector3::Length(nodes[next].bindPosition);
					chain.axes.push_back(localLength > 0.000001f ? nodes[next].bindPosition / localLength : FVector3{ 0.0f, 1.0f, 0.0f });
				}
				else
				{
					// The last bone goes on past its joint as long as the one before, the same way in its own frame
					chain.lengths.push_back(chain.lengths.back());
					chain.axes.push_back(chain.axes.back());
				}
			}

			runtime.group.chains.push_back(std::move(chain));
			runtime.bones.push_back(std::move(bones));
		}

		for (const auto& name : group.colliders)
		{
			const auto found = std::find_if(m_colliders.begin(), m_colliders.end(), [&](const Collider& p_collider) { return p_collider.name == name; });
			if (found == m_colliders.end())
			{
				fail(std::format("group '{}': no collider '{}'", group.name, name));
				continue;
			}

			if (const auto runtimeIndex = runtimeByCollider[static_cast<size_t>(std::distance(m_colliders.begin(), found))])
			{
				runtime.colliders.push_back(*runtimeIndex);
			}
		}

		m_runtimeGroups.push_back(std::move(runtime));
	}

	return true;
}

void OvCore::ECS::Components::CSpringBones::Apply(const SceneSystem::Scene&, float p_deltaTime)
{
	auto* renderer = owner.GetComponent<CSkinnedMeshRenderer>();
	if (!renderer || !renderer->GetSkeleton())
	{
		return;
	}

	const auto* modelRenderer = owner.GetComponent<CModelRenderer>();
	const auto* model = modelRenderer ? modelRenderer->GetModel() : nullptr;

	if (m_dirty || model != m_builtModel)
	{
		Build(*renderer);
		m_builtModel = model;
		m_dirty = false;
	}

	if (m_runtimeGroups.empty())
	{
		return;
	}

	// Out of sight, the chains rest (the pose keeps their bones at rest) and start again from rest once seen
	if (m_sleepWhenHidden && !renderer->WasVisible())
	{
		m_asleep = true;
		return;
	}

	if (m_asleep)
	{
		m_asleep = false;
		m_resetPending = true;
	}

	const auto& transform = owner.transform;
	const FVector3 position = transform.GetWorldPosition();
	const FQuaternion rotation = transform.GetWorldRotation();
	const FVector3 worldScale = transform.GetWorldScale();
	const float scale = worldScale.x;

	const auto toWorld = [&](const FVector3& p_model) { return position + rotation * (p_model * worldScale); };
	const auto boneWorld = [&](uint32_t p_bone) { return toWorld(renderer->GetBoneGlobalPosition(p_bone).value_or(FVector3::Zero)); };

	// Where each chain hangs from now: its first joint, and the rotation of what it hangs from (the pose's, turned
	// by the looks applied before)
	for (auto& runtime : m_runtimeGroups)
	{
		for (size_t k = 0; k < runtime.group.chains.size(); ++k)
		{
			const uint32_t first = runtime.bones[k].front();
			const FQuaternion global = renderer->GetBoneGlobalRotation(first).value_or(FQuaternion::Identity);
			const FQuaternion local = renderer->GetBoneLocalRotation(first).value_or(FQuaternion::Identity);
			const FQuaternion parent = FQuaternion::Normalize(rotation * (global * FQuaternion::Conjugate(FQuaternion::Normalize(local))));

			Animation::SpringSolver::SetAnchor(runtime.group.chains[k], boneWorld(first), parent);
		}
	}

	for (auto& collider : m_runtimeColliders)
	{
		const FVector3 from = boneWorld(collider.from);
		const FVector3 other = boneWorld(collider.other);
		FVector3 start = from;
		FVector3 end = other;

		if (collider.away)
		{
			const FVector3 outwards = from - other;
			const float length = FVector3::Length(outwards);
			const FVector3 direction = length > 0.000001f ? outwards / length : FVector3{ 0.0f, 1.0f, 0.0f };
			start = from + direction * (collider.start * scale);
			end = from + direction * (collider.end * scale);
		}

		collider.capsule.start0 = collider.capsule.start1;
		collider.capsule.end0 = collider.capsule.end1;
		collider.capsule.start1 = start;
		collider.capsule.end1 = end;
		collider.capsule.radius = collider.radius * scale;
	}

	// A jump further than a run would make in the frame starts the chains again from rest
	const float limit = std::max(kMinTeleport, kTeleportSpeed * p_deltaTime) * scale;
	for (const auto& runtime : m_runtimeGroups)
	{
		for (const auto& chain : runtime.group.chains)
		{
			if (Animation::SpringSolver::GetAnchorMove(chain) > limit)
			{
				m_resetPending = true;
			}
		}
	}

	if (m_resetPending)
	{
		for (auto& collider : m_runtimeColliders)
		{
			collider.capsule.start0 = collider.capsule.start1;
			collider.capsule.end0 = collider.capsule.end1;
		}

		for (auto& runtime : m_runtimeGroups)
		{
			for (auto& chain : runtime.group.chains)
			{
				Animation::SpringSolver::Reset(chain, scale);
			}

			Animation::SpringSolver::MeasureLinks(runtime.group, scale);
			runtime.group.previousStep = 0.0f;
		}

		m_resetPending = false;
	}

	const std::optional<float> ground = m_groundEnabled ? std::optional<float>{ position.y + m_groundOffset } : std::nullopt;

	for (auto& runtime : m_runtimeGroups)
	{
		runtime.capsules.clear();
		for (const size_t collider : runtime.colliders)
		{
			runtime.capsules.push_back(m_runtimeColliders[collider].capsule);
		}

		Animation::SpringSolver::Step(runtime.group, runtime.capsules, ground, scale, p_deltaTime);

		for (size_t k = 0; k < runtime.group.chains.size(); ++k)
		{
			for (size_t i = 0; i < runtime.bones[k].size(); ++i)
			{
				renderer->SetConstraintRotation(runtime.bones[k][i], Animation::SpringSolver::GetLocal(runtime.group.chains[k], i));
			}
		}
	}
}

void OvCore::ECS::Components::CSpringBones::OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	using OvCore::Helpers::Serializer;

	Serializer::SerializeBoolean(p_doc, p_node, "ground", m_groundEnabled);
	Serializer::SerializeFloat(p_doc, p_node, "ground_offset", m_groundOffset);
	Serializer::SerializeBoolean(p_doc, p_node, "sleep_when_hidden", m_sleepWhenHidden);

	tinyxml2::XMLNode* groupsNode = p_doc.NewElement("groups");
	p_node->InsertEndChild(groupsNode);
	for (const auto& group : m_groups)
	{
		tinyxml2::XMLElement* groupNode = p_doc.NewElement("group");
		groupNode->SetAttribute("name", group.name.c_str());
		SerializeSettings(groupNode, group.settings);
		groupsNode->InsertEndChild(groupNode);

		for (const auto& chain : group.chains)
		{
			tinyxml2::XMLElement* chainNode = p_doc.NewElement("chain");
			chainNode->SetAttribute("root", chain.c_str());
			groupNode->InsertEndChild(chainNode);
		}

		for (const auto& collider : group.colliders)
		{
			tinyxml2::XMLElement* colliderNode = p_doc.NewElement("meets");
			colliderNode->SetAttribute("collider", collider.c_str());
			groupNode->InsertEndChild(colliderNode);
		}
	}

	tinyxml2::XMLNode* collidersNode = p_doc.NewElement("colliders");
	p_node->InsertEndChild(collidersNode);
	for (const auto& collider : m_colliders)
	{
		tinyxml2::XMLElement* colliderNode = p_doc.NewElement("collider");
		colliderNode->SetAttribute("name", collider.name.c_str());
		colliderNode->SetAttribute("from", collider.from.c_str());
		colliderNode->SetAttribute("to", collider.to.c_str());
		colliderNode->SetAttribute("away", collider.away.c_str());
		colliderNode->SetAttribute("start", collider.start);
		colliderNode->SetAttribute("end", collider.end);
		colliderNode->SetAttribute("radius", collider.radius);
		collidersNode->InsertEndChild(colliderNode);
	}
}

void OvCore::ECS::Components::CSpringBones::OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	using OvCore::Helpers::Serializer;

	Serializer::DeserializeBoolean(p_doc, p_node, "ground", m_groundEnabled);
	Serializer::DeserializeFloat(p_doc, p_node, "ground_offset", m_groundOffset);
	Serializer::DeserializeBoolean(p_doc, p_node, "sleep_when_hidden", m_sleepWhenHidden);

	const auto text = [](const tinyxml2::XMLElement* p_element, const char* p_name)
	{
		const char* value = p_element->Attribute(p_name);
		return value ? std::string{ value } : std::string{};
	};

	m_groups.clear();
	if (tinyxml2::XMLElement* groupsNode = p_node->FirstChildElement("groups"))
	{
		for (auto groupNode = groupsNode->FirstChildElement("group"); groupNode; groupNode = groupNode->NextSiblingElement("group"))
		{
			Group group{ text(groupNode, "name"), Sanitize(DeserializeSettings(groupNode)), {}, {} };

			for (auto chainNode = groupNode->FirstChildElement("chain"); chainNode; chainNode = chainNode->NextSiblingElement("chain"))
			{
				if (auto root = text(chainNode, "root"); !root.empty())
				{
					group.chains.push_back(root);
				}
			}

			for (auto colliderNode = groupNode->FirstChildElement("meets"); colliderNode; colliderNode = colliderNode->NextSiblingElement("meets"))
			{
				if (auto collider = text(colliderNode, "collider"); !collider.empty())
				{
					group.colliders.push_back(collider);
				}
			}

			m_groups.push_back(std::move(group));
		}
	}

	m_colliders.clear();
	if (tinyxml2::XMLElement* collidersNode = p_node->FirstChildElement("colliders"))
	{
		for (auto colliderNode = collidersNode->FirstChildElement("collider"); colliderNode; colliderNode = colliderNode->NextSiblingElement("collider"))
		{
			m_colliders.push_back({
				text(colliderNode, "name"),
				text(colliderNode, "from"),
				text(colliderNode, "to"),
				text(colliderNode, "away"),
				colliderNode->FloatAttribute("start", 0.0f),
				colliderNode->FloatAttribute("end", 0.0f),
				std::max(colliderNode->FloatAttribute("radius", 0.05f), 0.0f)
			});
		}
	}

	MarkDirty();
}

void OvCore::ECS::Components::CSpringBones::OnInspector(OvUI::Internal::WidgetContainer& p_root)
{
	using namespace OvCore::Helpers;

	GUIDrawer::DrawBoolean(p_root, "Ground", [this] { return m_groundEnabled; }, [this](bool p_value) { m_groundEnabled = p_value; });
	GUIDrawer::DrawScalar<float>(p_root, "Ground Offset", [this] { return m_groundOffset; }, [this](float p_value) { m_groundOffset = p_value; }, 0.01f, -100.0f, 100.0f);
	GUIDrawer::DrawBoolean(p_root, "Sleep When Hidden", [this] { return m_sleepWhenHidden; }, [this](bool p_value) { m_sleepWhenHidden = p_value; });

	auto& status = p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>();
	status.AddPlugin<OvUI::Plugins::DataDispatcher<std::string>>().RegisterGatherer([this, &status]
	{
		status.color = m_error ? OVUI_STYLE(Danger) : OVUI_STYLE(TextDisabled);
		return m_error ? *m_error : std::format("{} chains{}", GetChainCount(), m_asleep ? ", asleep" : "");
	});

	// Groups and colliders live in their own container, rebuilt in place when one is added or removed
	auto& lists = p_root.CreateWidget<OvUI::Widgets::Layout::Group>();
	lists.fullWidth = true;
	BuildWidgets(lists);
}

void OvCore::ECS::Components::CSpringBones::BuildWidgets(OvUI::Internal::WidgetContainer& p_container)
{
	using namespace OvCore::Helpers;

	for (auto& widget : p_container.GetWidgets())
	{
		widget.first->Destroy();
	}

	// A list typed as text keeps what is typed while it says the same list, so that a comma being typed stays
	const auto drawNames = [](OvUI::Internal::WidgetContainer& p_columns, const std::string& p_label, std::function<std::vector<std::string>()> p_get, std::function<void(std::vector<std::string>)> p_set)
	{
		auto typed = std::make_shared<std::string>();
		GUIDrawer::DrawString(p_columns, p_label,
			[typed, p_get]
			{
				if (SplitNames(*typed) != p_get())
				{
					*typed = JoinNames(p_get());
				}
				return *typed;
			},
			[typed, p_set](std::string p_text)
			{
				*typed = p_text;
				p_set(SplitNames(p_text));
			}
		);
	};

	for (size_t i = 0; i < m_groups.size(); ++i)
	{
		auto& node = p_container.CreateWidget<OvUI::Widgets::Layout::TreeNode>("Group: " + m_groups[i].name);
		node.SetID("spring_group_node_" + std::to_string(i));
		auto& columns = node.CreateWidget<OvUI::Widgets::Layout::Columns<2>>();
		columns.SetID("spring_group_" + std::to_string(i));
		columns.widths[0] = 200 * OVUI_SCALE;

		const auto setting = [this, i, &columns](const std::string& p_label, float Animation::SpringSettings::* p_field, float p_step, float p_min, float p_max)
		{
			GUIDrawer::DrawScalar<float>(columns, p_label,
				[this, i, p_field] { return i < m_groups.size() ? m_groups[i].settings.*p_field : 0.0f; },
				[this, i, p_field](float p_value)
				{
					if (i < m_groups.size())
					{
						auto settings = m_groups[i].settings;
						settings.*p_field = p_value;
						SetGroup(m_groups[i].name, settings);
					}
				},
				p_step, p_min, p_max
			);
		};

		setting("Stiffness", &Animation::SpringSettings::stiffness, 1.0f, 0.0f, 10000.0f);
		setting("Damping", &Animation::SpringSettings::damping, 0.1f, 0.0f, 1000.0f);
		setting("Gravity", &Animation::SpringSettings::gravity, 0.01f, -10.0f, 10.0f);
		setting("Radius", &Animation::SpringSettings::radius, 0.001f, 0.0f, 10.0f);
		setting("Max Angle", &Animation::SpringSettings::maxAngle, 1.0f, 0.0f, 180.0f);
		setting("Link", &Animation::SpringSettings::link, 0.01f, 0.0f, 1.0f);

		drawNames(columns, "Chains",
			[this, i] { return i < m_groups.size() ? m_groups[i].chains : std::vector<std::string>{}; },
			[this, i](std::vector<std::string> p_names) { if (i < m_groups.size()) SetGroupChains(m_groups[i].name, p_names); });
		drawNames(columns, "Colliders",
			[this, i] { return i < m_groups.size() ? m_groups[i].colliders : std::vector<std::string>{}; },
			[this, i](std::vector<std::string> p_names) { if (i < m_groups.size()) SetGroupColliders(m_groups[i].name, p_names); });

		auto& remove = node.CreateWidget<OvUI::Widgets::Buttons::Button>("Remove Group");
		remove.ClickedEvent += [this, &p_container, i]
		{
			if (i < m_groups.size())
			{
				RemoveGroup(m_groups[i].name);
			}
			BuildWidgets(p_container);
		};
	}

	auto& addGroup = p_container.CreateWidget<OvUI::Widgets::Buttons::Button>("Add Group");
	addGroup.ClickedEvent += [this, &p_container]
	{
		size_t n = m_groups.size() + 1;
		while (FindGroup("Group " + std::to_string(n)))
		{
			++n;
		}
		SetGroup("Group " + std::to_string(n), {});
		BuildWidgets(p_container);
	};

	for (size_t c = 0; c < m_colliders.size(); ++c)
	{
		auto& node = p_container.CreateWidget<OvUI::Widgets::Layout::TreeNode>("Collider: " + m_colliders[c].name);
		node.SetID("spring_collider_node_" + std::to_string(c));
		auto& columns = node.CreateWidget<OvUI::Widgets::Layout::Columns<2>>();
		columns.SetID("spring_collider_" + std::to_string(c));
		columns.widths[0] = 200 * OVUI_SCALE;

		const auto text = [this, c, &columns](const std::string& p_label, std::string Collider::* p_field)
		{
			GUIDrawer::DrawString(columns, p_label,
				[this, c, p_field] { return c < m_colliders.size() ? m_colliders[c].*p_field : std::string{}; },
				[this, c, p_field](std::string p_value) { if (c < m_colliders.size()) { m_colliders[c].*p_field = p_value; MarkDirty(); } });
		};

		const auto number = [this, c, &columns](const std::string& p_label, float Collider::* p_field, float p_min)
		{
			GUIDrawer::DrawScalar<float>(columns, p_label,
				[this, c, p_field] { return c < m_colliders.size() ? m_colliders[c].*p_field : 0.0f; },
				[this, c, p_field, p_min](float p_value) { if (c < m_colliders.size()) { m_colliders[c].*p_field = std::max(p_value, p_min); MarkDirty(); } },
				0.01f, p_min, 100.0f);
		};

		text("From", &Collider::from);
		text("To", &Collider::to);
		text("Away", &Collider::away);
		number("Start", &Collider::start, -100.0f);
		number("End", &Collider::end, -100.0f);
		number("Radius", &Collider::radius, 0.0f);

		auto& remove = node.CreateWidget<OvUI::Widgets::Buttons::Button>("Remove Collider");
		remove.ClickedEvent += [this, &p_container, c]
		{
			if (c < m_colliders.size())
			{
				RemoveCollider(m_colliders[c].name);
			}
			BuildWidgets(p_container);
		};
	}

	auto& addCollider = p_container.CreateWidget<OvUI::Widgets::Buttons::Button>("Add Collider");
	addCollider.ClickedEvent += [this, &p_container]
	{
		size_t n = m_colliders.size() + 1;
		const auto exists = [this](const std::string& p_name)
		{
			return std::any_of(m_colliders.begin(), m_colliders.end(), [&](const Collider& p_collider) { return p_collider.name == p_name; });
		};
		while (exists("Collider " + std::to_string(n)))
		{
			++n;
		}
		SetCollider({ "Collider " + std::to_string(n) });
		BuildWidgets(p_container);
	};
}
