/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <format>
#include <fstream>
#include <sstream>
#include <utility>

#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CAnimator.h>
#include <OvCore/ECS/Components/CModelRenderer.h>
#include <OvCore/ECS/Components/CSkinnedMeshRenderer.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Helpers/GUIDrawer.h>
#include <OvCore/Helpers/Serializer.h>
#include <OvCore/ResourceManagement/ModelManager.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvCore/SceneSystem/SceneManager.h>
#include <OvDebug/Logger.h>
#include <OvRendering/Resources/Model.h>
#include <OvUI/Plugins/DataDispatcher.h>
#include <OvUI/Styling/Style.h>
#include <OvUI/Widgets/Buttons/Button.h>
#include <OvUI/Widgets/Layout/Group.h>
#include <OvUI/Widgets/Texts/TextColored.h>
#include <OvUI/Widgets/Texts/TextWrapped.h>

namespace
{
	using OvCore::Animation::BoneMap;
	using OvCore::Animation::EGraphParameterType;

	// "ModelBone=ClipBone, ..." as the inspector shows a bone map, sorted
	std::string FormatBoneMap(const BoneMap& p_boneMap)
	{
		std::vector<std::pair<std::string, std::string>> entries(p_boneMap.begin(), p_boneMap.end());
		std::sort(entries.begin(), entries.end());

		std::string text;
		for (const auto& [modelBone, clipBone] : entries)
		{
			text += (text.empty() ? "" : ", ") + modelBone + "=" + clipBone;
		}
		return text;
	}

	BoneMap ParseBoneMap(const std::string& p_text)
	{
		BoneMap boneMap;
		std::stringstream stream(p_text);
		std::string entry;

		while (std::getline(stream, entry, ','))
		{
			const auto equal = entry.find('=');
			if (equal == std::string::npos)
			{
				continue;
			}

			const auto trim = [](std::string p_value)
			{
				const auto first = p_value.find_first_not_of(" \t");
				const auto last = p_value.find_last_not_of(" \t");
				return first == std::string::npos ? std::string{} : p_value.substr(first, last - first + 1);
			};

			const std::string modelBone = trim(entry.substr(0, equal));
			const std::string clipBone = trim(entry.substr(equal + 1));
			if (!modelBone.empty() && !clipBone.empty())
			{
				boneMap[modelBone] = clipBone;
			}
		}

		return boneMap;
	}
}

OvCore::ECS::Components::CAnimator::CAnimator(ECS::Actor& p_owner) : AComponent(p_owner)
{
}

std::string OvCore::ECS::Components::CAnimator::GetName()
{
	return "Animator";
}

std::string OvCore::ECS::Components::CAnimator::GetTypeName()
{
	return std::string{ ComponentTraits<CAnimator>::Name };
}

void OvCore::ECS::Components::CAnimator::SetGraphPath(const std::string& p_path)
{
	m_graphPath = p_path;
	m_dirty = true;
}

const std::string& OvCore::ECS::Components::CAnimator::GetGraphPath() const
{
	return m_graphPath;
}

void OvCore::ECS::Components::CAnimator::SetAnimationSource(OvRendering::Resources::Model* p_model)
{
	m_source = p_model;
	m_dirty = true;
}

OvRendering::Resources::Model* OvCore::ECS::Components::CAnimator::GetAnimationSource() const
{
	return m_source;
}

void OvCore::ECS::Components::CAnimator::SetBoneMap(const Animation::BoneMap& p_boneMap)
{
	m_boneMap = p_boneMap;
	m_boneMapText = FormatBoneMap(m_boneMap);
	m_layersSent = false;
}

const OvCore::Animation::BoneMap& OvCore::ECS::Components::CAnimator::GetBoneMap() const
{
	return m_boneMap;
}

void OvCore::ECS::Components::CAnimator::Reload()
{
	m_dirty = true;
}

bool OvCore::ECS::Components::CAnimator::IsLoaded()
{
	return EnsureLoaded() != nullptr;
}

const std::optional<std::string>& OvCore::ECS::Components::CAnimator::GetError() const
{
	return m_error;
}

std::optional<size_t> OvCore::ECS::Components::CAnimator::FindParameter(const std::string& p_name, Animation::EGraphParameterType p_type)
{
	const auto* graph = EnsureLoaded();
	if (!graph)
	{
		return std::nullopt;
	}

	const auto index = graph->GetDefinition().FindParameter(p_name);
	return index && graph->GetDefinition().parameters[*index].type == p_type ? index : std::nullopt;
}

