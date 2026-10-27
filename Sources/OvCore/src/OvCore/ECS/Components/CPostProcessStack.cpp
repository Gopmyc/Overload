/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <OvCore/ECS/Components/CPostProcessStack.h>
#include <OvUI/Widgets/Visual/Separator.h>
#include <OvUI/Widgets/Layout/Spacing.h>
#include <OvUI/Widgets/Selection/ComboBox.h>

OvCore::ECS::Components::CPostProcessStack::CPostProcessStack(ECS::Actor& p_owner) : AComponent(p_owner)
{
}

std::string OvCore::ECS::Components::CPostProcessStack::GetName()
{
	return "Post Process Stack";
}

std::string OvCore::ECS::Components::CPostProcessStack::GetTypeName()
{
	return std::string{ComponentTraits<CPostProcessStack>::Name};
}

void OvCore::ECS::Components::CPostProcessStack::OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	auto& bloomSettings = m_settings.Get<Rendering::PostProcess::BloomEffect, Rendering::PostProcess::BloomSettings>();
	auto& autoExposureSettings = m_settings.Get<Rendering::PostProcess::AutoExposureEffect, Rendering::PostProcess::AutoExposureSettings>();
	auto& fxaaSettings = m_settings.Get<Rendering::PostProcess::FXAAEffect, Rendering::PostProcess::FXAASettings>();
	auto& tonemappingSettings = m_settings.Get<Rendering::PostProcess::TonemappingEffect, Rendering::PostProcess::TonemappingSettings>();

	Helpers::Serializer::SerializeBoolean(p_doc, p_node, "bloom_enabled", bloomSettings.enabled);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "bloom_intensity", bloomSettings.intensity);
	Helpers::Serializer::SerializeInt(p_doc, p_node, "bloom_passes", bloomSettings.passes);

	Helpers::Serializer::SerializeBoolean(p_doc, p_node, "auto_exposure_enabled", autoExposureSettings.enabled);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "auto_exposure_center_weight_bias", autoExposureSettings.centerWeightBias);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "auto_exposure_luminance_min", autoExposureSettings.minLuminanceEV);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "auto_exposure_luminance_max", autoExposureSettings.maxLuminanceEV);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "auto_exposure_exposure_compensation", autoExposureSettings.exposureCompensationEV);
	Helpers::Serializer::SerializeBoolean(p_doc, p_node, "auto_exposure_progressive", autoExposureSettings.progressive);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "auto_exposure_speed_up", autoExposureSettings.speedUp);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "auto_exposure_speed_down", autoExposureSettings.speedDown);

	Helpers::Serializer::SerializeBoolean(p_doc, p_node, "tonemapping_enabled", tonemappingSettings.enabled);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "tonemapping_exposure", tonemappingSettings.exposure);
	Helpers::Serializer::SerializeInt(p_doc, p_node, "tonemapping_mode", static_cast<int>(tonemappingSettings.mode));
	Helpers::Serializer::SerializeBoolean(p_doc, p_node, "tonemapping_gamma_correction", tonemappingSettings.gammaCorrection);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "grading_temperature", tonemappingSettings.temperature);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "grading_tint", tonemappingSettings.tint);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "grading_saturation", tonemappingSettings.saturation);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "grading_contrast", tonemappingSettings.contrast);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "vignette_intensity", tonemappingSettings.vignetteIntensity);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "vignette_smoothness", tonemappingSettings.vignetteSmoothness);

	Helpers::Serializer::SerializeBoolean(p_doc, p_node, "fxaa_enabled", fxaaSettings.enabled);

	Helpers::Serializer::SerializeBoolean(p_doc, p_node, "fog_enabled", m_fog.enabled);
	Helpers::Serializer::SerializeVec3(p_doc, p_node, "fog_color", m_fog.color);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "fog_intensity", m_fog.intensity);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "fog_start", m_fog.start);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "fog_density", m_fog.density);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "fog_max_opacity", m_fog.maxOpacity);
}

