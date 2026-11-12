/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <format>
#include <sstream>

#include <tinyxml2.h>

#include <baregl/Buffer.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CParticleSystem.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Helpers/GUIDrawer.h>
#include <OvCore/Helpers/Serializer.h>
#include <OvCore/ResourceManagement/MaterialManager.h>
#include <OvCore/Resources/Material.h>
#include <OvRendering/Geometry/Vertex.h>
#include <OvRendering/Resources/Mesh.h>
#include <OvTools/Time/Clock.h>
#include <OvUI/Plugins/DataDispatcher.h>
#include <OvUI/Widgets/Selection/ComboBox.h>

namespace
{
	using OvCore::Particles::ParticleBurst;
	using OvCore::Particles::ParticleSettings;

	constexpr auto kQuadVertices = std::to_array<OvRendering::Geometry::Vertex>({
		{ {-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} },
		{ { 0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} },
		{ { 0.5f,  0.5f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} },
		{ {-0.5f,  0.5f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} }
	});

	constexpr auto kQuadIndices = std::to_array<uint32_t>({ 0, 1, 2, 0, 2, 3 });

	// "time:count, time:count", as the inspector shows bursts
	std::string FormatBursts(const std::vector<ParticleBurst>& p_bursts)
	{
		std::string text;
		for (const auto& burst : p_bursts)
		{
			text += std::format("{}{:g}:{}", text.empty() ? "" : ", ", burst.time, burst.count);
		}
		return text;
	}

	std::vector<ParticleBurst> ParseBursts(const std::string& p_text)
	{
		std::vector<ParticleBurst> bursts;
		std::stringstream stream(p_text);

		for (std::string item; std::getline(stream, item, ','); )
		{
			ParticleBurst burst;
			char separator = 0;
			std::stringstream entry(item);

			if (entry >> burst.time >> separator >> burst.count && separator == ':')
			{
				bursts.push_back(burst);
			}
		}

		return bursts;
	}
}

OvCore::ECS::Components::CParticleSystem::CParticleSystem(ECS::Actor& p_owner) : AComponent(p_owner)
{
}

OvCore::ECS::Components::CParticleSystem::~CParticleSystem() = default;

std::string OvCore::ECS::Components::CParticleSystem::GetName()
{
	return "Particle System";
}

std::string OvCore::ECS::Components::CParticleSystem::GetTypeName()
{
	return std::string{ ComponentTraits<CParticleSystem>::Name };
}

void OvCore::ECS::Components::CParticleSystem::Play()
{
	m_emitter.Play();
}

void OvCore::ECS::Components::CParticleSystem::Stop()
{
	m_emitter.Stop();
}

void OvCore::ECS::Components::CParticleSystem::Clear()
{
	m_emitter.Clear();
}

bool OvCore::ECS::Components::CParticleSystem::IsPlaying() const
{
	return m_emitter.IsPlaying();
}

uint32_t OvCore::ECS::Components::CParticleSystem::Emit(uint32_t p_count, const Particles::ParticleOverrides& p_overrides)
{
	return m_emitter.Emit(p_count, owner.transform.GetWorldMatrix(), p_overrides);
}

uint32_t OvCore::ECS::Components::CParticleSystem::GetAliveCount() const
{
	return m_emitter.GetAliveCount();
}

void OvCore::ECS::Components::CParticleSystem::SetSettings(const Particles::ParticleSettings& p_settings)
{
	m_emitter.SetSettings(p_settings);
}

const OvCore::Particles::ParticleSettings& OvCore::ECS::Components::CParticleSystem::GetSettings() const
{
	return m_emitter.GetSettings();
}

void OvCore::ECS::Components::CParticleSystem::SetModel(OvRendering::Resources::Model* p_model)
{
	m_model = p_model;
}

OvRendering::Resources::Model* OvCore::ECS::Components::CParticleSystem::GetModel() const
{
	return m_model;
}

void OvCore::ECS::Components::CParticleSystem::SetMaterial(OvCore::Resources::Material* p_material)
{
	m_material = p_material;
}

OvCore::Resources::Material* OvCore::ECS::Components::CParticleSystem::GetMaterial() const
{
	return m_material;
}

const OvCore::Particles::ParticleEmitter& OvCore::ECS::Components::CParticleSystem::GetEmitter() const
{
	return m_emitter;
}