bool OvCore::ECS::Components::CAnimator::SetFloat(const std::string& p_name, float p_value)
{
	const auto index = FindParameter(p_name, EGraphParameterType::FLOAT);
	m_changed = m_changed || index.has_value();
	return index && m_graph->SetValue(*index, p_value);
}

std::optional<float> OvCore::ECS::Components::CAnimator::GetFloat(const std::string& p_name)
{
	const auto index = FindParameter(p_name, EGraphParameterType::FLOAT);
	return index ? std::optional<float>{ m_graph->GetValue(*index) } : std::nullopt;
}

bool OvCore::ECS::Components::CAnimator::SetBool(const std::string& p_name, bool p_value)
{
	const auto index = FindParameter(p_name, EGraphParameterType::BOOL);
	m_changed = m_changed || index.has_value();
	return index && m_graph->SetValue(*index, p_value ? 1.0f : 0.0f);
}

std::optional<bool> OvCore::ECS::Components::CAnimator::GetBool(const std::string& p_name)
{
	const auto index = FindParameter(p_name, EGraphParameterType::BOOL);
	return index ? std::optional<bool>{ m_graph->GetValue(*index) != 0.0f } : std::nullopt;
}

bool OvCore::ECS::Components::CAnimator::SetTrigger(const std::string& p_name)
{
	const auto index = FindParameter(p_name, EGraphParameterType::TRIGGER);
	m_changed = m_changed || index.has_value();
	return index && m_graph->SetTrigger(*index, true);
}

bool OvCore::ECS::Components::CAnimator::ResetTrigger(const std::string& p_name)
{
	const auto index = FindParameter(p_name, EGraphParameterType::TRIGGER);
	m_changed = m_changed || index.has_value();
	return index && m_graph->SetTrigger(*index, false);
}

std::vector<std::string> OvCore::ECS::Components::CAnimator::GetParameterNames()
{
	std::vector<std::string> names;
	if (const auto* graph = EnsureLoaded())
	{
		for (const auto& parameter : graph->GetDefinition().parameters)
		{
			names.push_back(parameter.name);
		}
	}
	return names;
}

std::vector<std::string> OvCore::ECS::Components::CAnimator::GetLayerNames()
{
	std::vector<std::string> names;
	if (const auto* graph = EnsureLoaded())
	{
		for (const auto& layer : graph->GetDefinition().layers)
		{
			names.push_back(layer.name);
		}
	}
	return names;
}

std::optional<size_t> OvCore::ECS::Components::CAnimator::FindLayer(const std::string& p_name)
{
	if (const auto* graph = EnsureLoaded())
	{
		const auto& layers = graph->GetDefinition().layers;
		for (size_t i = 0; i < layers.size(); ++i)
		{
			if (layers[i].name == p_name)
			{
				return i;
			}
		}
	}
	return std::nullopt;
}

std::optional<std::string> OvCore::ECS::Components::CAnimator::GetState(size_t p_layer)
{
	const auto* graph = EnsureLoaded();
	if (!graph || p_layer >= graph->GetDefinition().layers.size())
	{
		return std::nullopt;
	}

	return graph->GetDefinition().layers[p_layer].states[graph->GetState(p_layer)].name;
}

float OvCore::ECS::Components::CAnimator::GetStateTime(size_t p_layer)
{
	const auto* graph = EnsureLoaded();
	return graph ? graph->GetStateTime(p_layer) : 0.0f;
}

bool OvCore::ECS::Components::CAnimator::IsInTransition(size_t p_layer)
{
	const auto* graph = EnsureLoaded();
	return graph && graph->IsInTransition(p_layer);
}

bool OvCore::ECS::Components::CAnimator::SetLayerWeight(size_t p_layer, float p_weight)
{
	if (!EnsureLoaded() || p_layer >= m_layerWeights.size())
	{
		return false;
	}

	m_layerWeights[p_layer] = std::clamp(p_weight, 0.0f, 1.0f);
	m_changed = true;
	return true;
}

std::optional<float> OvCore::ECS::Components::CAnimator::GetLayerWeight(size_t p_layer)
{
	if (!EnsureLoaded() || p_layer >= m_layerWeights.size())
	{
		return std::nullopt;
	}

	return m_layerWeights[p_layer];
}

