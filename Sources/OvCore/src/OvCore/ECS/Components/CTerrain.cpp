/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>

#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CPhysicalHeightField.h>
#include <OvCore/ECS/Components/CTerrain.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Helpers/GUIDrawer.h>
#include <OvCore/Helpers/Serializer.h>
#include <OvCore/ResourceManagement/MaterialManager.h>
#include <OvCore/ResourceManagement/TextureManager.h>
#include <OvCore/Resources/Material.h>
#include <OvMaths/FMatrix4.h>
#include <OvMaths/FVector4.h>
#include <OvRendering/Resources/Loaders/TextureLoader.h>
#include <OvRendering/Resources/Mesh.h>
#include <OvRendering/Resources/Texture.h>
#include <OvUI/Plugins/DataDispatcher.h>
#include <OvUI/Styling/Style.h>
#include <OvUI/Widgets/Texts/TextColored.h>

namespace
{
	constexpr std::string_view kDefaultMaterialPath = ":Materials/Terrain.ovmat";
	constexpr std::string_view kAlbedoMapUniform = "u_AlbedoMap";

	uint8_t ToByte(float p_value)
	{
		return static_cast<uint8_t>(std::lround(std::clamp(p_value, 0.0f, 1.0f) * 255.0f));
	}
}

OvCore::ECS::Components::CTerrain::CTerrain(ECS::Actor& p_owner) :
	AComponent(p_owner),
	m_materialPath(kDefaultMaterialPath)
{
}

OvCore::ECS::Components::CTerrain::~CTerrain()
{
	ReleaseColorTexture();
}

std::string OvCore::ECS::Components::CTerrain::GetName()
{
	return "Terrain";
}

std::string OvCore::ECS::Components::CTerrain::GetTypeName()
{
	return std::string{ ComponentTraits<CTerrain>::Name };
}

void OvCore::ECS::Components::CTerrain::SetChunkSize(uint32_t p_cells)
{
	m_chunkSize = std::clamp(p_cells, 1u, 256u);
	m_dirty = true;
}

uint32_t OvCore::ECS::Components::CTerrain::GetChunkSize() const
{
	return m_chunkSize;
}

void OvCore::ECS::Components::CTerrain::SetLodCount(uint32_t p_count)
{
	m_lodCount = std::clamp(p_count, 1u, Terrain::kMaxLodCount);
	m_dirty = true;
}

uint32_t OvCore::ECS::Components::CTerrain::GetLodCount() const
{
	return m_lodCount;
}

void OvCore::ECS::Components::CTerrain::SetLodDistance(float p_distance)
{
	m_lodDistance = std::max(p_distance, 0.0f);
}

float OvCore::ECS::Components::CTerrain::GetLodDistance() const
{
	return m_lodDistance;
}

void OvCore::ECS::Components::CTerrain::SetMaterialPath(const std::string& p_path)
{
	m_materialPath = p_path.empty() ? std::string{ kDefaultMaterialPath } : p_path;
	m_materialDirty = true;
}

const std::string& OvCore::ECS::Components::CTerrain::GetMaterialPath() const
{
	return m_materialPath;
}

void OvCore::ECS::Components::CTerrain::SetColorMap(const std::string& p_path)
{
	m_colorMap = p_path;

	if (!m_colorMap.empty())
	{
		m_colorSize = 0;
		m_colors.clear();
		m_colorTextureDirty = true;
	}

	m_materialDirty = true;
}

const std::string& OvCore::ECS::Components::CTerrain::GetColorMap() const
{
	return m_colorMap;
}

