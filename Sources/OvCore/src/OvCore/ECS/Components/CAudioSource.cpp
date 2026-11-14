/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <memory>

#include <OvAudio/Core/AudioEngine.h>

#include <OvCore/ECS/Components/CAudioSource.h>
#include <OvCore/ECS/Actor.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/SceneSystem/SceneManager.h>

#include <OvDebug/Logger.h>
#include <OvUI/Widgets/Plots/PlotLines.h>
#include <OvUI/Widgets/Drags/DragFloat.h>
#include <OvUI/Widgets/Selection/ComboBox.h>

OvCore::ECS::Components::CAudioSource::CAudioSource(ECS::Actor& p_owner) :
	AComponent(p_owner),
	m_audioSource(OvCore::Global::ServiceLocator::Get<OvAudio::Core::AudioEngine>(), owner.transform.GetFTransform())
{
}

std::string OvCore::ECS::Components::CAudioSource::GetName()
{
	return "Audio Source";
}

std::string OvCore::ECS::Components::CAudioSource::GetTypeName()
{
	return std::string{ComponentTraits<CAudioSource>::Name};
}

void OvCore::ECS::Components::CAudioSource::SetSound(OvAudio::Resources::Sound* p_sound)
{
	m_sound = p_sound;
}

void OvCore::ECS::Components::CAudioSource::SetAutoplay(bool p_autoplay)
{
	m_autoPlay = p_autoplay;
}

void OvCore::ECS::Components::CAudioSource::SetVolume(float p_volume)
{
	m_audioSource.SetVolume(p_volume);
}

void OvCore::ECS::Components::CAudioSource::SetPan(float p_pan)
{
	m_audioSource.SetPan(p_pan);
}

void OvCore::ECS::Components::CAudioSource::SetLooped(bool p_looped)
{
	m_audioSource.SetLooped(p_looped);
}

void OvCore::ECS::Components::CAudioSource::SetPitch(float p_pitch)
{
	m_audioSource.SetPitch(p_pitch);
}

void OvCore::ECS::Components::CAudioSource::SetSpatial(bool p_value)
{
	m_audioSource.SetSpatial(p_value);
}

void OvCore::ECS::Components::CAudioSource::SetAttenuationThreshold(float p_distance)
{
	m_audioSource.SetAttenuationThreshold(p_distance);
}

void OvCore::ECS::Components::CAudioSource::SetAttenuationModel(OvAudio::Settings::EAttenuationModel p_model)
{
	m_audioSource.SetAttenuationModel(p_model);
}

void OvCore::ECS::Components::CAudioSource::SetRolloff(float p_rolloff)
{
	m_audioSource.SetRolloff(p_rolloff);
}

void OvCore::ECS::Components::CAudioSource::SetMaxDistance(float p_distance)
{
	m_audioSource.SetMaxDistance(p_distance);
}

void OvCore::ECS::Components::CAudioSource::SetBus(const std::string& p_bus)
{
	m_audioSource.SetBus(p_bus);
}

void OvCore::ECS::Components::CAudioSource::SetPitchVariation(float p_variation)
{
	m_audioSource.SetPitchVariation(p_variation);
}

OvAudio::Settings::EAttenuationModel OvCore::ECS::Components::CAudioSource::GetAttenuationModel() const
{
	return m_audioSource.GetAttenuationModel();
}

float OvCore::ECS::Components::CAudioSource::GetRolloff() const
{
	return m_audioSource.GetRolloff();
}

float OvCore::ECS::Components::CAudioSource::GetMaxDistance() const
{
	return m_audioSource.GetMaxDistance();
}

const std::string& OvCore::ECS::Components::CAudioSource::GetBus() const
{
	return m_audioSource.GetBus();
}

float OvCore::ECS::Components::CAudioSource::GetPitchVariation() const
{
	return m_audioSource.GetPitchVariation();
}

void OvCore::ECS::Components::CAudioSource::PlayOneShot(OvAudio::Resources::Sound& p_sound, float p_volumeScale)
{
	if (owner.IsActive())
		m_audioSource.PlayOneShot(p_sound, p_volumeScale);
}

OvAudio::Resources::Sound* OvCore::ECS::Components::CAudioSource::GetSound() const
{
	return m_sound;
}

