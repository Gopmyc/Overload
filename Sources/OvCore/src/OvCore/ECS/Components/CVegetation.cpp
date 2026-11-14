/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <format>
#include <limits>

#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CPhysicalHeightField.h>
#include <OvCore/ECS/Components/CTerrain.h>
#include <OvCore/ECS/Components/CVegetation.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Helpers/GUIDrawer.h>
#include <OvCore/Helpers/Serializer.h>
#include <OvCore/Rendering/FrameBuilder.h>
#include <OvCore/ResourceManagement/MaterialManager.h>
#include <OvCore/Resources/Material.h>
#include <OvCore/SceneSystem/SceneManager.h>
#include <OvCore/Terrain/Heightmap.h>
#include <OvMaths/FQuaternion.h>
#include <OvMaths/FVector4.h>
#include <OvRendering/Resources/Model.h>
#include <OvUI/Plugins/DataDispatcher.h>
#include <OvUI/Styling/Style.h>
#include <OvUI/Widgets/Buttons/Button.h>
#include <OvUI/Widgets/Layout/Group.h>
#include <OvUI/Widgets/Texts/Text.h>
#include <OvUI/Widgets/Texts/TextColored.h>

namespace
{
	constexpr uint32_t kDefaultChunkCells = 32;
	constexpr uint32_t kMaxDensityMapSize = 4096;
	constexpr float kHeightLimit = 10000.0f;
	constexpr std::string_view kDefaultMaterial = ":Materials/Default.ovmat";

	OvMaths::FVector3 Transform(const OvMaths::FMatrix4& p_matrix, const OvMaths::FVector3& p_point)
	{
		const auto result = p_matrix * OvMaths::FVector4{ p_point.x, p_point.y, p_point.z, 1.0f };
		return { result.x, result.y, result.z };
	}

	// The largest factor a matrix stretches a length by
	float LargestScale(const OvMaths::FMatrix4& p_matrix)
	{
		const float* m = p_matrix.data;
		const float x = std::sqrt(m[0] * m[0] + m[4] * m[4] + m[8] * m[8]);
		const float y = std::sqrt(m[1] * m[1] + m[5] * m[5] + m[9] * m[9]);
		const float z = std::sqrt(m[2] * m[2] + m[6] * m[6] + m[10] * m[10]);
		return std::max({ x, y, z });
	}

	OvCore::ECS::Components::CVegetation::Layer MakeDefaultLayer(uint32_t p_index)
	{
		OvCore::ECS::Components::CVegetation::Layer layer;
		layer.name = std::format("Layer {}", p_index + 1);

		if (OvCore::Global::ServiceLocator::Has<OvCore::ResourceManagement::MaterialManager>())
		{
			layer.material = OVSERVICE(OvCore::ResourceManagement::MaterialManager).GetResource(std::string{ kDefaultMaterial });
		}

		return layer;
	}
}

OvCore::ECS::Components::CVegetation::CVegetation(ECS::Actor& p_owner) : AComponent(p_owner)
{
	m_layerChangedEvent += [this]
	{
		m_layerDirty.fill(true);
	};
}

std::string OvCore::ECS::Components::CVegetation::GetName()
{
	return "Vegetation";
}

std::string OvCore::ECS::Components::CVegetation::GetTypeName()
{
	return std::string{ ComponentTraits<CVegetation>::Name };
}

std::optional<uint32_t> OvCore::ECS::Components::CVegetation::AddLayer(const Layer& p_layer)
{
	if (m_layerCount >= kMaxLayers)
	{
		return std::nullopt;
	}

	const uint32_t index = m_layerCount++;
	m_layers[index] = p_layer;
	m_states[index] = {};
	m_layerDirty[index] = true;
	++m_revision;
	return index;
}

void OvCore::ECS::Components::CVegetation::RemoveLayer(uint32_t p_index)
{
	if (p_index >= m_layerCount)
	{
		return;
	}

	for (uint32_t i = p_index; i + 1 < m_layerCount; ++i)
	{
		m_layers[i] = std::move(m_layers[i + 1]);
		m_states[i] = std::move(m_states[i + 1]);
		m_layerDirty[i] = m_layerDirty[i + 1];
	}

	--m_layerCount;
	m_layers[m_layerCount] = {};
	m_states[m_layerCount] = {};
	++m_revision;
}

