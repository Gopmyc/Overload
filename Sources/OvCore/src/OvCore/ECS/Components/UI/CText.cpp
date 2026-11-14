/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/UI/CText.h>
#include <OvCore/ECS/Components/UI/TextLayoutEngine.h>
#include <OvCore/ECS/Components/UI/TextMeshBuilder.h>
#include <OvCore/ECS/Components/UI/UIInspectorUtils.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Helpers/GUIDrawer.h>
#include <OvCore/Helpers/Serializer.h>
#include <OvCore/Localization/Localization.h>
#include <OvCore/ResourceManagement/FontManager.h>
#include <OvCore/ResourceManagement/MaterialManager.h>
#include <OvCore/ResourceManagement/UIResourceRegistry.h>
#include <OvTools/Utils/PathParser.h>
#include <OvUI/Types/Color.h>
#include <OvUI/Widgets/InputFields/InputText.h>
#include <OvUI/Widgets/Selection/ComboBox.h>

namespace
{
	constexpr float kMinimumFontSize = 1.0f;
	constexpr float kSizeUpdateEpsilon = 0.0001f;
	constexpr const char* kColorUniform = "u_Color";
	constexpr const char* kFontAtlasUniform = "u_FontAtlas";

	float ClampFinite(float p_value, float p_min)
	{
		return std::isfinite(p_value) ? std::max(p_value, p_min) : p_min;
	}

	float KeepFinite(float p_value, float p_fallback)
	{
		return std::isfinite(p_value) ? p_value : p_fallback;
	}

	OvUI::Types::Color ToColor(const OvMaths::FVector4& p_value)
	{
		return { p_value.x, p_value.y, p_value.z, p_value.w };
	}

	OvMaths::FVector4 ToVec4(const OvUI::Types::Color& p_value)
	{
		return { p_value.r, p_value.g, p_value.b, p_value.a };
	}

	OvCore::ECS::Components::UI::CText::EHorizontalAlignment ToHorizontalAlignment(int p_value)
	{
		using EHorizontalAlignment = OvCore::ECS::Components::UI::CText::EHorizontalAlignment;

		switch (p_value)
		{
		case static_cast<int>(EHorizontalAlignment::CENTER):
			return EHorizontalAlignment::CENTER;
		case static_cast<int>(EHorizontalAlignment::RIGHT):
			return EHorizontalAlignment::RIGHT;
		case static_cast<int>(EHorizontalAlignment::LEFT):
		default:
			return EHorizontalAlignment::LEFT;
		}
	}

	OvCore::ECS::Components::UI::CText::EVerticalAlignment ToVerticalAlignment(int p_value)
	{
		using EVerticalAlignment = OvCore::ECS::Components::UI::CText::EVerticalAlignment;

		switch (p_value)
		{
		case static_cast<int>(EVerticalAlignment::CENTER):
			return EVerticalAlignment::CENTER;
		case static_cast<int>(EVerticalAlignment::BOTTOM):
			return EVerticalAlignment::BOTTOM;
		case static_cast<int>(EVerticalAlignment::TOP):
		default:
			return EVerticalAlignment::TOP;
		}
	}

	bool IsNearlyEqual(float p_left, float p_right)
	{
		return std::abs(p_left - p_right) <= kSizeUpdateEpsilon;
	}

	bool IsSameSize(const OvMaths::FVector2& p_left, const OvMaths::FVector2& p_right)
	{
		return IsNearlyEqual(p_left.x, p_right.x) && IsNearlyEqual(p_left.y, p_right.y);
	}

	OvCore::ECS::Components::UI::TextLayoutEngine::EHorizontalAlignment ToTextLayoutAlignment(
		OvCore::ECS::Components::UI::CText::EHorizontalAlignment p_alignment
	)
	{
		using EHorizontalAlignment = OvCore::ECS::Components::UI::CText::EHorizontalAlignment;
		using ELayoutAlignment = OvCore::ECS::Components::UI::TextLayoutEngine::EHorizontalAlignment;

		switch (p_alignment)
		{
		case EHorizontalAlignment::CENTER:
			return ELayoutAlignment::CENTER;
		case EHorizontalAlignment::RIGHT:
			return ELayoutAlignment::RIGHT;
		case EHorizontalAlignment::LEFT:
		default:
			return ELayoutAlignment::LEFT;
		}
	}