bool OvCore::ECS::Components::CTerrain::SetColors(uint32_t p_sampleCount, const std::vector<float>& p_colors)
{
	if (p_colors.empty())
	{
		m_colorSize = 0;
		m_colors.clear();
	}
	else
	{
		const size_t samples = static_cast<size_t>(p_sampleCount) * p_sampleCount;
		if (p_sampleCount < 1 || p_colors.size() != samples * 3)
		{
			return false;
		}

		// Rows from the far edge, as an image loaded for the GPU: a sample's colour lands on its texture coordinates
		m_colorSize = p_sampleCount;
		m_colors.assign(samples * 4, 255);
		for (uint32_t z = 0; z < p_sampleCount; ++z)
		{
			for (uint32_t x = 0; x < p_sampleCount; ++x)
			{
				const size_t from = (static_cast<size_t>(z) * p_sampleCount + x) * 3;
				const size_t to = (static_cast<size_t>(p_sampleCount - 1 - z) * p_sampleCount + x) * 4;
				m_colors[to + 0] = ToByte(p_colors[from + 0]);
				m_colors[to + 1] = ToByte(p_colors[from + 1]);
				m_colors[to + 2] = ToByte(p_colors[from + 2]);
			}
		}

		m_colorMap.clear();
	}

	m_colorTextureDirty = true;
	m_materialDirty = true;
	return true;
}

bool OvCore::ECS::Components::CTerrain::HasColors() const
{
	return m_colorSize > 0;
}

OvCore::ECS::Components::CPhysicalHeightField* OvCore::ECS::Components::CTerrain::GetField() const
{
	return owner.GetComponent<CPhysicalHeightField>();
}

std::optional<std::string> OvCore::ECS::Components::CTerrain::GetError() const
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

	if (!Global::ServiceLocator::Has<ResourceManagement::MaterialManager>() ||
		!OVSERVICE(ResourceManagement::MaterialManager).GetResource(m_materialPath))
	{
		return std::format("no material '{}'", m_materialPath);
	}

	return std::nullopt;
}

void OvCore::ECS::Components::CTerrain::Refresh()
{
	const auto* field = GetField();
	const uint64_t revision = field ? field->GetHeightsRevision() : 0;

	if (!m_dirty && field == m_builtField && revision == m_builtRevision)
	{
		return;
	}

	m_dirty = false;
	m_builtField = field;
	m_builtRevision = revision;
	m_chunks.clear();
	m_triangleCount = 0;

	if (!field || field->GetHeights().empty())
	{
		m_layout = {};
		return;
	}

	const auto& heights = field->GetHeights();
	m_layout = Terrain::TerrainLayout::Make(field->GetSampleCount(), field->GetSpacing(), m_chunkSize, m_lodCount);
	m_chunks.resize(m_layout.GetChunkCount());

	for (uint32_t i = 0; i < m_layout.GetChunkCount(); ++i)
	{
		auto& chunk = m_chunks[i];
		chunk.skirtDepth = Terrain::ComputeSkirtDepth(heights, m_layout, i);
		chunk.box = Terrain::ComputeChunkBox(heights, m_layout, i, chunk.skirtDepth);
	}
}

uint32_t OvCore::ECS::Components::CTerrain::GetChunkCount()
{
	Refresh();
	return static_cast<uint32_t>(m_chunks.size());
}

std::optional<uint32_t> OvCore::ECS::Components::CTerrain::GetChunkLod(uint32_t p_chunk)
{
	Refresh();
	return p_chunk < m_chunks.size() ? std::optional<uint32_t>{ m_chunks[p_chunk].lod } : std::nullopt;
}

uint32_t OvCore::ECS::Components::CTerrain::GetTriangleCount() const
{
	return m_triangleCount;
}