bool OvCore::ECS::Components::CAnimator::Play(const std::string& p_state, float p_fade, std::optional<size_t> p_layer)
{
	auto* graph = EnsureLoaded();
	if (!graph)
	{
		return false;
	}

	const auto& layers = graph->GetDefinition().layers;
	for (size_t l = 0; l < layers.size(); ++l)
	{
		if (p_layer && *p_layer != l)
		{
			continue;
		}

		for (size_t s = 0; s < layers[l].states.size(); ++s)
		{
			if (layers[l].states[s].name == p_state)
			{
				m_changed = true;
				return graph->Play(l, s, std::max(p_fade, 0.0f));
			}
		}
	}

	return false;
}

void OvCore::ECS::Components::CAnimator::Reset()
{
	if (auto* graph = EnsureLoaded())
	{
		graph->Reset();
		m_changed = true;

		m_layerWeights.clear();
		for (const auto& layer : graph->GetDefinition().layers)
		{
			m_layerWeights.push_back(layer.weight);
		}
	}
}

OvCore::Animation::AnimationGraph* OvCore::ECS::Components::CAnimator::EnsureLoaded(const SceneSystem::Scene* p_scene)
{
	const auto* modelRenderer = owner.GetComponent<CModelRenderer>();
	const auto* model = modelRenderer ? modelRenderer->GetModel() : nullptr;

	if (m_dirty || model != m_builtModel)
	{
		// Outside the scene's own step (a script, the inspector), paths are read through the current scene
		if (!p_scene && Global::ServiceLocator::Has<SceneSystem::SceneManager>())
		{
			p_scene = OVSERVICE(SceneSystem::SceneManager).GetCurrentScene();
		}

		if (!p_scene)
		{
			return nullptr;
		}

		Load(*p_scene);
	}

	return m_graph ? &*m_graph : nullptr;
}

void OvCore::ECS::Components::CAnimator::Fail(std::string p_error)
{
	if (!m_error || *m_error != p_error)
	{
		OVLOG_WARNING("Animator on '" + owner.GetName() + "': " + p_error);
	}

	m_error = std::move(p_error);
	m_graph.reset();
}

void OvCore::ECS::Components::CAnimator::Load(const SceneSystem::Scene& p_scene)
{
	const auto* modelRenderer = owner.GetComponent<CModelRenderer>();
	const auto* model = modelRenderer ? modelRenderer->GetModel() : nullptr;

	m_dirty = false;
	++m_loadCount;
	m_builtModel = model;
	m_layersSent = false;
	m_graph.reset();
	m_clipIndices.clear();
	m_layerWeights.clear();

	if (m_graphPath.empty())
	{
		Fail("no graph file");
		return;
	}

	const auto* source = m_source ? m_source : model;
	if (!source)
	{
		Fail("no model to take the clips from");
		return;
	}

	std::ifstream stream(p_scene.GetRealAssetPath(m_graphPath), std::ios::binary);
	if (!stream)
	{
		Fail(std::format("cannot read '{}'", m_graphPath));
		return;
	}

	std::stringstream text;
	text << stream.rdbuf();

	Animation::GraphDefinition definition;
	if (const auto error = Animation::GraphDefinition::Parse(text.str(), definition))
	{
		Fail(std::format("'{}': {}", m_graphPath, *error));
		return;
	}

	// Each clip the file names, among the source's
	const auto& animations = source->GetAnimations();
	std::vector<float> durations;

	for (size_t c = 0; c < definition.clips.size(); ++c)
	{
		const auto found = std::find_if(animations.begin(), animations.end(), [&](const auto& p_animation) { return p_animation.name == definition.clips[c]; });
		if (found == animations.end())
		{
			Fail(std::format("'{}': {}: no clip '{}' in '{}'", m_graphPath, definition.clipPaths[c], definition.clips[c], source->path));
			return;
		}

		m_clipIndices.push_back(static_cast<uint32_t>(std::distance(animations.begin(), found)));
		durations.push_back(found->GetDurationSeconds());
	}

	for (const auto& layer : definition.layers)
	{
		m_layerWeights.push_back(layer.weight);
	}

	m_error.reset();
	m_graph.emplace(std::move(definition), std::move(durations));
	m_changed = true;
}

void OvCore::ECS::Components::CAnimator::ReleaseRenderer()
{
	if (m_driving)
	{
		if (auto* renderer = owner.GetComponent<CSkinnedMeshRenderer>())
		{
			renderer->ClearDrivenLayers();
		}
	}

	m_driving = false;
	m_layersSent = false;
}

