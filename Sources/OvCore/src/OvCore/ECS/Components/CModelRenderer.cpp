/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <format>

#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CModelRenderer.h>
#include <OvCore/ECS/Components/CSkinnedMeshRenderer.h>
#include <OvCore/Helpers/GUIDrawer.h>
#include <OvCore/Helpers/Serializer.h>

#include <OvUI/Plugins/DataDispatcher.h>
#include <OvUI/Plugins/DDTarget.h>
#include <OvUI/Styling/Style.h>
#include <OvUI/Widgets/Buttons/Button.h>
#include <OvUI/Widgets/Layout/Group.h>
#include <OvUI/Widgets/Drags/DragFloat.h>
#include <OvUI/Widgets/Selection/CheckBox.h>
#include <OvUI/Widgets/Selection/ComboBox.h>
#include <OvUI/Widgets/Texts/Text.h>
#include <OvUI/Widgets/Texts/TextColored.h>
#include <OvUI/Widgets/Visual/Image.h>

OvCore::ECS::Components::CModelRenderer::CModelRenderer(ECS::Actor& p_owner) : AComponent(p_owner)
{
	m_modelChangedEvent += [this]()
	{
		m_lodsDirty = true;

		if (auto skinnedMeshRenderer = owner.GetComponent<CSkinnedMeshRenderer>())
			skinnedMeshRenderer->NotifyModelChanged();
	};

	m_lodChangedEvent += [this]()
	{
		m_lodsDirty = true;
	};
}

std::string OvCore::ECS::Components::CModelRenderer::GetName()
{
	return "Model Renderer";
}

std::string OvCore::ECS::Components::CModelRenderer::GetTypeName()
{
	return std::string{ComponentTraits<CModelRenderer>::Name};
}

void OvCore::ECS::Components::CModelRenderer::SetModel(OvRendering::Resources::Model* p_model)
{
	m_model = p_model;
	m_modelChangedEvent.Invoke();
}

OvRendering::Resources::Model * OvCore::ECS::Components::CModelRenderer::GetModel() const
{
	return m_model;
}

void OvCore::ECS::Components::CModelRenderer::SetFrustumBehaviour(EFrustumBehaviour p_boundingMode)
{
	m_frustumBehaviour = p_boundingMode;
}

OvCore::ECS::Components::CModelRenderer::EFrustumBehaviour OvCore::ECS::Components::CModelRenderer::GetFrustumBehaviour() const
{
	return m_frustumBehaviour;
}

const OvRendering::Geometry::BoundingSphere& OvCore::ECS::Components::CModelRenderer::GetCustomBoundingSphere() const
{
	return m_customBoundingSphere;
}

void OvCore::ECS::Components::CModelRenderer::SetCustomBoundingSphere(const OvRendering::Geometry::BoundingSphere& p_boundingSphere)
{
	m_customBoundingSphere = p_boundingSphere;
}

void OvCore::ECS::Components::CModelRenderer::SetMaxDrawDistance(float p_distance)
{
	m_maxDrawDistance = std::max(p_distance, 0.0f);
}

float OvCore::ECS::Components::CModelRenderer::GetMaxDrawDistance() const
{
	return m_maxDrawDistance;
}

void OvCore::ECS::Components::CModelRenderer::SetLods(std::span<const LodLevel> p_levels)
{
	m_lodCount = static_cast<uint32_t>(std::min<size_t>(p_levels.size(), kMaxLodLevels));
	std::copy_n(p_levels.begin(), m_lodCount, m_lods.begin());
	std::fill(m_lods.begin() + m_lodCount, m_lods.end(), LodLevel{});
	m_lodsDirty = true;
	++m_lodRevision;
}

std::vector<OvCore::ECS::Components::CModelRenderer::LodLevel> OvCore::ECS::Components::CModelRenderer::GetLods() const
{
	return { m_lods.begin(), m_lods.begin() + m_lodCount };
}

void OvCore::ECS::Components::CModelRenderer::SetLodBias(float p_bias)
{
	m_lodBias = std::max(p_bias, 0.01f);
}

float OvCore::ECS::Components::CModelRenderer::GetLodBias() const
{
	return m_lodBias;
}

