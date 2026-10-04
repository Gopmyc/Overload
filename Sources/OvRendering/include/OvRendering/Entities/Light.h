/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstddef>
#include <vector>

#include <baregl/Framebuffer.h>

#include <OvMaths/FVector3.h>
#include <OvMaths/FMatrix4.h>
#include <OvMaths/FTransform.h>

#include <OvRendering/Data/FrameDescriptor.h>
#include <OvRendering/Entities/Camera.h>
#include <OvRendering/Entities/Entity.h>
#include <OvRendering/Resources/Texture.h>
#include <OvRendering/Settings/ELightType.h>
#include <OvRendering/Settings/EShadowMapUpdateMode.h>

namespace OvRendering::Entities
{
	/**
	* Data structure that can represent any type of light
	*/
	struct Light : public Entity
	{
		OvMaths::FVector3 color{ 1.f, 1.f, 1.f };
		float intensity = 1.f;
		float constant = 0.0f;
		float linear = 0.0f;
		float quadratic = 1.0f;
		float cutoff = 12.f;
		float outerCutoff = 15.f;
		Settings::ELightType type = Settings::ELightType::POINT;

		bool castShadows = false;
		float shadowAreaSize = 50.0f;
		bool shadowFollowCamera = true;
		int16_t shadowMapResolution = 2048;
		Settings::EShadowMapUpdateMode shadowMapUpdateMode = Settings::EShadowMapUpdateMode::ON_CHANGE;

		std::unique_ptr<baregl::Framebuffer> shadowBuffer;
		std::optional<OvRendering::Entities::Camera> shadowCamera;
		std::optional<OvMaths::FMatrix4> lightSpaceMatrix;

		// Description of everything the current content of the shadow map has been rendered from (empty if invalid).
		// Stored with the shadow map, as it can be rendered by several renderers (e.g. editor views).
		std::vector<std::byte> shadowMapSignature;

		/**
		* Forces the shadow map to be rendered again the next time it is used
		*/
		void InvalidateShadowMap();

		/**
		* Generate and cache light space matrix for the light
		*/
		void PrepareForShadowRendering(const OvRendering::Data::FrameDescriptor& p_frameDescriptor);

		/**
		* Returns true if the light is setup for shadow rendering
		*/
		bool IsSetupForShadowRendering() const;

		/**
		* Generate the light matrix, ready to send to the GPU
		*/
		OvMaths::FMatrix4 GenerateMatrix() const;

		/**
		* Calculate the light effect range from the quadratic falloff equation
		*/
		float CalculateEffectRange() const;
	};
}