void OvCore::ECS::Components::CPostProcessStack::OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	auto& bloomSettings = m_settings.Get<Rendering::PostProcess::BloomEffect, Rendering::PostProcess::BloomSettings>();
	auto& autoExposureSettings = m_settings.Get<Rendering::PostProcess::AutoExposureEffect, Rendering::PostProcess::AutoExposureSettings>();
	auto& fxaaSettings = m_settings.Get<Rendering::PostProcess::FXAAEffect, Rendering::PostProcess::FXAASettings>();
	auto& tonemappingSettings = m_settings.Get<Rendering::PostProcess::TonemappingEffect, Rendering::PostProcess::TonemappingSettings>();

	Helpers::Serializer::DeserializeBoolean(p_doc, p_node, "bloom_enabled", bloomSettings.enabled);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "bloom_intensity", bloomSettings.intensity);
	Helpers::Serializer::DeserializeInt(p_doc, p_node, "bloom_passes", bloomSettings.passes);

	Helpers::Serializer::DeserializeBoolean(p_doc, p_node, "auto_exposure_enabled", autoExposureSettings.enabled);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "auto_exposure_center_weight_bias", autoExposureSettings.centerWeightBias);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "auto_exposure_luminance_min", autoExposureSettings.minLuminanceEV);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "auto_exposure_luminance_max", autoExposureSettings.maxLuminanceEV);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "auto_exposure_exposure_compensation", autoExposureSettings.exposureCompensationEV);
	Helpers::Serializer::DeserializeBoolean(p_doc, p_node, "auto_exposure_progressive", autoExposureSettings.progressive);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "auto_exposure_speed_up", autoExposureSettings.speedUp);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "auto_exposure_speed_down", autoExposureSettings.speedDown);

	Helpers::Serializer::DeserializeBoolean(p_doc, p_node, "tonemapping_enabled", tonemappingSettings.enabled);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "tonemapping_exposure", tonemappingSettings.exposure);
	Helpers::Serializer::DeserializeInt(p_doc, p_node, "tonemapping_mode", reinterpret_cast<int&>(tonemappingSettings.mode));
	Helpers::Serializer::DeserializeBoolean(p_doc, p_node, "tonemapping_gamma_correction", tonemappingSettings.gammaCorrection);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "grading_temperature", tonemappingSettings.temperature);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "grading_tint", tonemappingSettings.tint);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "grading_saturation", tonemappingSettings.saturation);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "grading_contrast", tonemappingSettings.contrast);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "vignette_intensity", tonemappingSettings.vignetteIntensity);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "vignette_smoothness", tonemappingSettings.vignetteSmoothness);

	Helpers::Serializer::DeserializeBoolean(p_doc, p_node, "fxaa_enabled", fxaaSettings.enabled);

	Helpers::Serializer::DeserializeBoolean(p_doc, p_node, "fog_enabled", m_fog.enabled);
	Helpers::Serializer::DeserializeVec3(p_doc, p_node, "fog_color", m_fog.color);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "fog_intensity", m_fog.intensity);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "fog_start", m_fog.start);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "fog_density", m_fog.density);
	Helpers::Serializer::DeserializeFloat(p_doc, p_node, "fog_max_opacity", m_fog.maxOpacity);
}