void OvCore::ECS::Components::CParticleSystem::Simulate(float p_deltaTime)
{
	float step = p_deltaTime;

	if (GetSettings().unscaled && Global::ServiceLocator::Has<OvTools::Time::Clock>())
	{
		step = Global::ServiceLocator::Get<OvTools::Time::Clock>().GetDeltaTimeUnscaled();
	}

	m_emitter.Advance(step, owner.transform.GetWorldMatrix());
}

std::optional<OvRendering::Geometry::BoundingSphere> OvCore::ECS::Components::CParticleSystem::GetWorldBounds() const
{
	auto bounds = m_emitter.GetBounds();

	if (bounds && GetSettings().space == Particles::EParticleSpace::LOCAL)
	{
		// The particles follow the emitter: their sphere goes where it goes, as large as its largest scale
		const auto& m = owner.transform.GetWorldMatrix().data;
		const auto& p = bounds->position;
		const float scale = std::max({
			std::sqrt(m[0] * m[0] + m[4] * m[4] + m[8] * m[8]),
			std::sqrt(m[1] * m[1] + m[5] * m[5] + m[9] * m[9]),
			std::sqrt(m[2] * m[2] + m[6] * m[6] + m[10] * m[10])
		});

		bounds = OvRendering::Geometry::BoundingSphere{
			{
				m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3],
				m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7],
				m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11]
			},
			bounds->radius * scale
		};
	}

	return bounds;
}

OvCore::Resources::Material* OvCore::ECS::Components::CParticleSystem::ResolveMaterial() const
{
	if (m_material)
	{
		return m_material;
	}

	return Global::ServiceLocator::Get<ResourceManagement::MaterialManager>().GetResource(kDefaultMaterialPath);
}

OvRendering::Resources::Mesh& OvCore::ECS::Components::CParticleSystem::GetQuad()
{
	if (!m_quad)
	{
		m_quad = std::make_unique<OvRendering::Resources::Mesh>(kQuadVertices, kQuadIndices);
	}

	return *m_quad;
}

void OvCore::ECS::Components::CParticleSystem::BindBuffer(uint32_t p_bindingPoint)
{
	using namespace OvCore::Particles;

	const auto records = m_emitter.GetRecords();
	const auto capacity = static_cast<uint32_t>(records.size());
	constexpr uint64_t headerSize = sizeof(ParticleBufferHeader);

	if (!m_buffer || m_bufferCapacity != capacity)
	{
		m_buffer = std::make_unique<baregl::Buffer>();
		m_buffer->Allocate(headerSize + static_cast<uint64_t>(capacity) * sizeof(ParticleRecord), baregl::types::EAccessSpecifier::DYNAMIC_DRAW);
		m_bufferCapacity = capacity;
		m_uploadedHeader.reset();

		// A new buffer holds nothing yet: every slot goes up
		m_emitter.TakeChangedSlots();
		m_buffer->Upload(records.data(), baregl::data::BufferMemoryRange{ headerSize, static_cast<uint64_t>(capacity) * sizeof(ParticleRecord) });
	}
	else
	{
		for (const auto& range : m_emitter.TakeChangedSlots())
		{
			m_buffer->Upload(
				records.data() + range.first,
				baregl::data::BufferMemoryRange{
					headerSize + static_cast<uint64_t>(range.first) * sizeof(ParticleRecord),
					static_cast<uint64_t>(range.count) * sizeof(ParticleRecord)
				}
			);
		}
	}

	const auto header = m_emitter.BuildHeader(owner.transform.GetWorldMatrix());

	if (!m_uploadedHeader || std::memcmp(&*m_uploadedHeader, &header, sizeof(header)) != 0)
	{
		m_buffer->Upload(&header, baregl::data::BufferMemoryRange{ 0, headerSize });
		m_uploadedHeader = header;
	}

	m_buffer->Bind(baregl::types::EBufferType::SHADER_STORAGE, p_bindingPoint);
}

void OvCore::ECS::Components::CParticleSystem::OnStart()
{
	if (GetSettings().playOnStart)
	{
		Play();
	}
}

