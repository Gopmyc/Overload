/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <OvCore/ECS/Components/UI/TextLayoutEngine.h>
#include <OvRendering/Resources/FontAtlas.h>
#include <OvTests/TestUtils.h>

using OvCore::ECS::Components::UI::TextLayoutEngine;
using OvRendering::Resources::FontAtlas;

namespace
{
	std::shared_ptr<const std::vector<uint8_t>> ReadEngineFont()
	{
		const auto resources = OvTests::FindEngineResources();
		REQUIRE_MESSAGE(!resources.empty(), "Resources/Engine not found above the test program or the current folder");

		std::ifstream file(resources / "Fonts" / "Roboto-Regular.ttf", std::ios::binary);
		REQUIRE(file);
		return std::make_shared<const std::vector<uint8_t>>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}

	std::unique_ptr<FontAtlas> MakeAtlas(uint32_t p_pixelSize)
	{
		std::string error;
		auto atlas = FontAtlas::Create(ReadEngineFont(), p_pixelSize, error);
		REQUIRE_MESSAGE(atlas, error);
		return atlas;
	}

	/**
	* The atlas's alpha inside a glyph's rectangle: some coverage means it was drawn there
	*/
	bool HasCoverage(const FontAtlas& p_atlas, const FontAtlas::Glyph& p_glyph)
	{
		for (auto y = static_cast<uint32_t>(p_glyph.vMin); y < static_cast<uint32_t>(p_glyph.vMax); ++y)
		{
			for (auto x = static_cast<uint32_t>(p_glyph.uMin); x < static_cast<uint32_t>(p_glyph.uMax); ++x)
			{
				if (p_atlas.GetPixels()[static_cast<size_t>(y) * p_atlas.GetSize() + x] > 0)
				{
					return true;
				}
			}
		}

		return false;
	}

	std::vector<uint8_t> GlyphPixels(const FontAtlas& p_atlas, const FontAtlas::Glyph& p_glyph)
	{
		std::vector<uint8_t> pixels;

		for (auto y = static_cast<uint32_t>(p_glyph.vMin); y < static_cast<uint32_t>(p_glyph.vMax); ++y)
		{
			for (auto x = static_cast<uint32_t>(p_glyph.uMin); x < static_cast<uint32_t>(p_glyph.uMax); ++x)
			{
				pixels.push_back(p_atlas.GetPixels()[static_cast<size_t>(y) * p_atlas.GetSize() + x]);
			}
		}

		return pixels;
	}
}