std::optional<std::string> OvCore::ECS::Components::CModelRenderer::GetLodError()
{
	RefreshLods();
	return m_lodError;
}

uint32_t OvCore::ECS::Components::CModelRenderer::GetLodLevel() const
{
	return m_lodLevel > 0 && m_lodLevel <= m_activeLods.size() ? m_activeLods[m_lodLevel - 1].level : 0;
}

void OvCore::ECS::Components::CModelRenderer::RefreshLods()
{
	if (!m_lodsDirty)
	{
		return;
	}

	m_lodsDirty = false;
	m_activeLods.clear();
	m_activeStarts.clear();
	m_lodError.reset();

	std::string error;
	float previous = 0.0f;

	for (uint32_t i = 0; i < m_lodCount; ++i)
	{
		const auto& level = m_lods[i];
		std::optional<std::string> why;

		if (!level.model)
		{
			why = "it has no model";
		}
		else if (level.distance <= previous)
		{
			why = std::format("it starts at {} m, not beyond the level before ({} m)", level.distance, previous);
		}
		else if (m_model)
		{
			why = Rendering::LodSelection::CompareBones(m_model->GetSkeleton(), level.model->GetSkeleton());
		}

		if (why)
		{
			error += std::format("{}level {} is left out: {}", error.empty() ? "" : "\n", i + 1, why.value());
			continue;
		}

		m_activeLods.push_back({ level.model, i + 1 });
		m_activeStarts.push_back(level.distance);
		previous = level.distance;
	}

	if (!error.empty())
	{
		m_lodError = std::move(error);
	}
}

OvRendering::Resources::Model* OvCore::ECS::Components::CModelRenderer::SelectLod(const std::optional<OvMaths::FVector3>& p_viewPosition, bool p_mainView)
{
	RefreshLods();

	auto& level = p_mainView ? m_lodLevel : m_otherLodLevel;

	if (!m_model || m_activeLods.empty())
	{
		level = 0;
		return m_model;
	}

	if (p_viewPosition)
	{
		// Measured to the centre of the bounds, not to the pivot, which can stand far from the geometry
		const auto& bounds = m_frustumBehaviour == EFrustumBehaviour::CUSTOM_BOUNDS ? m_customBoundingSphere : m_model->GetBoundingSphere();
		const float distance = Rendering::LodSelection::DistanceToBounds(p_viewPosition.value(), owner.transform.GetWorldMatrix(), bounds);
		level = Rendering::LodSelection::SelectLevel(distance, m_activeStarts, m_lodBias, level);
	}

	level = std::min(level, static_cast<uint32_t>(m_activeLods.size()));
	return level == 0 ? m_model : m_activeLods[level - 1].model;
}

void OvCore::ECS::Components::CModelRenderer::ForgetModel(const OvRendering::Resources::Model* p_model)
{
	if (!p_model)
	{
		return;
	}

	for (uint32_t i = 0; i < m_lodCount; ++i)
	{
		if (m_lods[i].model == p_model)
		{
			m_lods[i].model = nullptr;
			m_lodsDirty = true;
			++m_lodRevision;
		}
	}

	if (m_model == p_model)
	{
		SetModel(nullptr);
	}
}

void OvCore::ECS::Components::CModelRenderer::OnSerialize(tinyxml2::XMLDocument & p_doc, tinyxml2::XMLNode * p_node)
{
	OvCore::Helpers::Serializer::SerializeModel(p_doc, p_node, "model", m_model);
	OvCore::Helpers::Serializer::SerializeInt(p_doc, p_node, "frustum_behaviour", reinterpret_cast<int&>(m_frustumBehaviour));
	OvCore::Helpers::Serializer::SerializeVec3(p_doc, p_node, "custom_bounding_sphere_position", m_customBoundingSphere.position);
	OvCore::Helpers::Serializer::SerializeFloat(p_doc, p_node, "custom_bounding_sphere_radius", m_customBoundingSphere.radius);
	OvCore::Helpers::Serializer::SerializeFloat(p_doc, p_node, "max_draw_distance", m_maxDrawDistance);

	tinyxml2::XMLNode* lodsNode = p_doc.NewElement("lods");
	p_node->InsertEndChild(lodsNode);
	for (uint32_t i = 0; i < m_lodCount; ++i)
	{
		tinyxml2::XMLNode* lodNode = p_doc.NewElement("lod");
		lodsNode->InsertEndChild(lodNode);
		OvCore::Helpers::Serializer::SerializeModel(p_doc, lodNode, "model", m_lods[i].model);
		OvCore::Helpers::Serializer::SerializeFloat(p_doc, lodNode, "distance", m_lods[i].distance);
	}

	OvCore::Helpers::Serializer::SerializeFloat(p_doc, p_node, "lod_bias", m_lodBias);
}