uint32_t OvCore::ECS::Components::CVegetation::GetLayerCount() const
{
	return m_layerCount;
}

std::optional<OvCore::ECS::Components::CVegetation::Layer> OvCore::ECS::Components::CVegetation::GetLayer(uint32_t p_index) const
{
	return p_index < m_layerCount ? std::optional<Layer>{ m_layers[p_index] } : std::nullopt;
}

void OvCore::ECS::Components::CVegetation::SetLayer(uint32_t p_index, const Layer& p_layer)
{
	if (p_index >= m_layerCount)
	{
		return;
	}

	// A density map image named takes the place of weights a script gave
	if (p_layer.densityMap != m_layers[p_index].densityMap && !p_layer.densityMap.empty())
	{
		m_states[p_index].scriptMap.clear();
		m_states[p_index].scriptMapSize = 0;
	}

	m_layers[p_index] = p_layer;
	m_layerDirty[p_index] = true;
	++m_revision;
}

std::optional<std::string> OvCore::ECS::Components::CVegetation::SetDensityMap(uint32_t p_index, uint32_t p_size, std::span<const float> p_weights)
{
	if (p_index >= m_layerCount)
	{
		return std::format("no layer {}", p_index);
	}

	auto& state = m_states[p_index];

	if (p_size == 0)
	{
		state.scriptMap.clear();
		state.scriptMapSize = 0;
		m_layerDirty[p_index] = true;
		return std::nullopt;
	}

	if (p_size < 2 || p_size > kMaxDensityMapSize)
	{
		return std::format("a density map is 2 to {} weights a side, not {}", kMaxDensityMapSize, p_size);
	}

	if (p_weights.size() != static_cast<size_t>(p_size) * p_size)
	{
		return std::format("{} weights for a map of {} a side: {} expected", p_weights.size(), p_size, static_cast<size_t>(p_size) * p_size);
	}

	if (std::any_of(p_weights.begin(), p_weights.end(), [](float p_weight) { return !std::isfinite(p_weight); }))
	{
		return std::string{ "a weight is not a number" };
	}

	state.scriptMap.assign(p_weights.begin(), p_weights.end());
	for (float& weight : state.scriptMap)
	{
		weight = std::clamp(weight, 0.0f, 1.0f);
	}
	state.scriptMapSize = p_size;

	// The weights take the place of the image
	m_layers[p_index].densityMap.clear();
	m_layerDirty[p_index] = true;
	++m_revision;
	return std::nullopt;
}

OvCore::ECS::Components::CPhysicalHeightField* OvCore::ECS::Components::CVegetation::GetField() const
{
	return owner.GetComponent<CPhysicalHeightField>();
}

std::optional<std::string> OvCore::ECS::Components::CVegetation::GetError()
{
	const auto* field = GetField();
	if (!field)
	{
		return std::string{ "no Physical Height Field on the actor" };
	}

	if (field->GetHeights().empty())
	{
		return field->GetHeightmapError().value_or("the height field has no heights");
	}

	Refresh();

	std::string errors;
	for (uint32_t i = 0; i < m_layerCount; ++i)
	{
		RefreshLayer(i);

		if (const auto& error = m_states[i].error)
		{
			errors += std::format("{}layer {} ('{}'): {}", errors.empty() ? "" : "\n", i, m_layers[i].name, error.value());
		}
	}

	return errors.empty() ? std::nullopt : std::optional<std::string>{ errors };
}

uint32_t OvCore::ECS::Components::CVegetation::GetInstanceCount(std::optional<uint32_t> p_layer) const
{
	if (p_layer)
	{
		return p_layer.value() < m_layerCount ? m_states[p_layer.value()].drawnCount : 0;
	}

	uint32_t total = 0;
	for (uint32_t i = 0; i < m_layerCount; ++i)
	{
		total += m_states[i].drawnCount;
	}
	return total;
}