void OvCore::ECS::Components::CTerrain::PrepareDraw(const std::optional<OvMaths::FVector3>& p_viewPosition, bool p_mainView, std::vector<ChunkDraw>& p_draws)
{
	Refresh();

	const auto* field = m_builtField;
	if (!field || m_chunks.empty())
	{
		return;
	}

	const auto& world = owner.transform.GetWorldMatrix();
	const auto& heights = field->GetHeights();
	Terrain::ChunkGeometry geometry;
	uint32_t triangleCount = 0;

	for (uint32_t i = 0; i < m_chunks.size(); ++i)
	{
		auto& chunk = m_chunks[i];
		auto& lod = p_mainView ? chunk.lod : chunk.otherLod;

		// The distance to the chunk's box in the world, not to the actor: the whole terrain stands on one actor
		if (p_viewPosition)
		{
			OvMaths::FVector3 low{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
			OvMaths::FVector3 high{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };

			for (uint32_t corner = 0; corner < 8; ++corner)
			{
				const OvMaths::FVector4 local{
					(corner & 1) ? chunk.box.max.x : chunk.box.min.x,
					(corner & 2) ? chunk.box.max.y : chunk.box.min.y,
					(corner & 4) ? chunk.box.max.z : chunk.box.min.z,
					1.0f
				};
				const auto point = world * local;
				low = { std::min(low.x, point.x), std::min(low.y, point.y), std::min(low.z, point.z) };
				high = { std::max(high.x, point.x), std::max(high.y, point.y), std::max(high.z, point.z) };
			}

			const float distance = Terrain::DistanceToBox(p_viewPosition.value(), low, high);
			lod = Terrain::SelectLod(distance, m_lodDistance, m_layout.lodCount, lod);
		}

		auto& mesh = chunk.meshes[lod];
		if (!mesh)
		{
			Terrain::BuildChunk(heights, m_layout, i, lod, chunk.skirtDepth, geometry);
			mesh = std::make_unique<OvRendering::Resources::Mesh>(geometry.vertices, geometry.indices);
			chunk.triangles[lod] = geometry.surfaceTriangles;
		}

		triangleCount += chunk.triangles[lod];

		const auto centre = (chunk.box.min + chunk.box.max) * 0.5f;
		p_draws.push_back(ChunkDraw{
			mesh.get(),
			OvRendering::Geometry::BoundingSphere{ centre, OvMaths::FVector3::Distance(centre, chunk.box.max) }
		});
	}

	if (p_mainView)
	{
		m_triangleCount = triangleCount;
	}
}

void OvCore::ECS::Components::CTerrain::ReleaseColorTexture()
{
	if (m_colorTexture)
	{
		OvRendering::Resources::Loaders::TextureLoader::Destroy(m_colorTexture);
		m_colorTexture = nullptr;
	}
}

OvRendering::Data::Material* OvCore::ECS::Components::CTerrain::ResolveMaterial()
{
	auto& materials = OVSERVICE(ResourceManagement::MaterialManager);
	auto* source = materials.GetResource(m_materialPath);
	if (!source || !source->HasShader())
	{
		return nullptr;
	}

	// A copy of its own, the colour map set on it: two terrains may share a material
	if (!m_material || m_materialDirty || m_builtMaterialPath != m_materialPath || m_materialRevision != materials.GetResourcesRevision())
	{
		if (!m_material)
		{
			m_material = std::make_unique<OvRendering::Data::Material>();
		}

		*m_material = *source;
		m_builtMaterialPath = m_materialPath;
		m_materialRevision = materials.GetResourcesRevision();
		m_materialDirty = false;

		if (m_colorTextureDirty)
		{
			ReleaseColorTexture();
			m_colorTextureDirty = false;

			if (m_colorSize > 0)
			{
				m_colorTexture = OvRendering::Resources::Loaders::TextureLoader::CreateFromMemory(
					m_colors.data(), m_colorSize, m_colorSize,
					baregl::types::ETextureFilteringMode::LINEAR_MIPMAP_LINEAR,
					baregl::types::ETextureFilteringMode::LINEAR,
					baregl::types::ETextureWrapMode::CLAMP_TO_EDGE,
					baregl::types::ETextureWrapMode::CLAMP_TO_EDGE,
					true
				);
			}
		}

		OvRendering::Resources::Texture* colorMap = m_colorTexture;
		if (!m_colorMap.empty() && Global::ServiceLocator::Has<ResourceManagement::TextureManager>())
		{
			colorMap = OVSERVICE(ResourceManagement::TextureManager).GetResource(m_colorMap);
		}

		if (colorMap)
		{
			m_material->TrySetProperty(std::string{ kAlbedoMapUniform }, colorMap);
		}
	}

	return m_material->IsValid() ? m_material.get() : nullptr;
}

void OvCore::ECS::Components::CTerrain::OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	Helpers::Serializer::SerializeInt(p_doc, p_node, "chunk_size", static_cast<int>(m_chunkSize));
	Helpers::Serializer::SerializeInt(p_doc, p_node, "lod_count", static_cast<int>(m_lodCount));
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "lod_distance", m_lodDistance);
	Helpers::Serializer::SerializeString(p_doc, p_node, "material", m_materialPath);
	Helpers::Serializer::SerializeString(p_doc, p_node, "color_map", m_colorMap);
}