void OvCore::ECS::Components::CModelRenderer::OnDeserialize(tinyxml2::XMLDocument & p_doc, tinyxml2::XMLNode* p_node)
{
	OvRendering::Resources::Model* model = nullptr;
	OvCore::Helpers::Serializer::DeserializeModel(p_doc, p_node, "model", model);
	SetModel(model);

	OvCore::Helpers::Serializer::DeserializeInt(p_doc, p_node, "frustum_behaviour", reinterpret_cast<int&>(m_frustumBehaviour));
	OvCore::Helpers::Serializer::DeserializeVec3(p_doc, p_node, "custom_bounding_sphere_position", m_customBoundingSphere.position);
	OvCore::Helpers::Serializer::DeserializeFloat(p_doc, p_node, "custom_bounding_sphere_radius", m_customBoundingSphere.radius);
	OvCore::Helpers::Serializer::DeserializeFloat(p_doc, p_node, "max_draw_distance", m_maxDrawDistance);

	std::vector<LodLevel> levels;
	if (tinyxml2::XMLElement* lodsNode = p_node->FirstChildElement("lods"))
	{
		for (auto lodNode = lodsNode->FirstChildElement("lod"); lodNode && levels.size() < kMaxLodLevels; lodNode = lodNode->NextSiblingElement("lod"))
		{
			LodLevel level;
			OvCore::Helpers::Serializer::DeserializeModel(p_doc, lodNode, "model", level.model);
			OvCore::Helpers::Serializer::DeserializeFloat(p_doc, lodNode, "distance", level.distance);
			levels.push_back(level);
		}
	}
	SetLods(levels);

	float bias = 1.0f;
	OvCore::Helpers::Serializer::DeserializeFloat(p_doc, p_node, "lod_bias", bias);
	SetLodBias(bias);
}