void OvCore::ECS::Components::CVegetation::Refresh()
{
	const auto* field = GetField();
	const uint64_t heights = field ? field->GetHeightsRevision() : 0;
	const auto* terrain = owner.GetComponent<CTerrain>();
	const uint32_t chunkSize = terrain ? terrain->GetChunkSize() : kDefaultChunkCells;
	const auto& world = owner.transform.GetWorldMatrix();

	if (!m_dirty && field == m_builtField && heights == m_builtHeights && chunkSize == m_builtChunkSize &&
		std::memcmp(world.data, m_builtWorld.data, sizeof(world.data)) == 0)
	{
		return;
	}

	m_dirty = false;
	m_builtField = field;
	m_builtHeights = heights;
	m_builtChunkSize = chunkSize;
	m_builtWorld = world;
	m_chunkBoxes.clear();
	m_layout = {};

	if (field && !field->GetHeights().empty())
	{
		// The chunks of the terrain drawn on the same field, when there is one
		m_layout = Terrain::TerrainLayout::Make(field->GetSampleCount(), field->GetSpacing(), chunkSize, 1);

		for (uint32_t chunk = 0; chunk < m_layout.GetChunkCount(); ++chunk)
		{
			m_chunkBoxes.push_back(Terrain::ComputeChunkBox(field->GetHeights(), m_layout, chunk, 0.0f));
		}
	}

	// Every layer is scattered again, chunk by chunk as the camera reaches them
	m_layerDirty.fill(true);
}

void OvCore::ECS::Components::CVegetation::RefreshLayer(uint32_t p_index)
{
	if (!m_layerDirty[p_index])
	{
		return;
	}

	m_layerDirty[p_index] = false;

	auto& layer = m_layers[p_index];
	auto& state = m_states[p_index];
	state.chunks.assign(m_layout.GetChunkCount(), LayerChunk{});
	state.error.reset();

	// The density map image, read again when its name changes
	if (layer.densityMap != state.loadedImageMap)
	{
		state.loadedImageMap = layer.densityMap;
		state.imageMap.clear();
		state.imageMapSize = 0;

		if (!layer.densityMap.empty())
		{
			std::filesystem::path file = layer.densityMap;
			if (Global::ServiceLocator::Has<SceneSystem::SceneManager>())
			{
				if (const auto* scene = OVSERVICE(SceneSystem::SceneManager).GetCurrentScene())
				{
					file = scene->GetRealAssetPath(layer.densityMap);
				}
			}

			Terrain::HeightmapImage image;
			if (auto error = Terrain::LoadHeightmap(file, image))
			{
				state.loadedImageMap.clear(); // Tried again when the layer changes
				state.error = std::format("density map '{}': {}", layer.densityMap, error.value());
				return;
			}

			state.imageMap = std::move(image.values);
			state.imageMapSize = image.size;
		}
	}

	if (!layer.model)
	{
		state.error = std::string{ "no model" };
	}
	else if (!layer.material || !layer.material->HasShader())
	{
		state.error = std::string{ "no material" };
	}
	else if (!layer.material->HasEngineCapability(std::string{ Rendering::FrameBuilder::kInstancingCapabilityName }))
	{
		state.error = std::format("its material '{}' cannot draw instances: its shader does not declare {}", layer.material->path, Rendering::FrameBuilder::kInstancingCapabilityName);
	}
}

void OvCore::ECS::Components::CVegetation::BuildChunk(uint32_t p_index, uint32_t p_chunk)
{
	const auto& layer = m_layers[p_index];
	auto& state = m_states[p_index];
	auto& chunk = state.chunks[p_chunk];
	chunk.built = true;

	const bool scripted = state.scriptMapSize > 0;
	std::vector<Terrain::ScatterInstance> scattered;
	Terrain::ScatterChunk(
		m_builtField->GetHeights(), m_layout, p_chunk, layer.scatter,
		scripted ? state.scriptMap : state.imageMap,
		scripted ? state.scriptMapSize : state.imageMapSize,
		scattered
	);

	chunk.instances.clear();
	chunk.instances.reserve(scattered.size());

	for (const auto& instance : scattered)
	{
		const auto model =
			m_builtWorld *
			OvMaths::FMatrix4::Translation(instance.position) *
			OvMaths::FMatrix4::Rotation(instance.rotation) *
			OvMaths::FMatrix4::Scaling({ instance.scale, instance.scale, instance.scale });

		chunk.instances.push_back(Rendering::PrepareInstance(model, OvMaths::FMatrix4::Identity));
	}

	// The bounds in the world, for the frustum
	const auto local = Terrain::BoundInstances(scattered, layer.model->GetBoundingSphere());
	chunk.bounds = { Transform(m_builtWorld, local.position), local.radius * LargestScale(m_builtWorld) };
}