	OvCore::ECS::Components::UI::TextLayoutEngine::EVerticalAlignment ToTextLayoutAlignment(
		OvCore::ECS::Components::UI::CText::EVerticalAlignment p_alignment
	)
	{
		using EVerticalAlignment = OvCore::ECS::Components::UI::CText::EVerticalAlignment;
		using ELayoutAlignment = OvCore::ECS::Components::UI::TextLayoutEngine::EVerticalAlignment;

		switch (p_alignment)
		{
		case EVerticalAlignment::CENTER:
			return ELayoutAlignment::CENTER;
		case EVerticalAlignment::BOTTOM:
			return ELayoutAlignment::BOTTOM;
		case EVerticalAlignment::TOP:
		default:
			return ELayoutAlignment::TOP;
		}
	}
}

OvCore::ECS::Components::UI::CText::CText(ECS::Actor& p_owner) :
AComponent(p_owner)
{
	m_fontPath = Global::ServiceLocator::Get<ResourceManagement::UIResourceRegistry>().GetDefinition().defaultFontPath;
	owner.transform.EnableUIData();
}

std::string OvCore::ECS::Components::UI::CText::GetName()
{
	return "Text";
}

std::string OvCore::ECS::Components::UI::CText::GetTypeName()
{
	return std::string{ComponentTraits<CText>::Name};
}

void OvCore::ECS::Components::UI::CText::SetText(const std::string& p_text)
{
	m_text = p_text;
	m_textKey.clear();
	MarkMeshDirty();
}

const std::string& OvCore::ECS::Components::UI::CText::GetText() const
{
	ResolveTextKey();
	return m_text;
}

void OvCore::ECS::Components::UI::CText::SetTextKey(const std::string& p_key)
{
	m_textKey = p_key;
	m_textKeyDirty = true;
}

const std::string& OvCore::ECS::Components::UI::CText::GetTextKey() const
{
	return m_textKey;
}

void OvCore::ECS::Components::UI::CText::SetFontPath(const std::string& p_fontPath)
{
	m_fontPath = p_fontPath;
	m_unavailableFontPath.clear();
	MarkMeshDirty();
	MarkMaterialSourceDirty();
}

const std::string& OvCore::ECS::Components::UI::CText::GetFontPath() const
{
	return m_fontPath;
}

void OvCore::ECS::Components::UI::CText::SetFontSize(float p_fontSize)
{
	m_fontSize = ClampFinite(p_fontSize, kMinimumFontSize);
	MarkMeshDirty();
	MarkMaterialSourceDirty();
}

float OvCore::ECS::Components::UI::CText::GetFontSize() const
{
	return m_fontSize;
}

void OvCore::ECS::Components::UI::CText::SetColor(const OvMaths::FVector4& p_color)
{
	m_color.x = KeepFinite(p_color.x, m_color.x);
	m_color.y = KeepFinite(p_color.y, m_color.y);
	m_color.z = KeepFinite(p_color.z, m_color.z);
	m_color.w = KeepFinite(p_color.w, m_color.w);
	MarkMaterialColorDirty();
}

const OvMaths::FVector4& OvCore::ECS::Components::UI::CText::GetColor() const
{
	return m_color;
}

void OvCore::ECS::Components::UI::CText::SetHorizontalAlignment(EHorizontalAlignment p_alignment)
{
	m_horizontalAlignment = ToHorizontalAlignment(static_cast<int>(p_alignment));
	MarkMeshDirty();
}

OvCore::ECS::Components::UI::CText::EHorizontalAlignment OvCore::ECS::Components::UI::CText::GetHorizontalAlignment() const
{
	return m_horizontalAlignment;
}

void OvCore::ECS::Components::UI::CText::SetVerticalAlignment(EVerticalAlignment p_alignment)
{
	m_verticalAlignment = ToVerticalAlignment(static_cast<int>(p_alignment));
	MarkMeshDirty();
}

OvCore::ECS::Components::UI::CText::EVerticalAlignment OvCore::ECS::Components::UI::CText::GetVerticalAlignment() const
{
	return m_verticalAlignment;
}

void OvCore::ECS::Components::UI::CText::SetLit(bool p_lit)
{
	m_lit = p_lit;
}

bool OvCore::ECS::Components::UI::CText::IsLit() const
{
	return m_lit;
}

OvRendering::Resources::Mesh* OvCore::ECS::Components::UI::CText::GetMesh() const
{
	RebuildMesh();
	return m_mesh.get();
}

OvRendering::Resources::Mesh* OvCore::ECS::Components::UI::CText::GetMesh(const OvMaths::FVector2& p_resolvedSize) const
{
	RebuildMesh(p_resolvedSize);
	return m_mesh.get();
}

