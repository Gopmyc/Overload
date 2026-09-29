/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include <unordered_map>

#include <OvMaths/FMatrix4.h>
#include <OvRendering/Features/ARenderFeature.h>
#include <OvRendering/Utils/StreamingBuffer.h>
#include <baregl/Buffer.h>

namespace OvCore::Rendering
{
	/**
	* Render feature responsible for uploading and binding skinning matrices.
	*/
	class SkinningRenderFeature : public OvRendering::Features::ARenderFeature
	{
	public:
		static constexpr uint32_t kDefaultBufferBindingPoint = 1;

		/**
		* Constructor
		* @param p_renderer
		* @param p_executionPolicy
		* @param p_bufferBindingPoint
		*/
		SkinningRenderFeature(
			OvRendering::Core::CompositeRenderer& p_renderer,
			OvRendering::Features::EFeatureExecutionPolicy p_executionPolicy,
			uint32_t p_bufferBindingPoint = kDefaultBufferBindingPoint
		);

		/**
		* Returns the skinning buffer binding point
		*/
		uint32_t GetBufferBindingPoint() const;

	protected:
		virtual void OnBeginFrame(const OvRendering::Data::FrameDescriptor& p_frameDescriptor) override;
		virtual void OnEndFrame() override;
		virtual void OnBeforeDraw(OvRendering::Data::PipelineState& p_pso, const OvRendering::Entities::Drawable& p_drawable) override;

	private:
		void BindIdentityPalette();

	private:
		enum class EBoundPalette
		{
			NONE,
			IDENTITY,
			SKINNING
		};

		struct PaletteKey
		{
			const OvMaths::FMatrix4* ptr = nullptr;
			uint32_t count = 0;
			uint64_t poseVersion = 0;

			bool operator==(const PaletteKey&) const = default;
		};

		struct PaletteKeyHash
		{
			size_t operator()(const PaletteKey& p_key) const
			{
				return std::hash<const void*>{}(p_key.ptr) ^ (std::hash<uint64_t>{}(p_key.poseVersion) << 1) ^ p_key.count;
			}
		};

		uint32_t m_bufferBindingPoint;

		// Palettes of the frame are appended to a streaming buffer (each one at its own offset),
		// so uploading a palette never overwrites data still in use by previous draws.
		OvRendering::Utils::StreamingBuffer m_skinningBuffer;
		std::unique_ptr<baregl::Buffer> m_identityBuffer;

		// Palettes already uploaded this frame (shared between passes and meshes of the same skinned model)
		std::unordered_map<PaletteKey, baregl::data::BufferMemoryRange, PaletteKeyHash> m_uploadedPalettes;
		uint64_t m_uploadedPalettesGeneration = 0;

		EBoundPalette m_boundType = EBoundPalette::NONE;
		PaletteKey m_boundPalette;
	};
}