void OvCore::ECS::Components::CVegetation::PrepareDraw(const std::optional<OvMaths::FVector3>& p_viewPosition, bool p_mainView, std::vector<LayerDraw>& p_draws)
{
	Refresh();

	const bool ready = m_builtField && !m_chunkBoxes.empty();

	for (uint32_t i = 0; i < m_layerCount; ++i)
	{
		auto& state = m_states[i];
		uint32_t drawn = 0;

		if (ready)
		{
			RefreshLayer(i);
		}

		if (ready && !state.error)
		{
			const auto& layer = m_layers[i];

			for (uint32_t chunk = 0; chunk < m_chunkBoxes.size(); ++chunk)
			{
				// Within reach: the distance to the chunk's ground, carried into the world
				if (p_viewPosition)
				{
					const auto& box = m_chunkBoxes[chunk];
					OvMaths::FVector3 low{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
					OvMaths::FVector3 high{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };

					for (uint32_t corner = 0; corner < 8; ++corner)
					{
						const auto point = Transform(m_builtWorld, {
							(corner & 1) ? box.max.x : box.min.x,
							(corner & 2) ? box.max.y : box.min.y,
							(corner & 4) ? box.max.z : box.min.z
						});
						low = { std::min(low.x, point.x), std::min(low.y, point.y), std::min(low.z, point.z) };
						high = { std::max(high.x, point.x), std::max(high.y, point.y), std::max(high.z, point.z) };
					}

					if (Terrain::DistanceToBox(p_viewPosition.value(), low, high) > layer.drawDistance)
					{
						continue;
					}
				}

				auto& data = state.chunks[chunk];
				if (!data.built)
				{
					BuildChunk(i, chunk);
				}

				if (data.instances.empty())
				{
					continue;
				}

				p_draws.push_back(LayerDraw{
					.model = layer.model,
					.material = layer.material,
					.instances = data.instances.data(),
					.count = static_cast<uint32_t>(data.instances.size()),
					.bounds = data.bounds,
					.castShadows = layer.castShadows
				});

				drawn += static_cast<uint32_t>(data.instances.size());
			}
		}

		if (p_mainView)
		{
			state.drawnCount = drawn;
		}
	}
}

void OvCore::ECS::Components::CVegetation::ForgetModel(const OvRendering::Resources::Model* p_model)
{
	for (uint32_t i = 0; i < m_layerCount; ++i)
	{
		if (p_model && m_layers[i].model == p_model)
		{
			m_layers[i].model = nullptr;
			m_layerDirty[i] = true;
			++m_revision;
		}
	}
}

void OvCore::ECS::Components::CVegetation::ForgetMaterial(const OvCore::Resources::Material* p_material)
{
	for (uint32_t i = 0; i < m_layerCount; ++i)
	{
		if (p_material && m_layers[i].material == p_material)
		{
			m_layers[i].material = nullptr;
			m_layerDirty[i] = true;
			++m_revision;
		}
	}
}

void OvCore::ECS::Components::CVegetation::OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	using OvCore::Helpers::Serializer;

	tinyxml2::XMLNode* layersNode = p_doc.NewElement("layers");
	p_node->InsertEndChild(layersNode);

	for (uint32_t i = 0; i < m_layerCount; ++i)
	{
		const auto& layer = m_layers[i];
		tinyxml2::XMLNode* node = p_doc.NewElement("layer");
		layersNode->InsertEndChild(node);

		Serializer::SerializeString(p_doc, node, "name", layer.name);
		Serializer::SerializeModel(p_doc, node, "model", layer.model);
		Serializer::SerializeMaterial(p_doc, node, "material", layer.material);
		Serializer::SerializeFloat(p_doc, node, "density", layer.scatter.density);
		Serializer::SerializeFloat(p_doc, node, "min_scale", layer.scatter.minScale);
		Serializer::SerializeFloat(p_doc, node, "max_scale", layer.scatter.maxScale);
		Serializer::SerializeBoolean(p_doc, node, "random_yaw", layer.scatter.randomYaw);
		Serializer::SerializeFloat(p_doc, node, "slope_alignment", layer.scatter.slopeAlignment);
		Serializer::SerializeFloat(p_doc, node, "max_slope", layer.scatter.maxSlope);
		Serializer::SerializeFloat(p_doc, node, "min_height", layer.scatter.minHeight);
		Serializer::SerializeFloat(p_doc, node, "max_height", layer.scatter.maxHeight);
		Serializer::SerializeInt(p_doc, node, "seed", static_cast<int>(layer.scatter.seed));
		Serializer::SerializeFloat(p_doc, node, "draw_distance", layer.drawDistance);
		Serializer::SerializeBoolean(p_doc, node, "cast_shadows", layer.castShadows);
		Serializer::SerializeString(p_doc, node, "density_map", layer.densityMap);
	}
}

