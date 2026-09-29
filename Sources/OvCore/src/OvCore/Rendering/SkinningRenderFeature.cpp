/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <string>

#include <OvCore/Rendering/SkinningDrawableDescriptor.h>
#include <OvCore/Rendering/SkinningRenderFeature.h>
#include <OvCore/Rendering/SkinningUtils.h>

namespace
{
	const std::string kSkinningFeatureName{ OvCore::Rendering::SkinningUtils::kFeatureName };
}

OvCore::Rendering::SkinningRenderFeature::SkinningRenderFeature(
	OvRendering::Core::CompositeRenderer& p_renderer,
	OvRendering::Features::EFeatureExecutionPolicy p_executionPolicy,
	uint32_t p_bufferBindingPoint
) :
	ARenderFeature(p_renderer, p_executionPolicy),
	m_bufferBindingPoint(p_bufferBindingPoint)
{
	m_identityBuffer = std::make_unique<baregl::Buffer>();

	const auto identity = OvMaths::FMatrix4::Transpose(OvMaths::FMatrix4::Identity);
	m_identityBuffer->Allocate(sizeof(OvMaths::FMatrix4), baregl::types::EAccessSpecifier::STATIC_DRAW);
	m_identityBuffer->Upload(&identity);
}

uint32_t OvCore::Rendering::SkinningRenderFeature::GetBufferBindingPoint() const
{
	return m_bufferBindingPoint;
}

void OvCore::Rendering::SkinningRenderFeature::OnBeginFrame(const OvRendering::Data::FrameDescriptor& p_frameDescriptor)
{
	(void)p_frameDescriptor;

	m_skinningBuffer.BeginFrame();
	m_uploadedPalettes.clear();
	m_uploadedPalettesGeneration = m_skinningBuffer.GetGeneration();
	m_boundType = EBoundPalette::NONE;
}

void OvCore::Rendering::SkinningRenderFeature::OnEndFrame()
{
	m_skinningBuffer.EndFrame();
	m_boundType = EBoundPalette::NONE;
}

void OvCore::Rendering::SkinningRenderFeature::OnBeforeDraw(
	OvRendering::Data::PipelineState& p_pso,
	const OvRendering::Entities::Drawable& p_drawable
)
{
	(void)p_pso;

	OvTools::Utils::OptRef<const SkinningDrawableDescriptor> skinningDescriptor;
	const bool hasSkinningDescriptor = p_drawable.TryGetDescriptor<SkinningDrawableDescriptor>(skinningDescriptor);

	const bool usesSkinningVariant =
		(p_drawable.featureSetOverride.has_value() && p_drawable.featureSetOverride->contains(kSkinningFeatureName)) ||
		(p_drawable.material.has_value() && p_drawable.material->GetFeatures().contains(kSkinningFeatureName));

	if (!usesSkinningVariant)
	{
		return;
	}

	if (!hasSkinningDescriptor ||
		!skinningDescriptor->matrices ||
		skinningDescriptor->count == 0)
	{
		BindIdentityPalette();
		return;
	}

	const PaletteKey key{
		.ptr = skinningDescriptor->matrices,
		.count = skinningDescriptor->count,
		.poseVersion = skinningDescriptor->poseVersion
	};

	if (m_boundType == EBoundPalette::SKINNING && m_boundPalette == key)
	{
		return;
	}

	// Ranges recorded before the streaming buffer got replaced (grown) are not valid anymore
	if (m_uploadedPalettesGeneration != m_skinningBuffer.GetGeneration())
	{
		m_uploadedPalettes.clear();
		m_uploadedPalettesGeneration = m_skinningBuffer.GetGeneration();
	}

	auto uploaded = m_uploadedPalettes.find(key);

	if (uploaded == m_uploadedPalettes.end())
	{
		const auto range = m_skinningBuffer.Push(
			skinningDescriptor->matrices,
			static_cast<uint64_t>(skinningDescriptor->count) * sizeof(OvMaths::FMatrix4)
		);

		// Pushing may have replaced the buffer, invalidating the previously recorded ranges
		if (m_uploadedPalettesGeneration != m_skinningBuffer.GetGeneration())
		{
			m_uploadedPalettes.clear();
			m_uploadedPalettesGeneration = m_skinningBuffer.GetGeneration();
		}

		uploaded = m_uploadedPalettes.emplace(key, range).first;
	}

	m_skinningBuffer.GetBuffer().Bind(baregl::types::EBufferType::SHADER_STORAGE, m_bufferBindingPoint, uploaded->second);
	m_boundType = EBoundPalette::SKINNING;
	m_boundPalette = key;
}

void OvCore::Rendering::SkinningRenderFeature::BindIdentityPalette()
{
	if (m_boundType != EBoundPalette::IDENTITY)
	{
		m_identityBuffer->Bind(baregl::types::EBufferType::SHADER_STORAGE, m_bufferBindingPoint);
		m_boundType = EBoundPalette::IDENTITY;
	}
}