OvRendering::Data::Material* OvCore::ECS::Components::UI::CText::GetMaterial()
{
	RefreshMaterial();
	return m_material && m_material->IsValid() ? m_material.get() : nullptr;
}

OvRendering::Data::Material* OvCore::ECS::Components::UI::CText::GetLitMaterial()
{
	RefreshLitMaterial();
	return m_litMaterial && m_litMaterial->IsValid() ? m_litMaterial.get() : nullptr;
}

const OvMaths::FVector2& OvCore::ECS::Components::UI::CText::GetSize() const
{
	RebuildLayout();
	return m_size;
}

OvMaths::FVector2 OvCore::ECS::Components::UI::CText::GetSize(const OvMaths::FVector2& p_resolvedSize) const
{
	RebuildLayout(p_resolvedSize);
	return m_size;
}

OvMaths::FVector2 OvCore::ECS::Components::UI::CText::GetContentSize() const
{
	RebuildLayout();
	return m_layout.contentSize;
}

void OvCore::ECS::Components::UI::CText::OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	Helpers::Serializer::SerializeString(p_doc, p_node, "text", GetText());
	Helpers::Serializer::SerializeString(p_doc, p_node, "text_key", m_textKey);
	Helpers::Serializer::SerializeString(p_doc, p_node, "font_path", m_fontPath);
	Helpers::Serializer::SerializeFloat(p_doc, p_node, "font_size", m_fontSize);
	Helpers::Serializer::SerializeVec4(p_doc, p_node, "color", m_color);
	Helpers::Serializer::SerializeInt(p_doc, p_node, "horizontal_alignment", static_cast<int>(m_horizontalAlignment));
	Helpers::Serializer::SerializeInt(p_doc, p_node, "vertical_alignment", static_cast<int>(m_verticalAlignment));
	Helpers::Serializer::SerializeBoolean(p_doc, p_node, "lit", m_lit);
}

void OvCore::ECS::Components::UI::CText::OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node)
{
	if (p_node->FirstChildElement("text"))
	{
		auto text = m_text;
		Helpers::Serializer::DeserializeString(p_doc, p_node, "text", text);
		SetText(text);
	}

	// After the text, which clears a key
	if (p_node->FirstChildElement("text_key"))
	{
		auto textKey = m_textKey;
		Helpers::Serializer::DeserializeString(p_doc, p_node, "text_key", textKey);
		SetTextKey(textKey);
	}

	if (p_node->FirstChildElement("font_path"))
	{
		auto fontPath = m_fontPath;
		Helpers::Serializer::DeserializeString(p_doc, p_node, "font_path", fontPath);
		SetFontPath(fontPath);
	}

	if (p_node->FirstChildElement("font_size"))
	{
		auto fontSize = m_fontSize;
		Helpers::Serializer::DeserializeFloat(p_doc, p_node, "font_size", fontSize);
		SetFontSize(fontSize);
	}

	if (p_node->FirstChildElement("color"))
	{
		auto color = m_color;
		Helpers::Serializer::DeserializeVec4(p_doc, p_node, "color", color);
		SetColor(color);
	}

	if (p_node->FirstChildElement("horizontal_alignment"))
	{
		auto horizontalAlignment = static_cast<int>(m_horizontalAlignment);
		Helpers::Serializer::DeserializeInt(p_doc, p_node, "horizontal_alignment", horizontalAlignment);
		SetHorizontalAlignment(ToHorizontalAlignment(horizontalAlignment));
	}

	if (p_node->FirstChildElement("vertical_alignment"))
	{
		auto verticalAlignment = static_cast<int>(m_verticalAlignment);
		Helpers::Serializer::DeserializeInt(p_doc, p_node, "vertical_alignment", verticalAlignment);
		SetVerticalAlignment(ToVerticalAlignment(verticalAlignment));
	}

	if (p_node->FirstChildElement("lit"))
	{
		auto lit = m_lit;
		Helpers::Serializer::DeserializeBoolean(p_doc, p_node, "lit", lit);
		SetLit(lit);
	}
}