void OvCore::ECS::Components::CPostProcessStack::OnInspector(OvUI::Internal::WidgetContainer& p_root)
{
	auto& bloomSettings = m_settings.Get<Rendering::PostProcess::BloomEffect, Rendering::PostProcess::BloomSettings>();
	auto& autoExposureSettings = m_settings.Get<Rendering::PostProcess::AutoExposureEffect, Rendering::PostProcess::AutoExposureSettings>();
	auto& fxaaSettings = m_settings.Get<Rendering::PostProcess::FXAAEffect, Rendering::PostProcess::FXAASettings>();
	auto& tonemappingSettings = m_settings.Get<Rendering::PostProcess::TonemappingEffect, Rendering::PostProcess::TonemappingSettings>();

	OvCore::Helpers::GUIDrawer::DrawBoolean(p_root, "Auto Exposure Enabled", autoExposureSettings.enabled);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Auto Exposure Center Weight Bias", autoExposureSettings.centerWeightBias, 0.1f, 0.0f, 1.0f);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Auto Exposure Min Luminance (EV)", autoExposureSettings.minLuminanceEV, 0.1f, OvCore::Helpers::GUIDrawer::_MIN_FLOAT, OvCore::Helpers::GUIDrawer::_MAX_FLOAT);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Auto Exposure Max Luminance (EV)", autoExposureSettings.maxLuminanceEV, 0.1f, OvCore::Helpers::GUIDrawer::_MIN_FLOAT, OvCore::Helpers::GUIDrawer::_MAX_FLOAT);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Auto Exposure Exposure Compensation (EV)", autoExposureSettings.exposureCompensationEV, 0.1f, OvCore::Helpers::GUIDrawer::_MIN_FLOAT, OvCore::Helpers::GUIDrawer::_MAX_FLOAT);
	OvCore::Helpers::GUIDrawer::DrawBoolean(p_root, "Auto Exposure Progressive", autoExposureSettings.progressive);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Auto Exposure Speed Up", autoExposureSettings.speedUp);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Auto Exposure Speed Down", autoExposureSettings.speedDown);

	p_root.CreateWidget<OvUI::Widgets::Visual::Separator>();

	OvCore::Helpers::GUIDrawer::DrawBoolean(p_root, "Bloom Enabled", bloomSettings.enabled);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Bloom Intensity", bloomSettings.intensity, 0.1f, 0.0f, Rendering::PostProcess::BloomConstants::kMaxBloomIntensity);
	OvCore::Helpers::GUIDrawer::DrawScalar<int>(p_root, "Bloom Passes", bloomSettings.passes, 1, Rendering::PostProcess::BloomConstants::kMinPassCount, Rendering::PostProcess::BloomConstants::kMaxPassCount);

	p_root.CreateWidget<OvUI::Widgets::Visual::Separator>();

	OvCore::Helpers::GUIDrawer::DrawBoolean(p_root, "Tonemapping Enabled", tonemappingSettings.enabled);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Tonemapping Exposure", tonemappingSettings.exposure, 0.05f, 0.0f);
	Helpers::GUIDrawer::CreateTitle(p_root, "Tonemapping Mode");
	auto& tonemappingMode = p_root.CreateWidget<OvUI::Widgets::Selection::ComboBox>(static_cast<int>(tonemappingSettings.mode));
	tonemappingMode.choices = {
		{ 0, "Neutral" },
		{ 1, "Reinhard" },
		{ 2, "Reinhard-Jodie" },
		{ 3, "Uncharted 2" },
		{ 4, "Uncharted 2 (Filmic)" },
		{ 5, "ACES" }
	};
	auto& tonemappingModeDispatcher = tonemappingMode.AddPlugin<OvUI::Plugins::DataDispatcher<int>>();
	tonemappingModeDispatcher.RegisterGatherer([this] {
		return static_cast<int>(m_settings.Get<Rendering::PostProcess::TonemappingEffect, Rendering::PostProcess::TonemappingSettings>().mode);
	});
	tonemappingModeDispatcher.RegisterProvider([this](int p_choice) {
		auto& tonemappingSettings = m_settings.Get<Rendering::PostProcess::TonemappingEffect, Rendering::PostProcess::TonemappingSettings>();
		tonemappingSettings.mode = static_cast<OvCore::Rendering::PostProcess::ETonemappingMode>(p_choice);
	});
	OvCore::Helpers::GUIDrawer::DrawBoolean(p_root, "Tonemapping Gamma Correction", tonemappingSettings.gammaCorrection);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Grading Temperature", tonemappingSettings.temperature, 0.01f, -1.0f, 1.0f);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Grading Tint", tonemappingSettings.tint, 0.01f, -1.0f, 1.0f);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Grading Saturation", tonemappingSettings.saturation, 0.01f, 0.0f, 2.0f);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Grading Contrast", tonemappingSettings.contrast, 0.01f, 0.5f, 2.0f);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Vignette Intensity", tonemappingSettings.vignetteIntensity, 0.01f, 0.0f, 1.0f);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Vignette Smoothness", tonemappingSettings.vignetteSmoothness, 0.01f, 0.0f, 1.0f);

	p_root.CreateWidget<OvUI::Widgets::Visual::Separator>();

	OvCore::Helpers::GUIDrawer::DrawBoolean(p_root, "FXAA Enabled", fxaaSettings.enabled);

	p_root.CreateWidget<OvUI::Widgets::Visual::Separator>();

	OvCore::Helpers::GUIDrawer::DrawBoolean(p_root, "Fog Enabled", m_fog.enabled);
	OvCore::Helpers::GUIDrawer::DrawColor(p_root, "Fog Color",
		[this] { return OvUI::Types::Color(m_fog.color); },
		[this](OvUI::Types::Color p_color) { m_fog.color = { p_color.r, p_color.g, p_color.b }; }
	);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Fog Intensity", m_fog.intensity, 0.05f, 0.0f, OvCore::Helpers::GUIDrawer::_MAX_FLOAT);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Fog Start", m_fog.start, 0.5f, 0.0f, OvCore::Helpers::GUIDrawer::_MAX_FLOAT);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Fog Density", m_fog.density, 0.001f, 0.0f, OvCore::Helpers::GUIDrawer::_MAX_FLOAT);
	OvCore::Helpers::GUIDrawer::DrawScalar<float>(p_root, "Fog Max Opacity", m_fog.maxOpacity, 0.01f, 0.0f, 1.0f);
}