void OvCore::ECS::Components::CParticleSystem::OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	using namespace OvCore::Helpers;
	const auto& s = GetSettings();

	Serializer::SerializeModel(p_doc, p_node, "model", m_model);
	Serializer::SerializeMaterial(p_doc, p_node, "material", m_material);
	Serializer::SerializeUint32(p_doc, p_node, "max_particles", s.maxParticles);
	Serializer::SerializeFloat(p_doc, p_node, "rate", s.rate);
	Serializer::SerializeFloat(p_doc, p_node, "duration", s.duration);
	Serializer::SerializeBoolean(p_doc, p_node, "looping", s.looping);
	Serializer::SerializeBoolean(p_doc, p_node, "play_on_start", s.playOnStart);
	Serializer::SerializeFloat(p_doc, p_node, "lifetime_min", s.lifetimeMin);
	Serializer::SerializeFloat(p_doc, p_node, "lifetime_max", s.lifetimeMax);
	Serializer::SerializeFloat(p_doc, p_node, "speed_min", s.speedMin);
	Serializer::SerializeFloat(p_doc, p_node, "speed_max", s.speedMax);
	Serializer::SerializeInt(p_doc, p_node, "shape", static_cast<int>(s.shape));
	Serializer::SerializeFloat(p_doc, p_node, "shape_radius", s.shapeRadius);
	Serializer::SerializeFloat(p_doc, p_node, "shape_angle", s.shapeAngle);
	Serializer::SerializeVec3(p_doc, p_node, "shape_size", s.shapeSize);
	Serializer::SerializeVec3(p_doc, p_node, "gravity", s.gravity);
	Serializer::SerializeFloat(p_doc, p_node, "drag", s.drag);
	Serializer::SerializeFloat(p_doc, p_node, "size_start", s.sizeStart);
	Serializer::SerializeFloat(p_doc, p_node, "size_end", s.sizeEnd);
	Serializer::SerializeVec4(p_doc, p_node, "color_start", s.colorStart);
	Serializer::SerializeVec4(p_doc, p_node, "color_end", s.colorEnd);
	Serializer::SerializeFloat(p_doc, p_node, "fade_in", s.fadeIn);
	Serializer::SerializeFloat(p_doc, p_node, "fade_out", s.fadeOut);
	Serializer::SerializeFloat(p_doc, p_node, "spin_min", s.spinMin);
	Serializer::SerializeFloat(p_doc, p_node, "spin_max", s.spinMax);
	Serializer::SerializeBoolean(p_doc, p_node, "random_rotation", s.randomRotation);
	Serializer::SerializeInt(p_doc, p_node, "space", static_cast<int>(s.space));
	Serializer::SerializeBoolean(p_doc, p_node, "unscaled", s.unscaled);
	Serializer::SerializeUint32(p_doc, p_node, "seed", s.seed);

	auto* bursts = p_doc.NewElement("bursts");
	p_node->InsertEndChild(bursts);

	for (const auto& burst : s.bursts)
	{
		auto* element = p_doc.NewElement("burst");
		bursts->InsertEndChild(element);
		Serializer::SerializeFloat(p_doc, element, "time", burst.time);
		Serializer::SerializeUint32(p_doc, element, "count", burst.count);
	}
}