TEST_SUITE("Fonts")
{
	TEST_CASE("an atlas starts with Basic Latin and Latin-1, and its metrics")
	{
		const auto atlas = MakeAtlas(32);

		CHECK(atlas->GetPixelSize() == 32.0f);
		CHECK(atlas->GetSize() == FontAtlas::kMinimumSize);
		CHECK(atlas->GetPixels().size() == static_cast<size_t>(atlas->GetSize()) * atlas->GetSize());
		CHECK(atlas->GetLineHeight() > 0.0f);
		CHECK(atlas->GetAscender() > 0.0f);
		CHECK(atlas->GetDescender() < 0.0f);

		for (const uint32_t codePoint : { 0x41u, 0x7Au, 0x3Fu, 0xE9u, 0xFFu })
		{
			const auto* glyph = atlas->GetGlyph(codePoint);
			REQUIRE_MESSAGE(glyph, "character " << codePoint);
			CHECK(glyph->valid);
			CHECK(glyph->xAdvance > 0.0f);
			CHECK(HasCoverage(*atlas, *glyph));
		}

		// A space advances without drawing
		const auto* space = atlas->GetGlyph(0x20);
		REQUIRE(space);
		CHECK(space->xAdvance > 0.0f);
		CHECK(space->width == 0.0f);

		// Not baked yet
		CHECK(atlas->GetGlyph(0x416) == nullptr);
	}

	TEST_CASE("characters beyond Latin-1 are baked when asked for, once")
	{
		const auto atlas = MakeAtlas(32);
		const std::vector<uint32_t> text = { 0x416, 0x3A9, 0x142, 0x416 }; // Cyrillic Zhe, Greek Omega, Polish l with stroke, Zhe again

		CHECK(atlas->Bake(text));

		for (const uint32_t codePoint : { 0x416u, 0x3A9u, 0x142u })
		{
			const auto* glyph = atlas->GetGlyph(codePoint);
			REQUIRE_MESSAGE(glyph, "character " << codePoint);
			CHECK(glyph->width > 0.0f);
			CHECK(HasCoverage(*atlas, *glyph));
		}

		// Baked already: nothing changes
		CHECK_FALSE(atlas->Bake(text));
	}

	TEST_CASE("a character the font has no glyph for has none, and is not looked for again")
	{
		const auto atlas = MakeAtlas(32);
		const std::vector<uint32_t> missing = { 0x4E2D, 0x1F600 }; // A CJK ideograph and an emoji, which Roboto lacks

		CHECK_FALSE(atlas->Bake(missing));
		CHECK(atlas->GetGlyph(0x4E2D) == nullptr);
		CHECK(atlas->GetGlyph(0x1F600) == nullptr);
		CHECK_FALSE(atlas->Bake(missing));
	}

	TEST_CASE("the atlas grows when full, and glyphs baked before keep their place")
	{
		const auto atlas = MakeAtlas(128);
		const uint32_t sizeBefore = atlas->GetSize();

		const auto* a = atlas->GetGlyph(0x41);
		REQUIRE(a);
		const FontAtlas::Glyph before = *a;
		const auto pixelsBefore = GlyphPixels(*atlas, before);

		// Latin Extended, Greek and Cyrillic, one at a time, until the atlas has to grow
		std::vector<uint32_t> text;
		for (uint32_t codePoint = 0x0100; codePoint <= 0x052F && atlas->GetSize() == sizeBefore; ++codePoint)
		{
			const uint32_t one[] = { codePoint };
			atlas->Bake(one);
			text.push_back(codePoint);
		}

		REQUIRE(atlas->GetSize() > sizeBefore);
		CHECK(atlas->GetSize() <= FontAtlas::kMaximumSize);

		const auto* after = atlas->GetGlyph(0x41);
		REQUIRE(after);
		CHECK(after->uMin == before.uMin);
		CHECK(after->vMin == before.vMin);
		CHECK(after->uMax == before.uMax);
		CHECK(after->vMax == before.vMax);
		CHECK(GlyphPixels(*atlas, *after) == pixelsBefore);

		// Every glyph lies inside the atlas, and none overlaps another
		std::vector<const FontAtlas::Glyph*> placed;
		for (const uint32_t codePoint : text)
		{
			const auto* glyph = atlas->GetGlyph(codePoint);
			if (!glyph || glyph->width == 0.0f)
			{
				continue;
			}

			CHECK(glyph->uMax <= static_cast<float>(atlas->GetSize()));
			CHECK(glyph->vMax <= static_cast<float>(atlas->GetSize()));

			for (const auto* other : placed)
			{
				const bool apart = glyph->uMax <= other->uMin || other->uMax <= glyph->uMin || glyph->vMax <= other->vMin || other->vMax <= glyph->vMin;
				CHECK(apart);
			}

			placed.push_back(glyph);
		}

		CHECK(placed.size() > 100);
	}

	TEST_CASE("UTF-8 text is read into characters, a broken sequence as U+FFFD")
	{
		CHECK(TextLayoutEngine::DecodeUTF8("A\xC3\xA9") == std::vector<uint32_t>{ 0x41, 0xE9 });
		CHECK(TextLayoutEngine::DecodeUTF8("\xD0\x96\xD0\xB8") == std::vector<uint32_t>{ 0x416, 0x438 });
		CHECK(TextLayoutEngine::DecodeUTF8("\xF0\x9F\x98\x80") == std::vector<uint32_t>{ 0x1F600 });

		// A lone continuation byte, a truncated sequence, an overlong form and a surrogate
		CHECK(TextLayoutEngine::DecodeUTF8("a\x80" "b") == std::vector<uint32_t>{ 0x61, 0xFFFD, 0x62 });
		CHECK(TextLayoutEngine::DecodeUTF8("\xE2\x82") == std::vector<uint32_t>{ 0xFFFD, 0xFFFD });
		CHECK(TextLayoutEngine::DecodeUTF8("\xC0\xAF") == std::vector<uint32_t>{ 0xFFFD, 0xFFFD });
		CHECK(TextLayoutEngine::DecodeUTF8("\xED\xA0\x80") == std::vector<uint32_t>{ 0xFFFD, 0xFFFD, 0xFFFD });
	}

	TEST_CASE("a font that is no font fails with why")
	{
		std::string error;
		CHECK_FALSE(FontAtlas::Create(nullptr, 32, error));
		CHECK(error == "no font data");

		const auto garbage = std::make_shared<const std::vector<uint8_t>>(std::vector<uint8_t>{ 1, 2, 3, 4 });
		CHECK_FALSE(FontAtlas::Create(garbage, 32, error));
		CHECK(error == "unable to read the font");
	}
}
