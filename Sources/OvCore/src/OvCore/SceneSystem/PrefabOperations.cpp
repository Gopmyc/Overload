/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <charconv>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <tinyxml2.h>

#include <OvDebug/Logger.h>
#include <OvCore/ECS/Actor.h>
#include <OvCore/Helpers/Serializer.h>
#include <OvCore/SceneSystem/PrefabOperations.h>

namespace
{
	void NormalizePrefabSourcesRecursively(
		OvCore::ECS::Actor& p_actor,
		const std::string& p_inheritedPrefabSource)
	{
		if (p_actor.HasPrefabSource())
		{
			const std::string currentPrefabSource = p_actor.GetPrefabSource();
			if (currentPrefabSource == p_inheritedPrefabSource)
			{
				p_actor.SetPrefabSource("?");
			}
		}

		const std::string nextInheritedPrefabSource =
			p_actor.HasPrefabSource() ? p_actor.GetPrefabSource() : p_inheritedPrefabSource;

		for (auto* child : p_actor.GetChildren())
		{
			NormalizePrefabSourcesRecursively(*child, nextInheritedPrefabSource);
		}
	}

	/**
	* Points every actor reference under the element (a Behaviour property of type "actor") that names an actor of the
	* prefab at that actor's instance, by the GUIDs the prefab file gave its actors
	*/
	void RemapActorReferences(tinyxml2::XMLElement& p_element, const std::unordered_map<uint64_t, uint64_t>& p_instanceGUIDs)
	{
		for (auto* child = p_element.FirstChildElement(); child; child = child->NextSiblingElement())
		{
			const char* type = child->Attribute("type");
			const char* text = child->GetText();

			if (type && text && std::string_view(type) == "actor")
			{
				uint64_t guid = 0;
				const auto [end, error] = std::from_chars(text, text + std::strlen(text), guid);

				if (error == std::errc{})
				{
					if (auto found = p_instanceGUIDs.find(guid); found != p_instanceGUIDs.end())
					{
						child->SetText(std::to_string(found->second).c_str());
					}
				}
			}

			RemapActorReferences(*child, p_instanceGUIDs);
		}
	}

	void SerializeActorHierarchy(
		OvCore::ECS::Actor& p_actor,
		tinyxml2::XMLDocument& p_doc,
		tinyxml2::XMLNode& p_actorsRoot)
	{
		if (!p_actor.HasPrefabNodeGUID())
		{
			p_actor.SetPrefabNodeGUID(p_actor.GetGUID());
		}

		p_actor.OnSerialize(p_doc, &p_actorsRoot);

		for (auto* child : p_actor.GetChildren())
		{
			SerializeActorHierarchy(*child, p_doc, p_actorsRoot);
		}
	}
}

bool OvCore::SceneSystem::PrefabOperations::SaveToFile(OvCore::ECS::Actor& p_rootActor, const std::filesystem::path& p_outputPath)
{
	tinyxml2::XMLDocument doc;

	auto* rootNode = doc.NewElement("root");
	doc.InsertFirstChild(rootNode);

	auto* prefabNode = doc.NewElement("prefab");
	rootNode->InsertEndChild(prefabNode);

	auto* actorsNode = doc.NewElement("actors");
	prefabNode->InsertEndChild(actorsNode);

	SerializeActorHierarchy(p_rootActor, doc, *actorsNode);

	return doc.SaveFile(p_outputPath.string().c_str()) == tinyxml2::XML_SUCCESS;
}

void OvCore::SceneSystem::PrefabOperations::SetRootPrefabSourceAndNormalizeChildren(
	OvCore::ECS::Actor& p_rootActor,
	const std::string& p_newRootPrefabSource)
{
	p_rootActor.SetPrefabSource(p_newRootPrefabSource);

	for (auto* child : p_rootActor.GetChildren())
	{
		NormalizePrefabSourcesRecursively(*child, p_newRootPrefabSource);
	}
}