void OvCore::ECS::Components::CTerrain::OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	int chunkSize = static_cast<int>(m_chunkSize);
	int lodCount = static_cast<int>(m_lodCount);
	float lodDistance = m_lodDistance;
	std::string material;
	std::string colorMap;

	Helpers::Serializer::DeserializeInt(p_doc, p_node, "chunk_size", chunkSize);
	Helpers::Serializer::DeserializeInt(p_doc, p_node, "lod_count", lodCount);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "lod_distance", lodDistance);
	Helpers::Serializer::DeserializeString(p_doc, p_node, "material", material);
	Helpers::Serializer::DeserializeString(p_doc, p_node, "color_map", colorMap);

	SetChunkSize(static_cast<uint32_t>(std::max(chunkSize, 1)));
	SetLodCount(static_cast<uint32_t>(std::max(lodCount, 1)));
	SetLodDistance(lodDistance);
	SetMaterialPath(material);
	SetColorMap(colorMap);
}

void OvCore::ECS::Components::CTerrain::OnInspector(OvUI::Internal::WidgetContainer& p_root)
{
	using namespace OvCore::Helpers;

	GUIDrawer::DrawScalar<int>(p_root, "Chunk Size",
		[this] { return static_cast<int>(m_chunkSize); },
		[this](int p_value) { SetChunkSize(static_cast<uint32_t>(std::max(p_value, 1))); },
		1.0f, 1, 256
	);

	GUIDrawer::DrawScalar<int>(p_root, "LOD Count",
		[this] { return static_cast<int>(m_lodCount); },
		[this](int p_value) { SetLodCount(static_cast<uint32_t>(std::max(p_value, 1))); },
		1.0f, 1, static_cast<int>(Terrain::kMaxLodCount)
	);

	GUIDrawer::DrawScalar<float>(p_root, "LOD Distance",
		[this] { return m_lodDistance; },
		[this](float p_value) { SetLodDistance(p_value); },
		0.5f, 0.0f, 100000.0f
	);

	GUIDrawer::DrawAsset(p_root, "Material",
		[this] { return m_materialPath; },
		[this](std::string p_path) { SetMaterialPath(p_path); },
		OvTools::Utils::PathParser::EFileType::MATERIAL
	);

	GUIDrawer::DrawAsset(p_root, "Color Map",
		[this] { return m_colorMap; },
		[this](std::string p_path) { SetColorMap(p_path); },
		OvTools::Utils::PathParser::EFileType::TEXTURE
	);

	// As the skinned mesh renderer does: a word, then the details
	auto& status = p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>();
	status.AddPlugin<OvUI::Plugins::DataDispatcher<std::string>>().RegisterGatherer([this, &status]
	{
		const bool ready = !GetError();
		status.color = ready ? OVUI_STYLE(Success) : OVUI_STYLE(Danger);
		return ready ? std::string{ "Ready" } : std::string{ "Error" };
	});

	auto& details = p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>("", OVUI_STYLE(TextDisabled));
	details.AddPlugin<OvUI::Plugins::DataDispatcher<std::string>>().RegisterGatherer([this]
	{
		if (const auto error = GetError())
		{
			return *error;
		}

		return std::format("{} chunks, {} triangles drawn", GetChunkCount(), GetTriangleCount());
	});
}
