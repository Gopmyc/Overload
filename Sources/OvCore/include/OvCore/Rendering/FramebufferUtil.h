/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>

#include <baregl/Framebuffer.h>
#include <baregl/types/EInternalFormat.h>

namespace OvCore::Rendering::FramebufferUtil
{
	/**
	* Prepare the given framebuffer for rendering by attaching
	* the necessary textures and renderbuffers.
	* @note This overload gives more control over the texture desc.
	* @param p_framebuffer
	* @param p_textureDesc
	* @param p_useDepth
	* @param p_useStencil
	*/
	void SetupFramebuffer(
		baregl::Framebuffer& p_framebuffer,
		const baregl::data::TextureDesc& p_textureDesc,
		bool p_useDepth = true,
		bool p_useStencil = false
	);

	/**
	* Prepare the given framebuffer for rendering by attaching
	* the necessary textures and renderbuffers
	* @param p_framebuffer
	* @param p_width
	* @param p_height
	* @param p_useDepth
	* @param p_useStencil
	* @param p_useMipMaps
	* @param p_internalFormat Format of the color attachment. Half-float is enough for HDR rendering
	*	and uses half the memory bandwidth of a 32-bit float format.
	*/
	void SetupFramebuffer(
		baregl::Framebuffer& p_framebuffer,
		uint32_t p_width = 0,
		uint32_t p_height = 0,
		bool p_useDepth = true,
		bool p_useStencil = false,
		bool p_useMipMaps = false,
		baregl::types::EInternalFormat p_internalFormat = baregl::types::EInternalFormat::RGBA16F
	);
}