void OvCore::ECS::Components::CParticleSystem::OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	using namespace OvCore::Helpers;
	auto s = GetSettings();

	m_model = Serializer::DeserializeModel(p_doc, p_node, "model");
	m_material = Serializer::DeserializeMaterial(p_doc, p_node, "material");

	int shape = static_cast<int>(s.shape);
	int space = static_cast<int>(s.space);

	Serializer::DeserializeUint32(p_doc, p_node, "max_particles", s.maxParticles);
	Serializer::DeserializeFloat(p_doc, p_node, "rate", s.rate);
	Serializer::DeserializeFloat(p_doc, p_node, "duration", s.duration);
	Serializer::DeserializeBoolean(p_doc, p_node, "looping", s.looping);
	Serializer::DeserializeBoolean(p_doc, p_node, "play_on_start", s.playOnStart);
	Serializer::DeserializeFloat(p_doc, p_node, "lifetime_min", s.lifetimeMin);
	Serializer::DeserializeFloat(p_doc, p_node, "lifetime_max", s.lifetimeMax);
	Serializer::DeserializeFloat(p_doc, p_node, "speed_min", s.speedMin);
	Serializer::DeserializeFloat(p_doc, p_node, "speed_max", s.speedMax);
	Serializer::DeserializeInt(p_doc, p_node, "shape", shape);
	Serializer::DeserializeFloat(p_doc, p_node, "shape_radius", s.shapeRadius);
	Serializer::DeserializeFloat(p_doc, p_node, "shape_angle", s.shapeAngle);
	Serializer::DeserializeVec3(p_doc, p_node, "shape_size", s.shapeSize);
	Serializer::DeserializeVec3(p_doc, p_node, "gravity", s.gravity);
	Serializer::DeserializeFloat(p_doc, p_node, "drag", s.drag);
	Serializer::DeserializeFloat(p_doc, p_node, "size_start", s.sizeStart);
	Serializer::DeserializeFloat(p_doc, p_node, "size_end", s.sizeEnd);
	Serializer::DeserializeVec4(p_doc, p_node, "color_start", s.colorStart);
	Serializer::DeserializeVec4(p_doc, p_node, "color_end", s.colorEnd);
	Serializer::DeserializeFloat(p_doc, p_node, "fade_in", s.fadeIn);
	Serializer::DeserializeFloat(p_doc, p_node, "fade_out", s.fadeOut);
	Serializer::DeserializeFloat(p_doc, p_node, "spin_min", s.spinMin);
	Serializer::DeserializeFloat(p_doc, p_node, "spin_max", s.spinMax);
	Serializer::DeserializeBoolean(p_doc, p_node, "random_rotation", s.randomRotation);
	Serializer::DeserializeInt(p_doc, p_node, "space", space);
	Serializer::DeserializeBoolean(p_doc, p_node, "unscaled", s.unscaled);
	Serializer::DeserializeUint32(p_doc, p_node, "seed", s.seed);

	// A value no shape or space has is read as the default
	s.shape = shape >= 0 && shape <= static_cast<int>(Particles::EParticleShape::BOX) ? static_cast<Particles::EParticleShape>(shape) : Particles::ParticleSettings{}.shape;
	s.space = space >= 0 && space <= static_cast<int>(Particles::EParticleSpace::LOCAL) ? static_cast<Particles::EParticleSpace>(space) : Particles::ParticleSettings{}.space;

	if (auto* bursts = p_node->FirstChildElement("bursts"))
	{
		s.bursts.clear();

		for (auto* element = bursts->FirstChildElement("burst"); element; element = element->NextSiblingElement("burst"))
		{
			ParticleBurst burst;
			Serializer::DeserializeFloat(p_doc, element, "time", burst.time);
			Serializer::DeserializeUint32(p_doc, element, "count", burst.count);
			s.bursts.push_back(burst);
		}
	}

	SetSettings(s);
}