bool OvCore::ECS::Components::CAudioSource::IsAutoplayed() const
{
	return m_autoPlay;
}

float OvCore::ECS::Components::CAudioSource::GetVolume() const
{
	return m_audioSource.GetVolume();
}

float OvCore::ECS::Components::CAudioSource::GetPan() const
{
	return m_audioSource.GetPan();
}

bool OvCore::ECS::Components::CAudioSource::IsLooped() const
{
	return m_audioSource.IsLooped();
}

float OvCore::ECS::Components::CAudioSource::GetPitch() const
{
	return m_audioSource.GetPitch();
}

bool OvCore::ECS::Components::CAudioSource::IsPlaying() const
{
	return m_audioSource.IsPlaying();
}

bool OvCore::ECS::Components::CAudioSource::IsSpatial() const
{
	return m_audioSource.IsSpatial();
}

float OvCore::ECS::Components::CAudioSource::GetAttenuationThreshold() const
{
	return m_audioSource.GetAttenuationThreshold();
}

void OvCore::ECS::Components::CAudioSource::Play()
{
	if (owner.IsActive() && m_sound)
		m_audioSource.Play(*m_sound);
}

void OvCore::ECS::Components::CAudioSource::Pause()
{
	if (owner.IsActive())
		m_audioSource.Pause();
}

void OvCore::ECS::Components::CAudioSource::Resume()
{
	if (owner.IsActive())
		m_audioSource.Resume();
}

void OvCore::ECS::Components::CAudioSource::Stop()
{
	if (owner.IsActive())
		m_audioSource.Stop();
}

void OvCore::ECS::Components::CAudioSource::OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	using namespace OvCore::Helpers;

	Serializer::SerializeBoolean(p_doc, p_node, "autoplay", m_autoPlay);
	Serializer::SerializeBoolean(p_doc, p_node, "spatial", IsSpatial());
	Serializer::SerializeFloat(p_doc, p_node, "volume", GetVolume());
	Serializer::SerializeFloat(p_doc, p_node, "pan", GetPan());
	Serializer::SerializeBoolean(p_doc, p_node, "looped", IsLooped());
	Serializer::SerializeFloat(p_doc, p_node, "pitch", GetPitch());
	Serializer::SerializeFloat(p_doc, p_node, "attenuation_threshold", GetAttenuationThreshold());
	Serializer::SerializeSound(p_doc, p_node, "audio_clip", m_sound);
	Serializer::SerializeString(p_doc, p_node, "bus", GetBus());
	Serializer::SerializeFloat(p_doc, p_node, "pitch_variation", GetPitchVariation());
	Serializer::SerializeInt(p_doc, p_node, "attenuation_model", static_cast<int>(GetAttenuationModel()));
	Serializer::SerializeFloat(p_doc, p_node, "rolloff", GetRolloff());
	Serializer::SerializeFloat(p_doc, p_node, "max_distance", GetMaxDistance());
}

void OvCore::ECS::Components::CAudioSource::OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	using namespace OvCore::Helpers;

	Serializer::DeserializeBoolean(p_doc, p_node, "autoplay", m_autoPlay);
	SetSpatial(Serializer::DeserializeBoolean(p_doc, p_node, "spatial"));
	SetVolume(Serializer::DeserializeFloat(p_doc, p_node, "volume"));
	SetPan(Serializer::DeserializeFloat(p_doc, p_node, "pan"));
	SetLooped(Serializer::DeserializeBoolean(p_doc, p_node, "looped"));
	SetPitch(Serializer::DeserializeFloat(p_doc, p_node, "pitch"));
	SetAttenuationThreshold(Serializer::DeserializeFloat(p_doc, p_node, "attenuation_threshold"));
	Serializer::DeserializeSound(p_doc, p_node, "audio_clip", m_sound);

	// Keys a file made before them lacks keep their defaults: no bus, no variation, exponential fading
	std::string bus = GetBus();
	Serializer::DeserializeString(p_doc, p_node, "bus", bus);
	SetBus(bus);

	float pitchVariation = GetPitchVariation();
	Serializer::DeserializeFloat(p_doc, p_node, "pitch_variation", pitchVariation);
	SetPitchVariation(pitchVariation);

	int model = static_cast<int>(GetAttenuationModel());
	Serializer::DeserializeInt(p_doc, p_node, "attenuation_model", model);
	if (model >= 0 && model <= static_cast<int>(OvAudio::Settings::EAttenuationModel::EXPONENTIAL_DISTANCE))
	{
		SetAttenuationModel(static_cast<OvAudio::Settings::EAttenuationModel>(model));
	}
	else
	{
		OVLOG_WARNING("Audio Source: no attenuation model " + std::to_string(model) + ", exponential is used");
	}

	float rolloff = GetRolloff();
	Serializer::DeserializeFloat(p_doc, p_node, "rolloff", rolloff);
	SetRolloff(rolloff);

	float maxDistance = GetMaxDistance();
	Serializer::DeserializeFloat(p_doc, p_node, "max_distance", maxDistance);
	SetMaxDistance(maxDistance);
}