void OvCore::ECS::Components::CVegetation::OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	using OvCore::Helpers::Serializer;

	while (m_layerCount > 0)
	{
		RemoveLayer(m_layerCount - 1);
	}

	tinyxml2::XMLElement* layersNode = p_node->FirstChildElement("layers");
	if (!layersNode)
	{
		return;
	}

	for (auto node = layersNode->FirstChildElement("layer"); node && m_layerCount < kMaxLayers; node = node->NextSiblingElement("layer"))
	{
		Layer layer = MakeDefaultLayer(m_layerCount);
		int seed = static_cast<int>(layer.scatter.seed);

		Serializer::DeserializeString(p_doc, node, "name", layer.name);
		Serializer::DeserializeModel(p_doc, node, "model", layer.model);
		Serializer::DeserializeMaterial(p_doc, node, "material", layer.material);
		Serializer::DeserializeFloat(p_doc, node, "density", layer.scatter.density);
		Serializer::DeserializeFloat(p_doc, node, "min_scale", layer.scatter.minScale);
		Serializer::DeserializeFloat(p_doc, node, "max_scale", layer.scatter.maxScale);
		Serializer::DeserializeBoolean(p_doc, node, "random_yaw", layer.scatter.randomYaw);
		Serializer::DeserializeFloat(p_doc, node, "slope_alignment", layer.scatter.slopeAlignment);
		Serializer::DeserializeFloat(p_doc, node, "max_slope", layer.scatter.maxSlope);
		Serializer::DeserializeFloat(p_doc, node, "min_height", layer.scatter.minHeight);
		Serializer::DeserializeFloat(p_doc, node, "max_height", layer.scatter.maxHeight);
		Serializer::DeserializeInt(p_doc, node, "seed", seed);
		Serializer::DeserializeFloat(p_doc, node, "draw_distance", layer.drawDistance);
		Serializer::DeserializeBoolean(p_doc, node, "cast_shadows", layer.castShadows);
		Serializer::DeserializeString(p_doc, node, "density_map", layer.densityMap);

		layer.scatter.seed = static_cast<uint32_t>(seed);
		layer.scatter.density = std::clamp(layer.scatter.density, 0.0f, Terrain::kMaxScatterDensity);
		layer.drawDistance = std::max(layer.drawDistance, 0.0f);
		AddLayer(layer);
	}
}

void OvCore::ECS::Components::CVegetation::OnInspector(OvUI::Internal::WidgetContainer& p_root)
{
	using namespace OvCore::Helpers;

	// A title, then the layers beside it: the status and the count share the next row
	GUIDrawer::CreateTitle(p_root, "Layers");
	auto& layersGroup = p_root.CreateWidget<OvUI::Widgets::Layout::Group>();
	BuildLayerWidgets(layersGroup);

	auto& status = p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>("", OVUI_STYLE(Success));
	auto& details = p_root.CreateWidget<OvUI::Widgets::Texts::Text>("");

	status.AddPlugin<OvUI::Plugins::DataDispatcher<std::string>>().RegisterGatherer([this, &status, &layersGroup]
	{
		if (m_inspectedRevision != m_revision)
		{
			BuildLayerWidgets(layersGroup);
		}

		const auto error = GetError();
		status.color = error ? OVUI_STYLE(Danger) : OVUI_STYLE(Success);
		return error ? error.value() : std::string{ "Ready" };
	});

	details.AddPlugin<OvUI::Plugins::DataDispatcher<std::string>>().RegisterGatherer([this]
	{
		return std::format("{} layers, {} instances drawn", m_layerCount, GetInstanceCount());
	});
}