void OvCore::ECS::Components::UI::CText::OnInspector(OvUI::Internal::WidgetContainer& p_root)
{
	UIInspectorUtils::DrawCanvasRequirement(p_root, owner);

	Helpers::GUIDrawer::CreateTitle(p_root, "Text");
	auto& textInput = p_root.CreateWidget<OvUI::Widgets::InputFields::InputText>("");
	textInput.multiline = true;
	textInput.multilineHeight = 100.0f;
	textInput.fullWidth = true;

	auto& textDispatcher = textInput.AddPlugin<OvUI::Plugins::DataDispatcher<std::string>>();
	textDispatcher.RegisterGatherer([this]() { return GetText(); });
	textDispatcher.RegisterProvider([this](std::string p_value) { SetText(p_value); });

	// A key shows its text above, in the current language; typing a text there clears the key
	Helpers::GUIDrawer::DrawString(
		p_root,
		"Text Key",
		[this]() { return GetTextKey(); },
		[this](std::string p_value) { SetTextKey(p_value); }
	);

	Helpers::GUIDrawer::DrawAsset(
		p_root,
		"Font",
		[this]() { return GetFontPath(); },
		[this](std::string p_value) { SetFontPath(p_value); },
		OvTools::Utils::PathParser::EFileType::FONT
	);

	Helpers::GUIDrawer::DrawScalar<float>(
		p_root,
		"Font Size",
		std::bind(&CText::GetFontSize, this),
		std::bind(&CText::SetFontSize, this, std::placeholders::_1),
		1.0f,
		kMinimumFontSize
	);

	Helpers::GUIDrawer::CreateTitle(p_root, "Horizontal Alignment");
	auto& horizontalAlignment = p_root.CreateWidget<OvUI::Widgets::Selection::ComboBox>(static_cast<int>(GetHorizontalAlignment()));
	horizontalAlignment.choices.emplace(static_cast<int>(EHorizontalAlignment::LEFT), "Left");
	horizontalAlignment.choices.emplace(static_cast<int>(EHorizontalAlignment::CENTER), "Center");
	horizontalAlignment.choices.emplace(static_cast<int>(EHorizontalAlignment::RIGHT), "Right");
	horizontalAlignment.ValueChangedEvent += [this](int p_choice)
	{
		SetHorizontalAlignment(ToHorizontalAlignment(p_choice));
	};

	Helpers::GUIDrawer::CreateTitle(p_root, "Vertical Alignment");
	auto& verticalAlignment = p_root.CreateWidget<OvUI::Widgets::Selection::ComboBox>(static_cast<int>(GetVerticalAlignment()));
	verticalAlignment.choices.emplace(static_cast<int>(EVerticalAlignment::TOP), "Top");
	verticalAlignment.choices.emplace(static_cast<int>(EVerticalAlignment::CENTER), "Center");
	verticalAlignment.choices.emplace(static_cast<int>(EVerticalAlignment::BOTTOM), "Bottom");
	verticalAlignment.ValueChangedEvent += [this](int p_choice)
	{
		SetVerticalAlignment(ToVerticalAlignment(p_choice));
	};

	Helpers::GUIDrawer::DrawColor(
		p_root,
		"Color",
		[this]() { return ToColor(m_color); },
		[this](OvUI::Types::Color p_value) { SetColor(ToVec4(p_value)); },
		true
	);

	// Under a world space canvas only: a screen space text stays unlit
	Helpers::GUIDrawer::DrawBoolean(
		p_root,
		"Lit",
		[this]() { return IsLit(); },
		[this](bool p_value) { SetLit(p_value); }
	);
}

const std::string& OvCore::ECS::Components::UI::CText::GetDisplayedFontPath() const
{
	// The font that stands for this one in the current language (a table's "fonts")
	if (Global::ServiceLocator::Has<Localization::Localization>())
	{
		return Global::ServiceLocator::Get<Localization::Localization>().GetFont(m_fontPath);
	}

	return m_fontPath;
}

OvRendering::Resources::Font* OvCore::ECS::Components::UI::CText::GetFont() const
{
	const std::string& fontPath = GetDisplayedFontPath();

	if (fontPath.empty() || fontPath == "?")
	{
		return nullptr;
	}

	auto& fontManager = Global::ServiceLocator::Get<ResourceManagement::FontManager>();
	if (m_unavailableFontPath == fontPath)
	{
		auto* font = fontManager.GetResource(fontPath, false);
		if (font)
		{
			m_unavailableFontPath.clear();
		}

		return font;
	}

	auto* font = fontManager.GetResource(fontPath);
	if (!font)
	{
		m_unavailableFontPath = fontPath;
	}

	return font;
}

