/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <array>
#include <initializer_list>
#include <memory>
#include <vector>

#include <OvDebug/Logger.h>
#include <OvRendering/Data/Image.h>
#include <OvRendering/Resources/Loaders/TextureLoader.h>
#include <OvTools/Utils/PathParser.h>

namespace
{
	/**
	* Keeps the given channels of RGBA8 pixels
	*/
	std::vector<uint8_t> PackChannels(const void* p_rgbaData, uint32_t p_width, uint32_t p_height, std::initializer_list<uint32_t> p_channels)
	{
		const auto* rgba = static_cast<const uint8_t*>(p_rgbaData);
		const size_t pixelCount = static_cast<size_t>(p_width) * p_height;

		std::vector<uint8_t> packed;
		packed.reserve(pixelCount * p_channels.size());

		for (size_t i = 0; i < pixelCount; ++i)
		{
			for (const auto channel : p_channels)
			{
				packed.push_back(rgba[i * 4 + channel]);
			}
		}

		return packed;
	}

	/**
	* @param p_sourceChannels Channel count of the source image (images are always decoded as RGBA)
	*/
	void PrepareTexture(
		baregl::Texture& p_texture,
		const void* p_data,
		baregl::types::ETextureFilteringMode p_minFilter,
		baregl::types::ETextureFilteringMode p_magFilter,
		baregl::types::ETextureWrapMode p_horizontalWrapMode,
		baregl::types::ETextureWrapMode p_verticalWrapMode,
		uint32_t p_width,
		uint32_t p_height,
		bool p_generateMipmap,
		bool p_hdr,
		int p_sourceChannels = 4
	)
	{
		using namespace baregl::types;

		// Grayscale images (e.g. roughness, metallic, ambient occlusion, masks, height maps) are stored with
		// one channel (two with alpha) instead of four: up to 4x less memory and bandwidth. The swizzle makes
		// them sample exactly like their RGBA version (gray, gray, gray, alpha).
		const bool grayscale = !p_hdr && p_sourceChannels == 1;
		const bool grayscaleAlpha = !p_hdr && p_sourceChannels == 2;

		const auto internalFormat =
			p_hdr ? EInternalFormat::RGBA32F :
			grayscale ? EInternalFormat::R8 :
			grayscaleAlpha ? EInternalFormat::RG8 :
			EInternalFormat::RGBA8;

		p_texture.Allocate({
			.width = p_width,
			.height = p_height,
			.minFilter = p_minFilter,
			.magFilter = p_magFilter,
			.horizontalWrap = p_horizontalWrapMode,
			.verticalWrap = p_verticalWrapMode,
			.internalFormat = internalFormat,
			.useMipMaps = p_generateMipmap
		});

		if (grayscale)
		{
			const auto packed = PackChannels(p_data, p_width, p_height, { 0 });
			p_texture.Upload(packed.data(), EFormat::RED, EPixelDataType::UNSIGNED_BYTE);
			p_texture.SetSwizzle(ETextureSwizzle::RED, ETextureSwizzle::RED, ETextureSwizzle::RED, ETextureSwizzle::ONE);
		}
		else if (grayscaleAlpha)
		{
			const auto packed = PackChannels(p_data, p_width, p_height, { 0, 3 });
			p_texture.Upload(packed.data(), EFormat::RG, EPixelDataType::UNSIGNED_BYTE);
			p_texture.SetSwizzle(ETextureSwizzle::RED, ETextureSwizzle::RED, ETextureSwizzle::RED, ETextureSwizzle::GREEN);
		}
		else
		{
			p_texture.Upload(p_data, EFormat::RGBA, p_hdr ? EPixelDataType::FLOAT : EPixelDataType::UNSIGNED_BYTE);
		}

		if (p_generateMipmap)
		{
			p_texture.GenerateMipmaps();
		}
	}
}

OvRendering::Resources::Texture* OvRendering::Resources::Loaders::TextureLoader::Create(
	const std::string& p_filepath,
	baregl::types::ETextureFilteringMode p_minFilter,
	baregl::types::ETextureFilteringMode p_magFilter,
	baregl::types::ETextureWrapMode p_horizontalWrapMode,
	baregl::types::ETextureWrapMode p_verticalWrapMode,
	bool p_generateMipmap
)
{
	if (Data::Image image{ p_filepath })
	{
		auto texture = std::make_unique<baregl::Texture>(
			baregl::types::ETextureType::TEXTURE_2D,
			OvTools::Utils::PathParser::GetElementName(p_filepath)
		);

		PrepareTexture(
			*texture,
			image.data,
			p_minFilter,
			p_magFilter,
			p_horizontalWrapMode,
			p_verticalWrapMode,
			image.width,
			image.height,
			p_generateMipmap,
			image.isHDR,
			image.bpp
		);

		return new Texture{ p_filepath, std::move(texture) };
	}

	return nullptr;
}

OvRendering::Resources::Texture* OvRendering::Resources::Loaders::TextureLoader::CreatePixel(
	uint8_t r,
	uint8_t g,
	uint8_t b,
	uint8_t a
)
{
	std::array<uint8_t, 4> colorData = { r, g, b, a };

	return OvRendering::Resources::Loaders::TextureLoader::CreateFromMemory(
		colorData.data(), 1, 1,
		baregl::types::ETextureFilteringMode::NEAREST,
		baregl::types::ETextureFilteringMode::NEAREST,
		baregl::types::ETextureWrapMode::REPEAT,
		baregl::types::ETextureWrapMode::REPEAT,
		false
	);
}