OvCore::ECS::Actor* OvCore::SceneSystem::PrefabOperations::InstantiateFromFile(
	const std::filesystem::path& p_prefabPath,
	const std::function<OvCore::ECS::Actor&(void)>& p_createActor)
{
	if (!p_createActor)
	{
		OVLOG_ERROR("Failed to instantiate prefab \"" + p_prefabPath.string() + "\": invalid actor factory callback.");
		return nullptr;
	}

	tinyxml2::XMLDocument doc;
	const auto loadResult = doc.LoadFile(p_prefabPath.string().c_str());
	if (loadResult != tinyxml2::XML_SUCCESS)
	{
		OVLOG_ERROR("Failed to instantiate prefab \"" + p_prefabPath.string() + "\": XML parsing failed (code " + std::to_string(loadResult) + ").");
		return nullptr;
	}

	auto* rootNode = doc.FirstChildElement("root");
	auto* prefabNode = rootNode ? rootNode->FirstChildElement("prefab") : nullptr;
	auto* actorsNode = prefabNode ? prefabNode->FirstChildElement("actors") : nullptr;

	if (!actorsNode)
	{
		OVLOG_ERROR("Failed to instantiate prefab \"" + p_prefabPath.string() + "\": missing <root>/<prefab>/<actors> node.");
		return nullptr;
	}

	struct PendingAttachment
	{
		OvCore::ECS::Actor* actor = nullptr;
		int64_t sourceParentID = 0;
	};

	struct PendingActor
	{
		tinyxml2::XMLElement* node = nullptr;
		OvCore::ECS::Actor* actor = nullptr;
		uint64_t sourceGUID = 0;
	};

	std::vector<PendingActor> pendingActors;
	std::vector<PendingAttachment> pendingAttachments;
	std::unordered_map<int64_t, OvCore::ECS::Actor*> sourceToInstance;
	std::unordered_map<uint64_t, uint64_t> instanceGUIDs;

	// First, an actor for every node, so that the GUID each actor had in the file is known before any is read
	for (auto* currentActor = actorsNode->FirstChildElement("actor");
		currentActor;
		currentActor = currentActor->NextSiblingElement("actor"))
	{
		auto& newActor = p_createActor();

		// The instance keeps its own GUID, written over the source's so that no other actor ever shares it
		uint64_t sourceGUID = newActor.GetGUID();
		if (auto* guidNode = currentActor->FirstChildElement("guid"))
		{
			OvCore::Helpers::Serializer::DeserializeUInt64(doc, currentActor, "guid", sourceGUID);
			instanceGUIDs[sourceGUID] = newActor.GetGUID();
			guidNode->SetText(std::to_string(newActor.GetGUID()).c_str());
		}

		pendingActors.push_back({ currentActor, &newActor, sourceGUID });
	}

	// A reference from one of the prefab's actors to another now names that actor's instance
	RemapActorReferences(*actorsNode, instanceGUIDs);

	for (auto& pending : pendingActors)
	{
		auto& newActor = *pending.actor;
		const int64_t generatedID = newActor.GetID();

		newActor.OnDeserialize(doc, pending.node);
		const uint64_t prefabNodeGUID = newActor.HasPrefabNodeGUID() ? newActor.GetPrefabNodeGUID() : pending.sourceGUID;
		newActor.SetPrefabNodeGUID(prefabNodeGUID);

		pendingAttachments.push_back({
			&newActor,
			newActor.GetParentID()
		});

		sourceToInstance[newActor.GetID()] = &newActor;

		newActor.SetID(generatedID);
	}

	OvCore::ECS::Actor* instantiatedRoot = nullptr;

	for (auto& pending : pendingAttachments)
	{
		if (auto found = sourceToInstance.find(pending.sourceParentID); found != sourceToInstance.end())
		{
			pending.actor->SetParent(*found->second);
		}
		else if (!instantiatedRoot)
		{
			instantiatedRoot = pending.actor;
		}
	}

	return instantiatedRoot;
}