void OvCore::ECS::Components::CVegetation::BuildLayerWidgets(OvUI::Internal::WidgetContainer& p_container)
{
	using namespace OvCore::Helpers;

	for (auto& widget : p_container.GetWidgets())
	{
		widget.first->Destroy();
	}

	m_inspectedRevision = m_revision;

	for (uint32_t i = 0; i < m_layerCount; ++i)
	{
		auto& layer = m_layers[i];
		const auto changed = [this, i] { m_layerDirty[i] = true; };

		GUIDrawer::CreateTitle(p_container, std::format("Layer {}", i));
		GUIDrawer::DrawString(p_container, "Name", [this, i] { return m_layers[i].name; }, [this, i](std::string p_value) { m_layers[i].name = p_value; });
		GUIDrawer::DrawMesh(p_container, "Model", layer.model, &m_layerChangedEvent);
		GUIDrawer::DrawMaterial(p_container, "Material", layer.material, &m_layerChangedEvent);

		const auto drawFloat = [&](const std::string& p_name, float& p_value, float p_step, float p_min, float p_max)
		{
			GUIDrawer::DrawScalar<float>(p_container, p_name,
				[&p_value] { return p_value; },
				[&p_value, changed, p_min, p_max](float p_new) { p_value = std::clamp(p_new, p_min, p_max); changed(); },
				p_step, p_min, p_max
			);
		};

		drawFloat("Density (per m²)", layer.scatter.density, 0.05f, 0.0f, Terrain::kMaxScatterDensity);
		drawFloat("Min Scale", layer.scatter.minScale, 0.01f, 0.0f, 100.0f);
		drawFloat("Max Scale", layer.scatter.maxScale, 0.01f, 0.0f, 100.0f);
		GUIDrawer::DrawBoolean(p_container, "Random Yaw",
			[&layer] { return layer.scatter.randomYaw; },
			[&layer, changed](bool p_value) { layer.scatter.randomYaw = p_value; changed(); }
		);
		drawFloat("Slope Alignment", layer.scatter.slopeAlignment, 0.01f, 0.0f, 1.0f);
		drawFloat("Max Slope (°)", layer.scatter.maxSlope, 0.5f, 0.0f, 90.0f);
		drawFloat("Min Height", layer.scatter.minHeight, 0.1f, -kHeightLimit, kHeightLimit);
		drawFloat("Max Height", layer.scatter.maxHeight, 0.1f, -kHeightLimit, kHeightLimit);
		GUIDrawer::DrawScalar<int>(p_container, "Seed",
			[&layer] { return static_cast<int>(layer.scatter.seed); },
			[&layer, changed](int p_value) { layer.scatter.seed = static_cast<uint32_t>(p_value); changed(); },
			1.0f, 0, std::numeric_limits<int>::max()
		);
		drawFloat("Draw Distance", layer.drawDistance, 0.5f, 0.0f, 100000.0f);
		GUIDrawer::DrawBoolean(p_container, "Cast Shadows",
			[&layer] { return layer.castShadows; },
			[&layer](bool p_value) { layer.castShadows = p_value; }
		);
		GUIDrawer::DrawAsset(p_container, "Density Map",
			[&layer] { return layer.densityMap; },
			[this, i](std::string p_value) { auto edited = m_layers[i]; edited.densityMap = p_value; SetLayer(i, edited); },
			OvTools::Utils::PathParser::EFileType::TEXTURE
		);

		auto& remove = p_container.CreateWidget<OvUI::Widgets::Buttons::Button>(std::format("Remove Layer {}", i));
		remove.ClickedEvent += [this, i] { RemoveLayer(i); };
	}

	if (m_layerCount < kMaxLayers)
	{
		auto& add = p_container.CreateWidget<OvUI::Widgets::Buttons::Button>("Add Layer");
		add.ClickedEvent += [this] { AddLayer(MakeDefaultLayer(m_layerCount)); };
	}
}
