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
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/SceneSystem/SceneManager.h>
#include <OvCore/Terrain/Heightmap.h>
#include <OvDebug/Logger.h>
#include <OvUI/Plugins/DataDispatcher.h>
#include <OvUI/Styling/Style.h>
#include <OvUI/Widgets/Texts/TextColored.h>

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
	if (!ApplyHeights(p_sampleCount, p_spacing, std::move(p_heights)))
	{
		return false;
	}

	m_heightmap.clear();
	m_heightmapError.reset();
	return true;
}

bool OvCore::ECS::Components::CPhysicalHeightField::ApplyHeights(uint32_t p_sampleCount, float p_spacing, std::vector<float> p_heights)
{
	if (!GetPhysicalObjectAs<PhysicalHeightField>().SetHeights(p_sampleCount, p_spacing, std::move(p_heights)))
	{
		return false;
	}

	++m_heightsRevision;
	return true;
}

void OvCore::ECS::Components::CPhysicalHeightField::SetHeightmap(const std::string& p_path)
{
	m_heightmap = p_path;
	m_heightmapError.reset();

	if (!m_heightmap.empty())
	{
		LoadHeightmap();
	}
}

void OvCore::ECS::Components::CPhysicalHeightField::LoadHeightmap()
{
	// Paths of the project or of the engine (":"), read through the scene being loaded or played
	std::filesystem::path file = m_heightmap;
	if (Global::ServiceLocator::Has<SceneSystem::SceneManager>())
	{
		if (const auto* scene = OVSERVICE(SceneSystem::SceneManager).GetCurrentScene())
		{
			file = scene->GetRealAssetPath(m_heightmap);
		}
	}

	Terrain::HeightmapImage image;
	if (auto error = Terrain::LoadHeightmap(file, image))
	{
		m_heightmapError = "'" + m_heightmap + "': " + *error;
		OVLOG_WARNING("Physical Height Field on '" + owner.GetName() + "': " + *m_heightmapError);
		ApplyHeights(0, GetSpacing(), {});
		return;
	}

	for (float& value : image.values)
	{
		value = m_heightOffset + value * m_heightScale;
	}

	m_heightmapError.reset();
	ApplyHeights(image.size, GetSpacing(), std::move(image.values));
}

const std::string& OvCore::ECS::Components::CPhysicalHeightField::GetHeightmap() const
{
	return m_heightmap;
}

const std::optional<std::string>& OvCore::ECS::Components::CPhysicalHeightField::GetHeightmapError() const
{
	return m_heightmapError;
}

void OvCore::ECS::Components::CPhysicalHeightField::SetHeightScale(float p_scale)
{
	m_heightScale = p_scale;

	if (!m_heightmap.empty())
	{
		LoadHeightmap();
	}
}

float OvCore::ECS::Components::CPhysicalHeightField::GetHeightScale() const
{
	return m_heightScale;
}

void OvCore::ECS::Components::CPhysicalHeightField::SetHeightOffset(float p_offset)
{
	m_heightOffset = p_offset;

	if (!m_heightmap.empty())
	{
		LoadHeightmap();
	}
}

float OvCore::ECS::Components::CPhysicalHeightField::GetHeightOffset() const
{
	return m_heightOffset;
}

void OvCore::ECS::Components::CPhysicalHeightField::SetSpacing(float p_spacing)
{
	if (p_spacing > 0.0f && p_spacing != GetSpacing())
	{
		ApplyHeights(GetSampleCount(), p_spacing, GetHeights());
	}
}

uint32_t OvCore::ECS::Components::CPhysicalHeightField::GetSampleCount() const
{
	return GetPhysicalObjectAs<PhysicalHeightField>().GetSampleCount();
}

float OvCore::ECS::Components::CPhysicalHeightField::GetSpacing() const
{
	return GetPhysicalObjectAs<PhysicalHeightField>().GetSpacing();
}

const std::vector<float>& OvCore::ECS::Components::CPhysicalHeightField::GetHeights() const
{
	return GetPhysicalObjectAs<PhysicalHeightField>().GetHeights();
}

uint64_t OvCore::ECS::Components::CPhysicalHeightField::GetHeightsRevision() const
{
	return m_heightsRevision;
}

namespace
{
	// The field's own sample under a point of the world, with the matrix that brings it back into the world
	std::optional<OvCore::Terrain::HeightSample> SampleUnder(const OvCore::ECS::Components::CPhysicalHeightField& p_field, float p_x, float p_z, float& p_localX, float& p_localZ)
	{
		const auto& world = p_field.owner.transform.GetWorldMatrix();
		const auto local = OvMaths::FMatrix4::Inverse(world) * OvMaths::FVector4{ p_x, 0.0f, p_z, 1.0f };
		p_localX = local.x;
		p_localZ = local.z;
		return OvCore::Terrain::SampleHeightField(p_field.GetHeights(), p_field.GetSampleCount(), p_field.GetSpacing(), p_localX, p_localZ);
	}
}