void OvCore::ECS::Components::UI::CText::ResolveTextKey() const
{
	if (m_textKey.empty())
	{
		return;
	}

	const bool localized = Global::ServiceLocator::Has<Localization::Localization>();
	const uint64_t revision = localized ? Global::ServiceLocator::Get<Localization::Localization>().GetRevision() : 0;

	if (!m_textKeyDirty && revision == m_textKeyRevision)
	{
		return;
	}

	m_textKeyDirty = false;
	m_textKeyRevision = revision;

	std::string text = localized ? Global::ServiceLocator::Get<Localization::Localization>().Get(m_textKey) : m_textKey;
	if (text != m_text)
	{
		m_text = std::move(text);
		m_layoutDirty = true;
		m_meshDirty = true;
	}
}

void OvCore::ECS::Components::UI::CText::MarkMeshDirty()
{
	m_layoutDirty = true;
	m_meshDirty = true;
}

void OvCore::ECS::Components::UI::CText::MarkMaterialSourceDirty()
{
	m_materialSourceDirty = true;
	m_materialColorDirty = true;
}

void OvCore::ECS::Components::UI::CText::MarkMaterialColorDirty()
{
	m_materialColorDirty = true;
	m_litColorDirty = true;
}

void OvCore::ECS::Components::UI::CText::RebuildLayout() const
{
	RebuildLayout(owner.transform.GetUISize());
}

void OvCore::ECS::Components::UI::CText::RebuildLayout(const OvMaths::FVector2& p_uiSize) const
{
	ResolveTextKey();

	auto* font = GetFont();
	const auto fontManagerRevision = Global::ServiceLocator::Get<ResourceManagement::FontManager>().GetResourcesRevision();
	const uint64_t fontRevision = font ? font->GetRevision() : 0;
	const bool fontChanged =
		font != m_layoutFont ||
		fontRevision != m_layoutFontRevision ||
		fontManagerRevision != m_layoutFontManagerRevision;

	if (!m_layoutDirty && !fontChanged && IsSameSize(m_lastLayoutUISize, p_uiSize))
	{
		return;
	}

	m_meshDirty = true;
	m_layoutFont = font;
	m_layoutFontRevision = fontRevision;
	m_layoutFontManagerRevision = fontManagerRevision;

	if (!font)
	{
		m_layout = {};
		m_size = OvMaths::FVector2::Zero;
		m_lastLayoutUISize = p_uiSize;
		m_layoutDirty = false;
		m_layoutAvailable = false;
		m_meshDirty = true;
		return;
	}

	m_layoutDirty = false;
	m_layoutAvailable = true;
	m_lastLayoutUISize = p_uiSize;

	m_layout = TextLayoutEngine::Layout({
		.text = m_text,
		.font = font,
		.fontSize = m_fontSize,
		.uiSize = p_uiSize,
		.horizontalAlignment = ToTextLayoutAlignment(m_horizontalAlignment),
		.verticalAlignment = ToTextLayoutAlignment(m_verticalAlignment)
	});

	m_size = m_layout.size;
}

void OvCore::ECS::Components::UI::CText::RebuildMesh() const
{
	RebuildMesh(owner.transform.GetUISize());
}

void OvCore::ECS::Components::UI::CText::RebuildMesh(const OvMaths::FVector2& p_uiSize) const
{
	RebuildLayout(p_uiSize);

	if (!m_meshDirty && IsSameSize(m_lastMeshUISize, p_uiSize))
	{
		return;
	}

	if (!m_layoutAvailable)
	{
		m_mesh.reset();
		m_meshDirty = false;
		m_lastMeshUISize = p_uiSize;
		return;
	}

	m_meshDirty = false;
	m_lastMeshUISize = p_uiSize;

	auto textMesh = TextMeshBuilder::Build(m_layout);

	m_size = textMesh.size;
	m_lastLayoutUISize = p_uiSize;
	m_mesh = std::move(textMesh.mesh);
}