void OvCore::ECS::Components::CAnimator::Apply(const SceneSystem::Scene& p_scene, float p_deltaTime)
{
	auto* renderer = owner.GetComponent<CSkinnedMeshRenderer>();
	if (!renderer)
	{
		if (!m_error)
		{
			Fail("no skinned mesh renderer on the actor");
		}
		return;
	}

	auto* graph = EnsureLoaded(&p_scene);
	if (!graph)
	{
		ReleaseRenderer();
		return;
	}

	// The renderer plays the graph's layers from now on, each from the graph's source, under its mask
	if (!m_layersSent || !renderer->IsDriven())
	{
		std::vector<CSkinnedMeshRenderer::DrivenLayer> layers;
		for (const auto& layer : graph->GetDefinition().layers)
		{
			layers.push_back({ m_source, layer.mask, m_boneMap });
		}

		renderer->SetDrivenLayers(layers);
		m_driving = true;
		m_layersSent = true;

		if (const auto error = renderer->GetDrivenLayerError(0))
		{
			Fail(std::format("the clips cannot play on the model: {}", *error));
			ReleaseRenderer();
			return;
		}
	}

	// A state that changes, and a change a script made, show in the pose at once
	bool changed = std::exchange(m_changed, false);
	const auto& layers = graph->GetDefinition().layers;
	std::vector<size_t> states(layers.size());
	for (size_t l = 0; l < layers.size(); ++l)
	{
		states[l] = graph->GetState(l);
	}

	graph->Update(p_deltaTime);

	for (size_t l = 0; l < layers.size(); ++l)
	{
		changed = changed || graph->GetState(l) != states[l];
	}

	m_drivenSamples.clear();
	for (const auto& sample : graph->GetSamples())
	{
		m_drivenSamples.push_back({
			static_cast<uint32_t>(sample.layer),
			m_clipIndices[sample.clip],
			sample.time,
			sample.weight,
			sample.loop
		});
	}

	renderer->SetDrivenPose(m_drivenSamples, m_layerWeights, p_deltaTime, changed);
}

void OvCore::ECS::Components::CAnimator::OnDisable()
{
	ReleaseRenderer();
}

void OvCore::ECS::Components::CAnimator::OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	using namespace OvCore::Helpers;

	Serializer::SerializeString(p_doc, p_node, "graph", m_graphPath);
	Serializer::SerializeModel(p_doc, p_node, "animation_source", m_source);

	if (!m_boneMap.empty())
	{
		std::vector<std::pair<std::string, std::string>> entries(m_boneMap.begin(), m_boneMap.end());
		std::sort(entries.begin(), entries.end());

		tinyxml2::XMLNode* boneMapNode = p_doc.NewElement("bone_map");
		p_node->InsertEndChild(boneMapNode);

		for (const auto& [modelBone, clipBone] : entries)
		{
			tinyxml2::XMLElement* entryNode = p_doc.NewElement("bone");
			entryNode->SetAttribute("model", modelBone.c_str());
			entryNode->SetAttribute("clip", clipBone.c_str());
			boneMapNode->InsertEndChild(entryNode);
		}
	}
}

void OvCore::ECS::Components::CAnimator::OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	using namespace OvCore::Helpers;

	Serializer::DeserializeString(p_doc, p_node, "graph", m_graphPath);
	Serializer::DeserializeModel(p_doc, p_node, "animation_source", m_source);

	m_boneMap.clear();
	if (tinyxml2::XMLElement* boneMapNode = p_node->FirstChildElement("bone_map"))
	{
		for (auto entry = boneMapNode->FirstChildElement("bone"); entry; entry = entry->NextSiblingElement("bone"))
		{
			const char* modelBone = entry->Attribute("model");
			const char* clipBone = entry->Attribute("clip");

			if (modelBone && clipBone && *modelBone && *clipBone)
			{
				m_boneMap[modelBone] = clipBone;
			}
		}
	}

	m_boneMapText = FormatBoneMap(m_boneMap);
	m_dirty = true;
}