void OvCore::ECS::Components::CParticleSystem::OnInspector(OvUI::Internal::WidgetContainer& p_root)
{
	using namespace OvCore::Helpers;
	using Settings = Particles::ParticleSettings;

	// Each field reads the settings, and changes a copy of them
	const auto scalar = [&]<typename T>(const std::string& p_name, T Settings::* p_member, float p_step, T p_min, T p_max)
	{
		GUIDrawer::DrawScalar<T>(p_root, p_name,
			[this, p_member]() { return GetSettings().*p_member; },
			[this, p_member](T p_value) { auto settings = GetSettings(); settings.*p_member = p_value; SetSettings(settings); },
			p_step, p_min, p_max
		);
	};

	const auto boolean = [&](const std::string& p_name, bool Settings::* p_member)
	{
		GUIDrawer::DrawBoolean(p_root, p_name,
			[this, p_member]() { return GetSettings().*p_member; },
			[this, p_member](bool p_value) { auto settings = GetSettings(); settings.*p_member = p_value; SetSettings(settings); }
		);
	};

	const auto vector = [&](const std::string& p_name, OvMaths::FVector3 Settings::* p_member, float p_step)
	{
		GUIDrawer::DrawVec3(p_root, p_name,
			[this, p_member]() { return GetSettings().*p_member; },
			[this, p_member](OvMaths::FVector3 p_value) { auto settings = GetSettings(); settings.*p_member = p_value; SetSettings(settings); },
			p_step
		);
	};

	const auto color = [&](const std::string& p_name, OvMaths::FVector4 Settings::* p_member)
	{
		GUIDrawer::DrawColor(p_root, p_name,
			[this, p_member]() { return OvUI::Types::Color(GetSettings().*p_member); },
			[this, p_member](OvUI::Types::Color p_value) { auto settings = GetSettings(); settings.*p_member = { p_value.r, p_value.g, p_value.b, p_value.a }; SetSettings(settings); },
			true
		);
	};

	const auto combo = [&](const std::string& p_name, std::vector<std::string> p_choices, std::function<int()> p_gatherer, std::function<void(int)> p_provider)
	{
		GUIDrawer::CreateTitle(p_root, p_name);
		auto& widget = p_root.CreateWidget<OvUI::Widgets::Selection::ComboBox>(p_gatherer());
		for (int i = 0; i < static_cast<int>(p_choices.size()); ++i)
		{
			widget.choices.emplace(i, p_choices[i]);
		}
		auto& dispatcher = widget.AddPlugin<OvUI::Plugins::DataDispatcher<int>>();
		dispatcher.RegisterGatherer(p_gatherer);
		dispatcher.RegisterProvider(p_provider);
	};

	constexpr float kMax = std::numeric_limits<float>::max();

	GUIDrawer::DrawMesh(p_root, "Model", m_model, nullptr);
	GUIDrawer::DrawMaterial(p_root, "Material", m_material, nullptr);

	scalar.operator()<uint32_t>("Max Particles", &Settings::maxParticles, 1.0f, 1, Settings::kMaxParticles);
	scalar.operator()<float>("Rate", &Settings::rate, 0.5f, 0.0f, kMax);
	GUIDrawer::DrawString(p_root, "Bursts (time:count, ...)",
		[this]() { return FormatBursts(GetSettings().bursts); },
		[this](std::string p_value) { auto settings = GetSettings(); settings.bursts = ParseBursts(p_value); SetSettings(settings); }
	);
	scalar.operator()<float>("Duration", &Settings::duration, 0.05f, 0.01f, kMax);
	boolean("Looping", &Settings::looping);
	boolean("Play On Start", &Settings::playOnStart);

	scalar.operator()<float>("Lifetime Min", &Settings::lifetimeMin, 0.05f, 0.001f, kMax);
	scalar.operator()<float>("Lifetime Max", &Settings::lifetimeMax, 0.05f, 0.001f, kMax);
	scalar.operator()<float>("Speed Min", &Settings::speedMin, 0.05f, -kMax, kMax);
	scalar.operator()<float>("Speed Max", &Settings::speedMax, 0.05f, -kMax, kMax);

	combo("Shape", { "Point", "Sphere", "Hemisphere", "Cone", "Box" },
		[this]() { return static_cast<int>(GetSettings().shape); },
		[this](int p_choice) { auto settings = GetSettings(); settings.shape = static_cast<Particles::EParticleShape>(p_choice); SetSettings(settings); }
	);
	scalar.operator()<float>("Shape Radius", &Settings::shapeRadius, 0.01f, 0.0f, kMax);
	scalar.operator()<float>("Shape Angle", &Settings::shapeAngle, 0.5f, 0.0f, 180.0f);
	vector("Shape Size", &Settings::shapeSize, 0.05f);

	vector("Gravity", &Settings::gravity, 0.1f);
	scalar.operator()<float>("Drag", &Settings::drag, 0.05f, 0.0f, kMax);

	scalar.operator()<float>("Size Start", &Settings::sizeStart, 0.01f, 0.0f, kMax);
	scalar.operator()<float>("Size End", &Settings::sizeEnd, 0.01f, 0.0f, kMax);
	color("Color Start", &Settings::colorStart);
	color("Color End", &Settings::colorEnd);
	scalar.operator()<float>("Fade In", &Settings::fadeIn, 0.01f, 0.0f, 1.0f);
	scalar.operator()<float>("Fade Out", &Settings::fadeOut, 0.01f, 0.0f, 1.0f);

	scalar.operator()<float>("Spin Min", &Settings::spinMin, 1.0f, -kMax, kMax);
	scalar.operator()<float>("Spin Max", &Settings::spinMax, 1.0f, -kMax, kMax);
	boolean("Random Rotation", &Settings::randomRotation);

	combo("Space", { "World", "Local" },
		[this]() { return static_cast<int>(GetSettings().space); },
		[this](int p_choice) { auto settings = GetSettings(); settings.space = static_cast<Particles::EParticleSpace>(p_choice); SetSettings(settings); }
	);
	boolean("Unscaled Time", &Settings::unscaled);
	scalar.operator()<uint32_t>("Seed", &Settings::seed, 1.0f, 0, std::numeric_limits<uint32_t>::max());
}