const OvCore::Rendering::PostProcess::PostProcessStack& OvCore::ECS::Components::CPostProcessStack::GetStack()
{
	return m_settings;
}

const OvCore::Rendering::PostProcess::BloomSettings& OvCore::ECS::Components::CPostProcessStack::GetBloomSettings() const
{
	return m_settings.Get<Rendering::PostProcess::BloomEffect, Rendering::PostProcess::BloomSettings>();
}

const OvCore::Rendering::PostProcess::AutoExposureSettings& OvCore::ECS::Components::CPostProcessStack::GetAutoExposureSettings() const
{
	return m_settings.Get<Rendering::PostProcess::AutoExposureEffect, Rendering::PostProcess::AutoExposureSettings>();
}

const OvCore::Rendering::PostProcess::TonemappingSettings& OvCore::ECS::Components::CPostProcessStack::GetTonemappingSettings() const
{
	return m_settings.Get<Rendering::PostProcess::TonemappingEffect, Rendering::PostProcess::TonemappingSettings>();
}

const OvCore::Rendering::PostProcess::FXAASettings& OvCore::ECS::Components::CPostProcessStack::GetFXAASettings() const
{
	return m_settings.Get<Rendering::PostProcess::FXAAEffect, Rendering::PostProcess::FXAASettings>();
}

void OvCore::ECS::Components::CPostProcessStack::SetBloomSettings(const OvCore::Rendering::PostProcess::BloomSettings& p_settings)
{
	m_settings.Set<Rendering::PostProcess::BloomEffect>(p_settings);
}

void OvCore::ECS::Components::CPostProcessStack::SetAutoExposureSettings(const OvCore::Rendering::PostProcess::AutoExposureSettings& p_settings)
{
	m_settings.Set<Rendering::PostProcess::AutoExposureEffect>(p_settings);
}

void OvCore::ECS::Components::CPostProcessStack::SetTonemappingSettings(const OvCore::Rendering::PostProcess::TonemappingSettings& p_settings)
{
	m_settings.Set<Rendering::PostProcess::TonemappingEffect>(p_settings);
}

void OvCore::ECS::Components::CPostProcessStack::SetFXAASettings(const OvCore::Rendering::PostProcess::FXAASettings& p_settings)
{
	m_settings.Set<Rendering::PostProcess::FXAAEffect>(p_settings);
}

const OvCore::Rendering::FogSettings& OvCore::ECS::Components::CPostProcessStack::GetFogSettings() const
{
	return m_fog;
}

void OvCore::ECS::Components::CPostProcessStack::SetFogSettings(const OvCore::Rendering::FogSettings& p_settings)
{
	m_fog = p_settings;
}