void OvCore::ECS::Components::UI::CText::RefreshMaterial()
{
	auto& fontManager = Global::ServiceLocator::Get<ResourceManagement::FontManager>();
	auto& materialManager = Global::ServiceLocator::Get<ResourceManagement::MaterialManager>();

	if (!m_material)
	{
		m_material = std::make_unique<OvRendering::Data::Material>();
		MarkMaterialSourceDirty();
	}

	if (!m_materialSourceDirty)
	{
		const auto& textMaterialPath = Global::ServiceLocator::Get<ResourceManagement::UIResourceRegistry>().GetDefinition().textMaterialPath;
		auto* defaultMaterial = textMaterialPath.empty() ?
			nullptr :
			materialManager.GetResource(textMaterialPath, false);
		auto* currentShader = defaultMaterial && defaultMaterial->HasShader() ? defaultMaterial->GetShader() : nullptr;
		const std::string& fontPath = GetDisplayedFontPath();
		auto* currentFont = fontPath.empty() || fontPath == "?" ?
			nullptr :
			fontManager.GetResource(fontPath, false);

		if (
			materialManager.GetResourcesRevision() != m_materialManagerRevision ||
			fontManager.GetResourcesRevision() != m_materialFontManagerRevision ||
			currentShader != m_materialShader ||
			currentFont != m_materialFont ||
			(currentFont && currentFont->GetRevision() != m_materialFontRevision)
		)
		{
			MarkMaterialSourceDirty();
		}
	}

	if (m_materialSourceDirty)
	{
		const auto& textMaterialPath = Global::ServiceLocator::Get<ResourceManagement::UIResourceRegistry>().GetDefinition().textMaterialPath;
		auto* defaultMaterial = textMaterialPath.empty() ?
			nullptr :
			materialManager.GetResource(textMaterialPath);
		auto* defaultShader = defaultMaterial && defaultMaterial->HasShader() ? defaultMaterial->GetShader() : nullptr;
		auto* font = GetFont();

		m_materialFont = font;
		m_materialShader = defaultShader;
		m_materialFontRevision = font ? font->GetRevision() : 0;
		m_materialFontManagerRevision = fontManager.GetResourcesRevision();
		m_materialManagerRevision = materialManager.GetResourcesRevision();

		if (!defaultShader || !font || !font->EnsureEmbeddedMaterial(defaultShader, m_fontSize))
		{
			if (m_material->HasShader())
			{
				m_material->SetShader(nullptr);
			}

			m_materialColorDirty = true;
			return;
		}

		auto* embeddedMaterial = font->GetEmbeddedMaterial(m_fontSize);
		if (!embeddedMaterial || !embeddedMaterial->IsValid())
		{
			if (m_material->HasShader())
			{
				m_material->SetShader(nullptr);
			}

			m_materialColorDirty = true;
			return;
		}

		*m_material = *embeddedMaterial;
		m_materialFontRevision = font->GetRevision();
		m_materialSourceDirty = false;
		m_materialColorDirty = true;
	}

	if (!m_material->IsValid())
	{
		return;
	}

	if (m_materialColorDirty)
	{
		m_material->TrySetProperty(kColorUniform, m_color);
		m_materialColorDirty = false;
	}
}

void OvCore::ECS::Components::UI::CText::RefreshLitMaterial()
{
	auto& materialManager = Global::ServiceLocator::Get<ResourceManagement::MaterialManager>();
	const auto& litTextMaterialPath = Global::ServiceLocator::Get<ResourceManagement::UIResourceRegistry>().GetDefinition().litTextMaterialPath;
	// Loaded once for each change of the manager's resources: a missing file is not looked for at every frame
	const bool load = !m_litMaterial || materialManager.GetResourcesRevision() != m_litManagerRevision;
	OvRendering::Data::Material* source = litTextMaterialPath.empty() ?
		nullptr :
		materialManager.GetResource(litTextMaterialPath, load);
	auto* shader = source && source->HasShader() ? source->GetShader() : nullptr;
	auto* font = GetFont();
	auto* atlas = font ? font->GetAtlasTexture(m_fontSize) : nullptr;

	if (!m_litMaterial)
	{
		m_litMaterial = std::make_unique<OvRendering::Data::Material>();
		m_litSource = nullptr;
	}

	// A copy of the host's material, settings and uniforms included, made again whenever it may have changed
	if (
		source != m_litSource ||
		shader != m_litShader ||
		atlas != m_litAtlas ||
		materialManager.GetResourcesRevision() != m_litManagerRevision
	)
	{
		m_litSource = source;
		m_litShader = shader;
		m_litAtlas = atlas;
		m_litManagerRevision = materialManager.GetResourcesRevision();
		m_litColorDirty = true;

		if (!source || !source->IsValid() || !atlas)
		{
			if (m_litMaterial->HasShader())
			{
				m_litMaterial->SetShader(nullptr);
			}

			return;
		}

		*m_litMaterial = *source;
		m_litMaterial->TrySetProperty(kFontAtlasUniform, atlas);
	}

	if (!m_litMaterial->IsValid())
	{
		return;
	}

	if (m_litColorDirty)
	{
		m_litMaterial->TrySetProperty(kColorUniform, m_color);
		m_litColorDirty = false;
	}
}