std::optional<float> OvCore::ECS::Components::CPhysicalHeightField::GetHeightAt(float p_x, float p_z) const
{
	float localX = 0.0f;
	float localZ = 0.0f;
	const auto sample = SampleUnder(*this, p_x, p_z, localX, localZ);
	if (!sample)
	{
		return std::nullopt;
	}

	const auto point = owner.transform.GetWorldMatrix() * OvMaths::FVector4{ localX, sample->height, localZ, 1.0f };
	return point.y;
}

std::optional<OvMaths::FVector3> OvCore::ECS::Components::CPhysicalHeightField::GetNormalAt(float p_x, float p_z) const
{
	float localX = 0.0f;
	float localZ = 0.0f;
	const auto sample = SampleUnder(*this, p_x, p_z, localX, localZ);
	if (!sample)
	{
		return std::nullopt;
	}

	// A normal goes through the inverse transpose, so a scaled field keeps its slopes
	const auto normalMatrix = OvMaths::FMatrix4::Transpose(OvMaths::FMatrix4::Inverse(owner.transform.GetWorldMatrix()));
	const auto normal = normalMatrix * OvMaths::FVector4{ -sample->slopeX, 1.0f, -sample->slopeZ, 0.0f };
	return OvMaths::FVector3::Normalize({ normal.x, normal.y, normal.z });
}

void OvCore::ECS::Components::CPhysicalHeightField::OnSerialize(tinyxml2::XMLDocument & p_doc, tinyxml2::XMLNode * p_node)
{
	CPhysicalObject::OnSerialize(p_doc, p_node);

	Helpers::Serializer::SerializeInt(p_doc, p_node, "sample_count", static_cast<int>(GetSampleCount()));
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "spacing", GetSpacing());
	Helpers::Serializer::SerializeString(p_doc, p_node, "heightmap", m_heightmap);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "height_scale", m_heightScale);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "height_offset", m_heightOffset);
}

void OvCore::ECS::Components::CPhysicalHeightField::OnDeserialize(tinyxml2::XMLDocument & p_doc, tinyxml2::XMLNode * p_node)
{
	CPhysicalObject::OnDeserialize(p_doc, p_node);

	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "height_scale", m_heightScale);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "height_offset", m_heightOffset);

	std::string heightmap;
	Helpers::Serializer::DeserializeString(p_doc, p_node, "heightmap", heightmap);

	ApplyHeights(
		static_cast<uint32_t>(std::max(Helpers::Serializer::DeserializeInt(p_doc, p_node, "sample_count"), 0)),
		Helpers::Serializer::DeserializeFloat(p_doc, p_node, "spacing"),
		{}
	);

	SetHeightmap(heightmap);
}

void OvCore::ECS::Components::CPhysicalHeightField::OnInspector(OvUI::Internal::WidgetContainer & p_root)
{
	CPhysicalObject::OnInspector(p_root);

	Helpers::GUIDrawer::DrawAsset(p_root, "Heightmap",
		[this] { return m_heightmap; },
		[this](std::string p_path) { SetHeightmap(p_path); },
		OvTools::Utils::PathParser::EFileType::TEXTURE
	);

	Helpers::GUIDrawer::DrawScalar<float>(p_root, "Height Scale",
		[this] { return m_heightScale; },
		[this](float p_value) { SetHeightScale(p_value); },
		0.1f
	);

	Helpers::GUIDrawer::DrawScalar<float>(p_root, "Height Offset",
		[this] { return m_heightOffset; },
		[this](float p_value) { SetHeightOffset(p_value); },
		0.1f
	);

	Helpers::GUIDrawer::DrawScalar<float>(p_root, "Spacing",
		[this] { return GetSpacing(); },
		[this](float p_value) { SetSpacing(p_value); },
		0.05f, 0.01f, 1000.0f
	);

	Helpers::GUIDrawer::DrawReadOnlyString(p_root, "Sample Count", [this]() { return std::to_string(GetSampleCount()); });

	// Why the field is empty, if it is: a height map that cannot be read, or none at all
	Helpers::GUIDrawer::CreateTitle(p_root, "Heights");
	auto& status = p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>();
	status.AddPlugin<OvUI::Plugins::DataDispatcher<std::string>>().RegisterGatherer([this, &status]
	{
		status.color = m_heightmapError || GetHeights().empty() ? OVUI_STYLE(Warning) : OVUI_STYLE(TextDisabled);

		if (m_heightmapError)
		{
			return *m_heightmapError;
		}

		if (GetHeights().empty())
		{
			return std::string{ "None: set a height map, or heights from a script" };
		}

		return m_heightmap.empty() ? std::string{ "From a script" } : std::string{ "From the height map" };
	});
}