void OvCore::ECS::Components::CModelRenderer::OnInspector(OvUI::Internal::WidgetContainer& p_root)
{
	using namespace OvCore::Helpers;

	GUIDrawer::DrawMesh(p_root, "Model", m_model, &m_modelChangedEvent);

	GUIDrawer::CreateTitle(p_root, "Frustum Culling Behaviour");
	auto& boundingMode = p_root.CreateWidget<OvUI::Widgets::Selection::ComboBox>(0);
	boundingMode.choices.emplace(0, "Disabled");
	// [[deprecated]] boundingMode.choices.emplace(1, "Model Bounds");
	boundingMode.choices.emplace(2, "Mesh Bounds");
	boundingMode.choices.emplace(3, "Custom Bounds");
	auto& boundingModeDispatcher = boundingMode.AddPlugin<OvUI::Plugins::DataDispatcher<int>>();
	boundingModeDispatcher.RegisterReference(reinterpret_cast<int&>(m_frustumBehaviour));

	auto& centerLabel = p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>("Bounding Sphere Center", OVUI_STYLE(InspectorTitle));
	auto& centerWidget = p_root.CreateWidget<OvUI::Widgets::Drags::DragMultipleScalars<float, 3>>(GUIDrawer::GetDataType<float>(), GUIDrawer::_MIN_FLOAT, GUIDrawer::_MAX_FLOAT, 0.f, 0.05f, "", GUIDrawer::GetFormat<float>());
	auto& centerDispatcher = centerWidget.AddPlugin<OvUI::Plugins::DataDispatcher<std::array<float, 3>>>();
	centerDispatcher.RegisterReference(reinterpret_cast<std::array<float, 3>&>(m_customBoundingSphere.position));

	auto& radiusLabel = p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>("Bounding Sphere Radius", OVUI_STYLE(InspectorTitle));
	auto& radiusWidget = p_root.CreateWidget<OvUI::Widgets::Drags::DragFloat>(0.0f, GUIDrawer::_MAX_FLOAT, 0.f, 0.1f);
	auto& radiusDispatcher = radiusWidget.AddPlugin<OvUI::Plugins::DataDispatcher<float>>();
	radiusDispatcher.RegisterReference(m_customBoundingSphere.radius);

	boundingMode.ValueChangedEvent += [&](int p_choice)
	{
		centerLabel.enabled = centerWidget.enabled = radiusLabel.enabled = radiusWidget.enabled = p_choice == 3;
	};

	centerLabel.enabled = centerWidget.enabled = radiusLabel.enabled = radiusWidget.enabled = m_frustumBehaviour == EFrustumBehaviour::CUSTOM_BOUNDS;

	GUIDrawer::DrawScalar<float>(p_root, "Max Draw Distance (0: always)", m_maxDrawDistance, 0.5f, 0.0f, GUIDrawer::_MAX_FLOAT);

	GUIDrawer::CreateTitle(p_root, "Levels of Detail");
	auto& lodGroup = p_root.CreateWidget<OvUI::Widgets::Layout::Group>();
	BuildLodWidgets(lodGroup);

	GUIDrawer::DrawScalar<float>(p_root, "LOD Bias",
		[this] { return m_lodBias; },
		[this](float p_value) { SetLodBias(p_value); },
		0.01f, 0.01f, 100.0f
	);

	auto& status = p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>("", OVUI_STYLE(Success));
	auto& details = p_root.CreateWidget<OvUI::Widgets::Texts::Text>("");

	status.AddPlugin<OvUI::Plugins::DataDispatcher<std::string>>().RegisterGatherer([this, &status, &lodGroup]
	{
		if (m_inspectedLodRevision != m_lodRevision)
		{
			BuildLodWidgets(lodGroup);
		}

		const auto error = GetLodError();
		status.color = error ? OVUI_STYLE(Danger) : OVUI_STYLE(Success);
		return error ? error.value() : std::string{ "Ready" };
	});

	details.AddPlugin<OvUI::Plugins::DataDispatcher<std::string>>().RegisterGatherer([this]
	{
		return m_lodCount == 0 ? std::string{ "No lighter level" } : std::format("Level {} drawn", GetLodLevel());
	});
}

void OvCore::ECS::Components::CModelRenderer::BuildLodWidgets(OvUI::Internal::WidgetContainer& p_container)
{
	using namespace OvCore::Helpers;

	for (auto& widget : p_container.GetWidgets())
	{
		widget.first->Destroy();
	}

	m_inspectedLodRevision = m_lodRevision;

	for (uint32_t i = 0; i < m_lodCount; ++i)
	{
		GUIDrawer::DrawMesh(p_container, std::format("Level {}", i + 1), m_lods[i].model, &m_lodChangedEvent);
		GUIDrawer::DrawScalar<float>(p_container, std::format("Level {} From (m)", i + 1),
			[this, i] { return m_lods[i].distance; },
			[this, i](float p_value) { m_lods[i].distance = std::max(p_value, 0.0f); m_lodsDirty = true; },
			0.5f, 0.0f, GUIDrawer::_MAX_FLOAT
		);

		auto& remove = p_container.CreateWidget<OvUI::Widgets::Buttons::Button>(std::format("Remove Level {}", i + 1));
		remove.ClickedEvent += [this, i]
		{
			auto levels = GetLods();
			levels.erase(levels.begin() + i);
			SetLods(levels);
		};
	}

	if (m_lodCount < kMaxLodLevels)
	{
		auto& add = p_container.CreateWidget<OvUI::Widgets::Buttons::Button>("Add Level");
		add.ClickedEvent += [this]
		{
			auto levels = GetLods();
			levels.push_back({ nullptr, levels.empty() ? 20.0f : levels.back().distance * 2.0f });
			SetLods(levels);
		};
	}
}