OvRendering::Resources::Texture* OvRendering::Resources::Loaders::TextureLoader::CreateFromMemory(
	const uint8_t* p_data,
	uint32_t p_width,
	uint32_t p_height,
	baregl::types::ETextureFilteringMode p_minFilter,
	baregl::types::ETextureFilteringMode p_magFilter,
	baregl::types::ETextureWrapMode p_horizontalWrapMode,
	baregl::types::ETextureWrapMode p_verticalWrapMode,
	bool p_generateMipmap
)
{
	auto texture = std::make_unique<baregl::Texture>(baregl::types::ETextureType::TEXTURE_2D, "FromMemory");

	PrepareTexture(
		*texture,
		p_data,
		p_minFilter,
		p_magFilter,
		p_horizontalWrapMode,
		p_verticalWrapMode,
		p_width,
		p_height,
		p_generateMipmap,
		false
	);

	return new Texture("", std::move(texture));
}

OvRendering::Resources::Texture* OvRendering::Resources::Loaders::TextureLoader::CreateFromEncodedMemory(
	const uint8_t* p_data,
	size_t p_size,
	baregl::types::ETextureFilteringMode p_minFilter,
	baregl::types::ETextureFilteringMode p_magFilter,
	baregl::types::ETextureWrapMode p_horizontalWrapMode,
	baregl::types::ETextureWrapMode p_verticalWrapMode,
	bool p_generateMipmap
)
{
	if (Data::Image image{ p_data, p_size })
	{
		auto texture = std::make_unique<baregl::Texture>(baregl::types::ETextureType::TEXTURE_2D, "FromEncodedMemory");

		PrepareTexture(
			*texture,
			image.data,
			p_minFilter,
			p_magFilter,
			p_horizontalWrapMode,
			p_verticalWrapMode,
			image.width,
			image.height,
			p_generateMipmap,
			image.isHDR,
			image.bpp
		);

		return new Texture("", std::move(texture));
	}

	return nullptr;
}

void OvRendering::Resources::Loaders::TextureLoader::Reload(
	Texture& p_texture,
	const std::string& p_filePath,
	baregl::types::ETextureFilteringMode p_minFilter,
	baregl::types::ETextureFilteringMode p_magFilter,
	baregl::types::ETextureWrapMode p_horizontalWrapMode,
	baregl::types::ETextureWrapMode p_verticalWrapMode,
	bool p_generateMipmap
)
{
	if (Data::Image image{ p_filePath })
	{
		auto texture = std::make_unique<baregl::Texture>(
			baregl::types::ETextureType::TEXTURE_2D,
			OvTools::Utils::PathParser::GetElementName(p_filePath)
		);

		PrepareTexture(
			*texture,
			image.data,
			p_minFilter,
			p_magFilter,
			p_horizontalWrapMode,
			p_verticalWrapMode,
			image.width,
			image.height,
			p_generateMipmap,
			image.isHDR,
			image.bpp
		);

		p_texture.SetTexture(std::move(texture));
	}
}

void OvRendering::Resources::Loaders::TextureLoader::ReloadFromMemory(
	Texture& p_texture,
	const uint8_t* p_data,
	uint32_t p_width,
	uint32_t p_height,
	baregl::types::ETextureFilteringMode p_minFilter,
	baregl::types::ETextureFilteringMode p_magFilter,
	baregl::types::ETextureWrapMode p_horizontalWrapMode,
	baregl::types::ETextureWrapMode p_verticalWrapMode,
	bool p_generateMipmap
)
{
	auto texture = std::make_unique<baregl::Texture>(baregl::types::ETextureType::TEXTURE_2D, "FromMemory");

	PrepareTexture(
		*texture,
		p_data,
		p_minFilter,
		p_magFilter,
		p_horizontalWrapMode,
		p_verticalWrapMode,
		p_width,
		p_height,
		p_generateMipmap,
		false
	);

	p_texture.SetTexture(std::move(texture));
}

void OvRendering::Resources::Loaders::TextureLoader::ReloadFromEncodedMemory(
	Texture& p_texture,
	const uint8_t* p_data,
	size_t p_size,
	baregl::types::ETextureFilteringMode p_minFilter,
	baregl::types::ETextureFilteringMode p_magFilter,
	baregl::types::ETextureWrapMode p_horizontalWrapMode,
	baregl::types::ETextureWrapMode p_verticalWrapMode,
	bool p_generateMipmap
)
{
	if (Data::Image image{ p_data, p_size })
	{
		auto texture = std::make_unique<baregl::Texture>(baregl::types::ETextureType::TEXTURE_2D, "FromEncodedMemory");

		PrepareTexture(
			*texture,
			image.data,
			p_minFilter,
			p_magFilter,
			p_horizontalWrapMode,
			p_verticalWrapMode,
			image.width,
			image.height,
			p_generateMipmap,
			image.isHDR,
			image.bpp
		);

		p_texture.SetTexture(std::move(texture));
	}
}

bool OvRendering::Resources::Loaders::TextureLoader::Destroy(Texture*& p_textureInstance)
{
	if (p_textureInstance)
	{
		delete p_textureInstance;
		p_textureInstance = nullptr;
		return true;
	}

	return false;
}
