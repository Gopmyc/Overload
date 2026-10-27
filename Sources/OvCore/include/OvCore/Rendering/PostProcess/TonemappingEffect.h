/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <chrono>

#include <OvCore/Rendering/PostProcess/AEffect.h>
#include <OvRendering/Data/Material.h>

namespace OvCore::Rendering::PostProcess
{
	/**
	* Enum of available tonemapping modes
	*/
	enum class ETonemappingMode
	{
		NEUTRAL = 0,
		REINHARD = 1,
		REINHARD_JODIE = 2,
		UNCHARTED2 = 3,
		UNCHARTED2_FILMIC = 4,
		ACES = 5
	};

	/**
	* Tonemapping effect settings, with the colour grading and the vignette applied in the same pass.
	* The defaults leave the image as the tonemapping curve makes it
	*/
	struct TonemappingSettings : public EffectSettings
	{
		float exposure = 1.0f;
		ETonemappingMode mode = ETonemappingMode::NEUTRAL;
		bool gammaCorrection = true;

		float temperature = 0.0f; // White balance, -1 (cool, blue) to 1 (warm, orange), before the curve
		float tint = 0.0f; // White balance, -1 (green) to 1 (magenta), before the curve
		float saturation = 1.0f; // 0 is greyscale, after the curve
		float contrast = 1.0f; // Around middle grey, after the curve
		float vignetteIntensity = 0.0f; // How much the corners darken, 0 to 1
		float vignetteSmoothness = 0.5f; // How far towards the centre the darkening reaches, 0 to 1
	};

	/**
	* Post-processing effect that applies Tonemapping
	* Remap HDR values to the LDR range.
	*/
	class TonemappingEffect : public AEffect
	{
	public:
		/**
		* Constructor of the TonemappingEffect class
		* @param p_renderer
		*/
		TonemappingEffect(OvRendering::Core::CompositeRenderer& p_renderer);

		/**
		* Render the Tonemapping effect
		* @note: make sure the effect is applicable before calling this method
		* @param p_pso
		* @param p_src
		* @param p_dst
		* @param p_settings
		*/
		virtual void Draw(
			OvRendering::Data::PipelineState p_pso,
			baregl::Framebuffer& p_src,
			baregl::Framebuffer& p_dst,
			const EffectSettings& p_settings
		) override;

	private:
		OvRendering::Data::Material m_tonemappingMaterial;
	};
}
