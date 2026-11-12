/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <string>

#include <doctest/doctest.h>
#include <tinyxml2.h>

#include <OvCore/Resources/Material.h>
#include <OvRendering/Data/PipelineState.h>

namespace
{
	using OvRendering::Settings::EBlendMode;
	using baregl::types::EBlendingEquation;
	using baregl::types::EBlendingFactor;

	/**
	* The blend function of a pipeline state, read out of its bit-fields
	*/
	struct BlendFunction
	{
		EBlendingFactor source;
		EBlendingFactor destination;
		EBlendingEquation equation;
	};

	BlendFunction Read(const OvRendering::Data::PipelineState& p_state)
	{
		return { p_state.blendingSrcFactor, p_state.blendingDestFactor, p_state.blendingEquation };
	}

	/**
	* Saves a material's settings to a document, as a .ovmat holds them
	*/
	void Save(OvCore::Resources::Material& p_material, tinyxml2::XMLDocument& p_document)
	{
		auto* root = p_document.NewElement("root");
		p_document.InsertFirstChild(root);
		p_material.OnSerialize(p_document, root);
	}

	/**
	* Reads a material from a document holding the given settings, and no shader
	*/
	EBlendMode Load(const std::string& p_settings)
	{
		tinyxml2::XMLDocument document;
		REQUIRE(document.Parse(("<root><shader>?</shader><settings>" + p_settings + "</settings></root>").c_str()) == tinyxml2::XML_SUCCESS);

		OvCore::Resources::Material material;
		material.SetBlendMode(EBlendMode::PREMULTIPLIED); // Overwritten by whatever the file says, or by the default
		material.OnDeserialize(document, document.FirstChildElement("root"));
		return material.GetBlendMode();
	}
}

TEST_SUITE("Blend modes")
{
	TEST_CASE("each blend mode gives its blend function, and alpha keeps the pass's")
	{
		OvRendering::Data::PipelineState alpha;
		alpha.ApplyBlendMode(EBlendMode::ALPHA);
		CHECK(Read(alpha).source == EBlendingFactor::SRC_ALPHA);
		CHECK(Read(alpha).destination == EBlendingFactor::ONE_MINUS_SRC_ALPHA);
		CHECK(Read(alpha).equation == EBlendingEquation::FUNC_ADD);

		OvRendering::Data::PipelineState additive;
		additive.ApplyBlendMode(EBlendMode::ADDITIVE);
		CHECK(Read(additive).source == EBlendingFactor::SRC_ALPHA);
		CHECK(Read(additive).destination == EBlendingFactor::ONE);
		CHECK(Read(additive).equation == EBlendingEquation::FUNC_ADD);

		OvRendering::Data::PipelineState premultiplied;
		premultiplied.ApplyBlendMode(EBlendMode::PREMULTIPLIED);
		CHECK(Read(premultiplied).source == EBlendingFactor::ONE);
		CHECK(Read(premultiplied).destination == EBlendingFactor::ONE_MINUS_SRC_ALPHA);

		// A pass with its own blend function, as bloom has, keeps it for an alpha material
		OvRendering::Data::PipelineState accumulation;
		accumulation.blendingSrcFactor = EBlendingFactor::ONE;
		accumulation.blendingDestFactor = EBlendingFactor::ONE;
		accumulation.ApplyBlendMode(EBlendMode::ALPHA);
		CHECK(Read(accumulation).source == EBlendingFactor::ONE);
		CHECK(Read(accumulation).destination == EBlendingFactor::ONE);
	}

	TEST_CASE("a material carries its blend mode in its state mask, alpha by default")
	{
		OvCore::Resources::Material material;
		CHECK(material.GetBlendMode() == EBlendMode::ALPHA);
		CHECK(material.GenerateStateMask().blendMode == static_cast<uint8_t>(EBlendMode::ALPHA));

		material.SetBlendable(true);
		for (const auto mode : { EBlendMode::ADDITIVE, EBlendMode::PREMULTIPLIED, EBlendMode::ALPHA })
		{
			material.SetBlendMode(mode);
			const auto mask = material.GenerateStateMask();
			CHECK(static_cast<EBlendMode>(mask.blendMode) == mode);

			// The other settings are untouched by the two bits it takes
			CHECK(mask.blendable);
			CHECK(mask.depthTest);
			CHECK(mask.depthWriting);
			CHECK(mask.colorWriting);
			CHECK(mask.backfaceCulling);
			CHECK_FALSE(mask.frontfaceCulling);
		}
	}

	TEST_CASE("the blend mode is saved with the material, and a material saved before it blends as it did")
	{
		for (const auto mode : { EBlendMode::ALPHA, EBlendMode::ADDITIVE, EBlendMode::PREMULTIPLIED })
		{
			OvCore::Resources::Material material;
			material.SetBlendable(true);
			material.SetBlendMode(mode);

			tinyxml2::XMLDocument document;
			Save(material, document);
			REQUIRE(document.FirstChildElement("root")->FirstChildElement("settings")->FirstChildElement("blend_mode"));

			OvCore::Resources::Material other;
			other.OnDeserialize(document, document.FirstChildElement("root"));
			CHECK(other.GetBlendMode() == mode);
			CHECK(other.IsBlendable());
		}

		CHECK(Load("<blendable>true</blendable>") == EBlendMode::ALPHA);
		CHECK(Load("<blend_mode>1</blend_mode>") == EBlendMode::ADDITIVE);
		CHECK(Load("<blend_mode>2</blend_mode>") == EBlendMode::PREMULTIPLIED);

		// A value no blend mode has is read as alpha, with a warning
		CHECK(Load("<blend_mode>7</blend_mode>") == EBlendMode::ALPHA);
		CHECK(Load("<blend_mode>-1</blend_mode>") == EBlendMode::ALPHA);
	}
}