void OvCore::ECS::Components::CAnimator::OnInspector(OvUI::Internal::WidgetContainer& p_root)
{
	using namespace OvCore::Helpers;

	GUIDrawer::DrawAsset(p_root, "Graph",
		[this] { return m_graphPath; },
		[this](std::string p_path) { SetGraphPath(p_path); },
		OvTools::Utils::PathParser::EFileType::ANIMATION_GRAPH
	);

	GUIDrawer::DrawAsset(p_root, "Animation Source",
		[this] { return m_source ? m_source->path : std::string{}; },
		[this](std::string p_path)
		{
			SetAnimationSource(p_path.empty() ? nullptr : OVSERVICE(OvCore::ResourceManagement::ModelManager).GetResource(p_path));
		},
		OvTools::Utils::PathParser::EFileType::MODEL
	);

	// As typed: "ModelBone=ClipBone, ...", kept while it says the same map
	GUIDrawer::DrawString(p_root, "Bone Map",
		[this]
		{
			if (ParseBoneMap(m_boneMapText) != m_boneMap)
			{
				m_boneMapText = FormatBoneMap(m_boneMap);
			}
			return m_boneMapText;
		},
		[this](std::string p_text)
		{
			m_boneMapText = p_text;
			m_boneMap = ParseBoneMap(p_text);
			m_layersSent = false;
		}
	);

	// The inspector lays components out in two columns: the status, then what each layer plays. A file that cannot play
	// is said below on the full width, its JSON path being long
	auto& status = p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>();
	auto& details = p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>("", OVUI_STYLE(TextDisabled));
	auto& errorGroup = p_root.CreateWidget<OvUI::Widgets::Layout::Group>();
	errorGroup.fullWidth = true;
	auto& error = errorGroup.CreateWidget<OvUI::Widgets::Texts::TextWrapped>();

	// The parameters, editable while the game plays, in a container rebuilt whenever the graph is read again
	auto& parameters = p_root.CreateWidget<OvUI::Widgets::Layout::Group>();
	parameters.fullWidth = true;
	BuildParameterWidgets(parameters);

	status.AddPlugin<OvUI::Plugins::DataDispatcher<std::string>>().RegisterGatherer([this, &status, &errorGroup, &parameters]
	{
		EnsureLoaded();

		if (m_inspectedLoadCount != m_loadCount)
		{
			BuildParameterWidgets(parameters);
		}

		errorGroup.enabled = m_error.has_value();
		status.color = m_graph ? OVUI_STYLE(Success) : OVUI_STYLE(Danger);
		return m_graph ? std::string{ "Ready" } : std::string{ "Error" };
	});

	details.AddPlugin<OvUI::Plugins::DataDispatcher<std::string>>().RegisterGatherer([this]
	{
		if (!m_graph)
		{
			return std::string{ "Does not play" };
		}

		std::string text;
		const auto& layers = m_graph->GetDefinition().layers;
		for (size_t l = 0; l < layers.size(); ++l)
		{
			text += std::format("{}{}: {}{}", l > 0 ? "\n" : "", layers[l].name, layers[l].states[m_graph->GetState(l)].name, m_graph->IsInTransition(l) ? " (fading in)" : "");
		}
		return text;
	});

	error.AddPlugin<OvUI::Plugins::DataDispatcher<std::string>>().RegisterGatherer([this]
	{
		return m_error.value_or(std::string{});
	});
}

void OvCore::ECS::Components::CAnimator::BuildParameterWidgets(OvUI::Internal::WidgetContainer& p_container)
{
	using namespace OvCore::Helpers;

	for (auto& widget : p_container.GetWidgets())
	{
		widget.first->Destroy();
	}

	auto& reload = p_container.CreateWidget<OvUI::Widgets::Buttons::Button>("Reload Graph");
	reload.ClickedEvent += [this, &p_container]
	{
		Reload();
		EnsureLoaded();
		BuildParameterWidgets(p_container);
	};

	const auto* graph = EnsureLoaded();
	m_inspectedLoadCount = m_loadCount;

	if (!graph)
	{
		return;
	}

	for (const auto& parameter : graph->GetDefinition().parameters)
	{
		const std::string name = parameter.name;

		switch (parameter.type)
		{
		case Animation::EGraphParameterType::FLOAT:
			GUIDrawer::DrawScalar<float>(p_container, name,
				[this, name] { return GetFloat(name).value_or(0.0f); },
				[this, name](float p_value) { SetFloat(name, p_value); },
				0.01f, -10000.0f, 10000.0f
			);
			break;

		case Animation::EGraphParameterType::BOOL:
			GUIDrawer::DrawBoolean(p_container, name,
				[this, name] { return GetBool(name).value_or(false); },
				[this, name](bool p_value) { SetBool(name, p_value); }
			);
			break;

		case Animation::EGraphParameterType::TRIGGER:
		{
			auto& button = p_container.CreateWidget<OvUI::Widgets::Buttons::Button>("Trigger " + name);
			button.ClickedEvent += [this, name] { SetTrigger(name); };
			break;
		}
		}
	}
}