void OvCore::ECS::Components::CAudioSource::OnInspector(OvUI::Internal::WidgetContainer& p_root)
{
	using namespace OvAudio::Entities;
	using namespace OvAudio::Settings;
	using namespace OvCore::Helpers;

	auto& audioEngine = OvCore::Global::ServiceLocator::Get<OvAudio::Core::AudioEngine>();

	GUIDrawer::DrawSound(p_root, "Sound", m_sound);
	GUIDrawer::DrawBoolean(p_root, "Auto-play", m_autoPlay);

	// The project's buses, the Master first, and a bus the project no longer has, kept as it is
	GUIDrawer::CreateTitle(p_root, "Bus");
	auto& bus = p_root.CreateWidget<OvUI::Widgets::Selection::ComboBox>(0);
	auto busNames = std::make_shared<std::vector<std::string>>(1, std::string());
	bus.choices.emplace(0, std::string(OvAudio::Core::AudioEngine::kMasterBus));
	for (const auto& name : audioEngine.GetBusNames())
	{
		bus.choices.emplace(static_cast<int>(busNames->size()), name);
		busNames->push_back(name);
	}
	if (!GetBus().empty() && std::ranges::find(*busNames, GetBus()) == busNames->end())
	{
		bus.choices.emplace(static_cast<int>(busNames->size()), GetBus() + " (not in the project)");
		busNames->push_back(GetBus());
	}
	auto& busDispatcher = bus.AddPlugin<OvUI::Plugins::DataDispatcher<int>>();
	busDispatcher.RegisterGatherer([this, busNames]
	{
		const auto found = std::ranges::find(*busNames, GetBus());
		return found == busNames->end() ? 0 : static_cast<int>(found - busNames->begin());
	});
	busDispatcher.RegisterProvider([this, busNames](int p_choice)
	{
		if (p_choice >= 0 && static_cast<size_t>(p_choice) < busNames->size())
		{
			SetBus((*busNames)[static_cast<size_t>(p_choice)]);
		}
	});

	GUIDrawer::DrawScalar<float>(p_root, "Volume", std::bind(&CAudioSource::GetVolume, this), std::bind(&CAudioSource::SetVolume, this, std::placeholders::_1), 0.01f, 0.0f, 1.0f);
	GUIDrawer::DrawScalar<float>(p_root, "Pan", std::bind(&CAudioSource::GetPan, this), std::bind(&CAudioSource::SetPan, this, std::placeholders::_1), 0.01f, -1.0f, 1.0f);
	GUIDrawer::DrawBoolean(p_root, "Looped", std::bind(&CAudioSource::IsLooped, this), std::bind(&CAudioSource::SetLooped, this, std::placeholders::_1));
	GUIDrawer::DrawScalar<float>(p_root, "Pitch", std::bind(&CAudioSource::GetPitch, this), std::bind(&CAudioSource::SetPitch, this, std::placeholders::_1), 0.01f, 0.01f, 10000.0f);
	GUIDrawer::DrawScalar<float>(p_root, "Pitch variation", std::bind(&CAudioSource::GetPitchVariation, this), std::bind(&CAudioSource::SetPitchVariation, this, std::placeholders::_1), 0.01f, 0.0f, 10.0f);
	GUIDrawer::DrawBoolean(p_root, "Spatial", std::bind(&CAudioSource::IsSpatial, this), std::bind(&CAudioSource::SetSpatial, this, std::placeholders::_1));

	GUIDrawer::CreateTitle(p_root, "Attenuation model");
	auto& model = p_root.CreateWidget<OvUI::Widgets::Selection::ComboBox>(0);
	model.choices.emplace(static_cast<int>(EAttenuationModel::NONE), "None");
	model.choices.emplace(static_cast<int>(EAttenuationModel::INVERSE_DISTANCE), "Inverse distance");
	model.choices.emplace(static_cast<int>(EAttenuationModel::LINEAR_DISTANCE), "Linear distance");
	model.choices.emplace(static_cast<int>(EAttenuationModel::EXPONENTIAL_DISTANCE), "Exponential distance");
	auto& modelDispatcher = model.AddPlugin<OvUI::Plugins::DataDispatcher<int>>();
	modelDispatcher.RegisterGatherer([this] { return static_cast<int>(GetAttenuationModel()); });
	modelDispatcher.RegisterProvider([this](int p_choice) { SetAttenuationModel(static_cast<EAttenuationModel>(p_choice)); });

	GUIDrawer::DrawScalar<float>(p_root, "Attenuation threshold", std::bind(&CAudioSource::GetAttenuationThreshold, this), std::bind(&CAudioSource::SetAttenuationThreshold, this, std::placeholders::_1), 0.5f, AttenuationSettings::kSmallestDistance, 100000.0f);
	GUIDrawer::DrawScalar<float>(p_root, "Max distance", std::bind(&CAudioSource::GetMaxDistance, this), std::bind(&CAudioSource::SetMaxDistance, this, std::placeholders::_1), 0.5f, AttenuationSettings::kSmallestDistance, 100000.0f);
	GUIDrawer::DrawScalar<float>(p_root, "Rolloff", std::bind(&CAudioSource::GetRolloff, this), std::bind(&CAudioSource::SetRolloff, this, std::placeholders::_1), 0.01f, 0.0f, 100.0f);

	// The model in use, from the source out to a little past its maximum distance
	GUIDrawer::CreateTitle(p_root, "Spatial graph");
	auto& graph = p_root.CreateWidget<OvUI::Widgets::Plots::PlotLines>(std::vector<float>(), -0.1f, 1.1f);

	graph.size.y = 75.0f;

	auto& graphDispacther = graph.AddPlugin<OvUI::Plugins::DataDispatcher<std::vector<float>>>();
	graphDispacther.RegisterGatherer([this, &graph]()->std::vector<float>
	{
		constexpr int kPoints = 200;

		OvMaths::FVector3 listenerPosition(0.0f, 0.0f, 0.0f);
		bool playMode = OvCore::Global::ServiceLocator::Get<SceneSystem::SceneManager>().GetCurrentScene()->IsPlaying();
		auto mainListener = OvCore::Global::ServiceLocator::Get<OvAudio::Core::AudioEngine>().FindMainListener(!playMode);
		if (mainListener)
		{
			listenerPosition = mainListener->GetTransform().GetWorldPosition();
		}

		const float distanceToListener = OvMaths::FVector3::Distance(listenerPosition, owner.transform.GetWorldPosition());
		const auto attenuation = m_audioSource.GetAttenuation().Sanitized();
		const float range = std::clamp(attenuation.maxDistance * 1.1f, attenuation.minDistance * 4.0f, 100000.0f);
		const float step = range / static_cast<float>(kPoints);

		std::vector<float> result;
		graph.forceHover = -1;
		graph.overlay.clear();

		for (int point = 0; point < kPoints; ++point)
		{
			const float distance = static_cast<float>(point) * step;
			const float volume = attenuation.Evaluate(distance);

			if (std::abs(distance - distanceToListener) <= step * 0.5f)
			{
				graph.forceHover = point;
				graph.overlay = std::to_string(static_cast<int>(volume * 100.0f)) + "% at " + std::to_string(static_cast<int>(distance)) + " m";
			}

			result.push_back(volume);
		}

		return result;
	});
}

void OvCore::ECS::Components::CAudioSource::OnEnable()
{
	if (m_autoPlay)
		Play();
}

void OvCore::ECS::Components::CAudioSource::OnDisable()
{
	m_audioSource.Stop();
}
