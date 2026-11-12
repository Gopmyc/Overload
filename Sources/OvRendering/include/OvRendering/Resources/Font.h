/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <OvRendering/Data/Material.h>
#include <OvRendering/Resources/FontAtlas.h>

namespace OvRendering::Resources
{
	class Texture;

	/**
	* Font resource, with a glyph atlas for each pixel size it is drawn at. Basic Latin and Latin-1 are baked with an
	* atlas, any other character the font has when a text first needs it (EnsureGlyphs)
	*/
	class Font
	{
	public:
		// Its texture coordinates are in atlas pixels, which do not move when the atlas grows
		using Glyph = FontAtlas::Glyph;

		/**
		* Constructor
		* @param p_path
		* @param p_realPath
		*/
		Font(const std::string& p_path, const std::filesystem::path& p_realPath);

		/**
		* Destructor
		*/
		~Font();

		Font(const Font&) = delete;
		Font& operator=(const Font&) = delete;

		/**
		* Reloads the font from disk
		* @param p_realPath
		*/
		bool Reload(const std::filesystem::path& p_realPath);

		/**
		* Ensures a baked atlas variant exists for the given pixel size and makes it active
		* @param p_pixelSize
		*/
		bool SetActivePixelSize(float p_pixelSize);

		/**
		* Ensures a baked atlas variant exists for the given pixel size
		* @param p_pixelSize
		*/
		bool EnsurePixelSize(float p_pixelSize);

		/**
		* Returns true if the font has a valid atlas
		*/
		bool IsValid() const;

		/**
		* Returns the resource revision, incremented when atlas variants are rebuilt
		*/
		uint64_t GetRevision() const;

		/**
		* Returns the glyph atlas font size
		*/
		float GetPixelSize() const;

		/**
		* Returns the glyph atlas font size for the given requested pixel size
		*/
		float GetPixelSize(float p_pixelSize) const;

		/**
		* Returns the line height in atlas pixels
		*/
		float GetLineHeight() const;

		/**
		* Returns the line height in atlas pixels for the given requested pixel size
		*/
		float GetLineHeight(float p_pixelSize) const;

		/**
		* Returns the ascender in atlas pixels
		*/
		float GetAscender() const;

		/**
		* Returns the ascender in atlas pixels for the given requested pixel size
		* @param p_pixelSize
		*/
		float GetAscender(float p_pixelSize) const;

		/**
		* Returns the descender in atlas pixels
		*/
		float GetDescender() const;

		/**
		* Returns the descender in atlas pixels for the given requested pixel size
		* @param p_pixelSize
		*/
		float GetDescender(float p_pixelSize) const;

		/**
		* Bakes the characters the font has and the atlas of the given pixel size lacks, and uploads the atlas when it
		* changed. Returns false when the font has no atlas at that size
		* @param p_codePoints
		* @param p_pixelSize
		*/
		bool EnsureGlyphs(std::span<const uint32_t> p_codePoints, float p_pixelSize);

		/**
		* Returns the glyph associated with the given Unicode code point
		* @param p_codePoint
		*/
		const Glyph* GetGlyph(uint32_t p_codePoint) const;

		/**
		* Returns the glyph associated with the given Unicode code point and requested pixel size
		* @param p_codePoint
		* @param p_pixelSize
		*/
		const Glyph* GetGlyph(uint32_t p_codePoint, float p_pixelSize) const;

		/**
		* Returns the static glyph atlas texture
		*/
		Texture* GetAtlasTexture() const;

		/**
		* Returns the static glyph atlas texture for the given requested pixel size
		*/
		Texture* GetAtlasTexture(float p_pixelSize);

		/**
		* Initializes or refreshes the embedded material used for text rendering
		* @param p_shader
		*/
		bool EnsureEmbeddedMaterial(Shader* p_shader);

		/**
		* Initializes or refreshes the embedded material used for text rendering at the given pixel size
		* @param p_shader
		* @param p_pixelSize
		*/
		bool EnsureEmbeddedMaterial(Shader* p_shader, float p_pixelSize);

		/**
		* Returns the embedded material used for text rendering
		*/
		Data::Material* GetEmbeddedMaterial() const;

		/**
		* Returns the embedded material used for text rendering at the given pixel size
		*/
		Data::Material* GetEmbeddedMaterial(float p_pixelSize);

	public:
		const std::string path;

	private:
		struct AtlasVariant
		{
			bool valid = false;
			float pixelSize = 32.0f;
			float lineHeight = 32.0f;
			float ascender = 25.6f;
			float descender = -6.4f;
			std::unique_ptr<FontAtlas> atlas;
			Texture* atlasTexture = nullptr;
			std::unique_ptr<Data::Material> embeddedMaterial;
		};

		AtlasVariant* GetActiveVariant();
		const AtlasVariant* GetActiveVariant() const;
		AtlasVariant* GetVariant(uint32_t p_pixelSize);
		const AtlasVariant* GetVariant(uint32_t p_pixelSize) const;
		AtlasVariant* GetOrCreateVariant(uint32_t p_pixelSize);
		void DestroyAtlasVariants();
		bool CreateAtlasVariant(uint32_t p_pixelSize);
		bool UploadAtlas(AtlasVariant& p_variant);

	private:
		bool m_valid = false;
		uint32_t m_activePixelSize = 32;
		uint64_t m_revision = 0;
		std::filesystem::path m_realPath;
		std::shared_ptr<const std::vector<uint8_t>> m_fontData;
		std::unordered_map<uint32_t, AtlasVariant> m_atlasVariants;
	};
}
