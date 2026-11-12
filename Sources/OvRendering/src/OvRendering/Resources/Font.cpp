/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <vector>

#include <OvDebug/Logger.h>
#include <OvRendering/Resources/Font.h>
#include <OvRendering/Resources/Loaders/TextureLoader.h>

#include <baregl/types/ETextureFilteringMode.h>
#include <baregl/types/ETextureWrapMode.h>

namespace
{
	constexpr float kDefaultPixelSize = 32.0f;
	constexpr float kMinimumPixelSize = 1.0f;
	constexpr float kMaximumPixelSize = 256.0f;
	constexpr const char* kFontAtlasUniform = "u_FontAtlas";

	uint32_t ToPixelSizeKey(float p_pixelSize)
	{
		if (!std::isfinite(p_pixelSize))
		{
			return static_cast<uint32_t>(kDefaultPixelSize);
		}

		const auto clamped = std::clamp(p_pixelSize, kMinimumPixelSize, kMaximumPixelSize);
		return std::max<uint32_t>(1, static_cast<uint32_t>(std::lround(clamped)));
	}

	void ConfigureEmbeddedMaterial(OvRendering::Data::Material& p_material)
	{
		p_material.SetOrthographicSupport(true);
		p_material.SetPerspectiveSupport(true);
		p_material.SetBlendable(true);
		p_material.SetUserInterface(true);
		p_material.SetBackfaceCulling(false);
		p_material.SetFrontfaceCulling(false);
		p_material.SetDepthTest(false);
		p_material.SetDepthWriting(false);
		p_material.SetColorWriting(true);
		p_material.SetCastShadows(false);
		p_material.SetReceiveShadows(false);
		p_material.SetCapturedByReflectionProbes(false);
		p_material.SetReceiveReflections(false);
		p_material.SetGPUInstances(1);
	}

	std::vector<uint8_t> ReadFile(const std::filesystem::path& p_path)
	{
		std::ifstream file{ p_path, std::ios::binary | std::ios::ate };
		if (!file)
		{
			return {};
		}

		const auto size = file.tellg();
		if (size <= std::streampos{ 0 })
		{
			return {};
		}

		std::vector<uint8_t> data(static_cast<size_t>(size));
		file.seekg(0, std::ios::beg);
		file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));

		return file ? data : std::vector<uint8_t>{};
	}

	/**
	* The atlas's alpha as the texture holds it: white, with the alpha as coverage
	*/
	std::vector<uint8_t> ToRGBA(const std::vector<uint8_t>& p_alpha)
	{
		std::vector<uint8_t> rgba(p_alpha.size() * 4, 255);

		for (size_t i = 0; i < p_alpha.size(); ++i)
		{
			rgba[i * 4 + 3] = p_alpha[i];
		}

		return rgba;
	}
}

OvRendering::Resources::Font::Font(const std::string& p_path, const std::filesystem::path& p_realPath) :
	path(p_path)
{
	Reload(p_realPath);
}

OvRendering::Resources::Font::~Font()
{
	DestroyAtlasVariants();
}

bool OvRendering::Resources::Font::Reload(const std::filesystem::path& p_realPath)
{
	m_realPath = p_realPath;
	m_activePixelSize = ToPixelSizeKey(kDefaultPixelSize);
	m_valid = false;
	DestroyAtlasVariants();
	m_atlasVariants.clear();
	++m_revision;

	auto fontData = ReadFile(p_realPath);
	m_fontData = fontData.empty() ? nullptr : std::make_shared<const std::vector<uint8_t>>(std::move(fontData));

	if (!m_fontData)
	{
		OVLOG_WARNING("Unable to read font: " + p_realPath.string());
	}

	return SetActivePixelSize(static_cast<float>(m_activePixelSize));
}

bool OvRendering::Resources::Font::SetActivePixelSize(float p_pixelSize)
{
	const auto pixelSize = ToPixelSizeKey(p_pixelSize);
	auto* variant = GetOrCreateVariant(pixelSize);
	if (!variant)
	{
		m_valid = false;
		return false;
	}

	m_activePixelSize = pixelSize;
	m_valid = variant->valid && variant->atlasTexture;
	return m_valid;
}

bool OvRendering::Resources::Font::EnsurePixelSize(float p_pixelSize)
{
	return GetOrCreateVariant(ToPixelSizeKey(p_pixelSize)) != nullptr;
}

bool OvRendering::Resources::Font::IsValid() const
{
	const auto* variant = GetActiveVariant();
	return m_valid && variant && variant->atlasTexture;
}

uint64_t OvRendering::Resources::Font::GetRevision() const
{
	return m_revision;
}

float OvRendering::Resources::Font::GetPixelSize() const
{
	if (const auto* variant = GetActiveVariant(); variant)
	{
		return variant->pixelSize;
	}

	return static_cast<float>(m_activePixelSize);
}

float OvRendering::Resources::Font::GetPixelSize(float p_pixelSize) const
{
	if (const auto* variant = GetVariant(ToPixelSizeKey(p_pixelSize)); variant)
	{
		return variant->pixelSize;
	}

	return static_cast<float>(ToPixelSizeKey(p_pixelSize));
}

float OvRendering::Resources::Font::GetLineHeight() const
{
	if (const auto* variant = GetActiveVariant(); variant)
	{
		return variant->lineHeight;
	}

	return static_cast<float>(m_activePixelSize);
}

float OvRendering::Resources::Font::GetLineHeight(float p_pixelSize) const
{
	if (const auto* variant = GetVariant(ToPixelSizeKey(p_pixelSize)); variant)
	{
		return variant->lineHeight;
	}

	return static_cast<float>(ToPixelSizeKey(p_pixelSize));
}

float OvRendering::Resources::Font::GetAscender() const
{
	if (const auto* variant = GetActiveVariant(); variant)
	{
		return variant->ascender;
	}

	return static_cast<float>(m_activePixelSize) * 0.8f;
}

float OvRendering::Resources::Font::GetAscender(float p_pixelSize) const
{
	if (const auto* variant = GetVariant(ToPixelSizeKey(p_pixelSize)); variant)
	{
		return variant->ascender;
	}

	return static_cast<float>(ToPixelSizeKey(p_pixelSize)) * 0.8f;
}

float OvRendering::Resources::Font::GetDescender() const
{
	if (const auto* variant = GetActiveVariant(); variant)
	{
		return variant->descender;
	}

	return -static_cast<float>(m_activePixelSize) * 0.2f;
}

float OvRendering::Resources::Font::GetDescender(float p_pixelSize) const
{
	const auto* variant = GetVariant(ToPixelSizeKey(p_pixelSize));
	return variant ? variant->descender : -static_cast<float>(ToPixelSizeKey(p_pixelSize)) * 0.2f;
}

bool OvRendering::Resources::Font::EnsureGlyphs(std::span<const uint32_t> p_codePoints, float p_pixelSize)
{
	auto* variant = GetOrCreateVariant(ToPixelSizeKey(p_pixelSize));
	if (!variant)
	{
		return false;
	}

	// The texture is filled again in place: materials that hold it see the new glyphs
	return !variant->atlas->Bake(p_codePoints) || UploadAtlas(*variant);
}

const OvRendering::Resources::Font::Glyph* OvRendering::Resources::Font::GetGlyph(uint32_t p_codePoint) const
{
	const auto* variant = GetActiveVariant();
	return variant && variant->atlas ? variant->atlas->GetGlyph(p_codePoint) : nullptr;
}

const OvRendering::Resources::Font::Glyph* OvRendering::Resources::Font::GetGlyph(uint32_t p_codePoint, float p_pixelSize) const
{
	const auto* variant = GetVariant(ToPixelSizeKey(p_pixelSize));
	return variant && variant->atlas ? variant->atlas->GetGlyph(p_codePoint) : nullptr;
}

OvRendering::Resources::Texture* OvRendering::Resources::Font::GetAtlasTexture() const
{
	if (const auto* variant = GetActiveVariant(); variant)
	{
		return variant->atlasTexture;
	}

	return nullptr;
}

OvRendering::Resources::Texture* OvRendering::Resources::Font::GetAtlasTexture(float p_pixelSize)
{
	if (auto* variant = GetOrCreateVariant(ToPixelSizeKey(p_pixelSize)); variant)
	{
		return variant->atlasTexture;
	}

	return nullptr;
}

bool OvRendering::Resources::Font::EnsureEmbeddedMaterial(Shader* p_shader)
{
	return EnsureEmbeddedMaterial(p_shader, static_cast<float>(m_activePixelSize));
}

bool OvRendering::Resources::Font::EnsureEmbeddedMaterial(Shader* p_shader, float p_pixelSize)
{
	auto* variant = GetOrCreateVariant(ToPixelSizeKey(p_pixelSize));
	if (!variant)
	{
		return false;
	}

	if (!p_shader || !variant->atlasTexture)
	{
		variant->embeddedMaterial.reset();
		return false;
	}

	if (!variant->embeddedMaterial)
	{
		variant->embeddedMaterial = std::make_unique<Data::Material>(p_shader);
		ConfigureEmbeddedMaterial(*variant->embeddedMaterial);
	}
	else if (variant->embeddedMaterial->GetShader() != p_shader)
	{
		variant->embeddedMaterial->SetShader(p_shader);
		ConfigureEmbeddedMaterial(*variant->embeddedMaterial);
	}

	if (!variant->embeddedMaterial->IsValid())
	{
		return false;
	}

	variant->embeddedMaterial->TrySetProperty(kFontAtlasUniform, variant->atlasTexture);
	return true;
}

OvRendering::Data::Material* OvRendering::Resources::Font::GetEmbeddedMaterial() const
{
	if (const auto* variant = GetActiveVariant(); variant)
	{
		return variant->embeddedMaterial.get();
	}

	return nullptr;
}

OvRendering::Data::Material* OvRendering::Resources::Font::GetEmbeddedMaterial(float p_pixelSize)
{
	if (auto* variant = GetOrCreateVariant(ToPixelSizeKey(p_pixelSize)); variant)
	{
		return variant->embeddedMaterial.get();
	}

	return nullptr;
}

OvRendering::Resources::Font::AtlasVariant* OvRendering::Resources::Font::GetActiveVariant()
{
	return GetVariant(m_activePixelSize);
}

const OvRendering::Resources::Font::AtlasVariant* OvRendering::Resources::Font::GetActiveVariant() const
{
	return GetVariant(m_activePixelSize);
}

OvRendering::Resources::Font::AtlasVariant* OvRendering::Resources::Font::GetVariant(uint32_t p_pixelSize)
{
	const auto found = m_atlasVariants.find(p_pixelSize);
	return found != m_atlasVariants.end() ? &found->second : nullptr;
}

const OvRendering::Resources::Font::AtlasVariant* OvRendering::Resources::Font::GetVariant(uint32_t p_pixelSize) const
{
	const auto found = m_atlasVariants.find(p_pixelSize);
	return found != m_atlasVariants.end() ? &found->second : nullptr;
}

OvRendering::Resources::Font::AtlasVariant* OvRendering::Resources::Font::GetOrCreateVariant(uint32_t p_pixelSize)
{
	if (auto* variant = GetVariant(p_pixelSize))
	{
		return variant->valid ? variant : nullptr;
	}

	if (!CreateAtlasVariant(p_pixelSize))
	{
		auto& failedVariant = m_atlasVariants[p_pixelSize];
		failedVariant.valid = false;
		failedVariant.pixelSize = static_cast<float>(p_pixelSize);
		return nullptr;
	}

	if (auto* variant = GetVariant(p_pixelSize))
	{
		return variant->valid ? variant : nullptr;
	}

	return nullptr;
}

void OvRendering::Resources::Font::DestroyAtlasVariants()
{
	for (auto& [_, variant] : m_atlasVariants)
	{
		Loaders::TextureLoader::Destroy(variant.atlasTexture);
		variant.atlasTexture = nullptr;
		variant.atlas.reset();
		variant.embeddedMaterial.reset();
		variant.valid = false;
	}
}

bool OvRendering::Resources::Font::CreateAtlasVariant(uint32_t p_pixelSize)
{
	if (!m_fontData)
	{
		return false;
	}

	std::string error;
	auto atlas = FontAtlas::Create(m_fontData, p_pixelSize, error);
	if (!atlas)
	{
		OVLOG_WARNING(std::format("Unable to bake font {} at {} px: {}", m_realPath.string(), p_pixelSize, error));
		return false;
	}

	auto& variant = m_atlasVariants[p_pixelSize];
	variant.atlas = std::move(atlas);

	if (!UploadAtlas(variant))
	{
		variant.atlas.reset();
		return false;
	}

	variant.valid = true;
	variant.pixelSize = variant.atlas->GetPixelSize();
	variant.lineHeight = variant.atlas->GetLineHeight();
	variant.ascender = variant.atlas->GetAscender();
	variant.descender = variant.atlas->GetDescender();

	if (variant.embeddedMaterial && variant.embeddedMaterial->IsValid())
	{
		variant.embeddedMaterial->TrySetProperty(kFontAtlasUniform, variant.atlasTexture);
	}

	return true;
}

bool OvRendering::Resources::Font::UploadAtlas(AtlasVariant& p_variant)
{
	using namespace baregl::types;

	const uint32_t size = p_variant.atlas->GetSize();
	const auto pixels = ToRGBA(p_variant.atlas->GetPixels());

	if (p_variant.atlasTexture)
	{
		Loaders::TextureLoader::ReloadFromMemory(
			*p_variant.atlasTexture,
			pixels.data(),
			size,
			size,
			ETextureFilteringMode::LINEAR,
			ETextureFilteringMode::LINEAR,
			ETextureWrapMode::CLAMP_TO_EDGE,
			ETextureWrapMode::CLAMP_TO_EDGE,
			false
		);

		return true;
	}

	p_variant.atlasTexture = Loaders::TextureLoader::CreateFromMemory(
		pixels.data(),
		size,
		size,
		ETextureFilteringMode::LINEAR,
		ETextureFilteringMode::LINEAR,
		ETextureWrapMode::CLAMP_TO_EDGE,
		ETextureWrapMode::CLAMP_TO_EDGE,
		false
	);

	if (!p_variant.atlasTexture)
	{
		OVLOG_WARNING("Unable to create font atlas texture: " + m_realPath.string());
		return false;
	}

	return true;
}
