/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cstring>

#include <tracy/Tracy.hpp>

#include <OvCore/Rendering/EngineBufferRenderFeature.h>
#include <OvCore/Rendering/EngineDrawableDescriptor.h>
#include <OvRendering/Core/CompositeRenderer.h>

namespace
{
	constexpr uint32_t kEngineUBOBindingPoint = 0;

	/**
	* Returns the matrix to upload so that GLSL reads a normal matrix for the given model matrix.
	* The normal matrix only matters up to a positive scale (normals are normalized), so the adjugate
	* of the upper 3x3 (det * inverse) is used: unlike an actual inverse, it needs no division and
	* stays accurate for tiny or huge scales. It is signed by the determinant to keep normals
	* oriented for mirrored transforms. Matrices are uploaded transposed (row-major to column-major),
	* so uploading adjugate(M) makes GLSL read transpose(adjugate(M)) ~ transpose(inverse(M)).
	*/
	OvMaths::FMatrix4 CalculateNormalMatrix(const OvMaths::FMatrix4& p_modelMatrix)
	{
		const float* m = p_modelMatrix.data;

		// Cofactors of the upper 3x3
		const float c00 = m[5] * m[10] - m[6] * m[9];
		const float c01 = m[6] * m[8] - m[4] * m[10];
		const float c02 = m[4] * m[9] - m[5] * m[8];
		const float c10 = m[2] * m[9] - m[1] * m[10];
		const float c11 = m[0] * m[10] - m[2] * m[8];
		const float c12 = m[1] * m[8] - m[0] * m[9];
		const float c20 = m[1] * m[6] - m[2] * m[5];
		const float c21 = m[2] * m[4] - m[0] * m[6];
		const float c22 = m[0] * m[5] - m[1] * m[4];

		const float determinant = m[0] * c00 + m[1] * c01 + m[2] * c02;
		const float sign = determinant < 0.0f ? -1.0f : 1.0f;

		// Adjugate = transposed cofactor matrix
		OvMaths::FMatrix4 result = OvMaths::FMatrix4::Identity;
		result.data[0] = sign * c00; result.data[1] = sign * c10; result.data[2] = sign * c20;
		result.data[4] = sign * c01; result.data[5] = sign * c11; result.data[6] = sign * c21;
		result.data[8] = sign * c02; result.data[9] = sign * c12; result.data[10] = sign * c22;
		return result;
	}
}

OvCore::Rendering::EngineBufferRenderFeature::EngineBufferRenderFeature(
	OvRendering::Core::CompositeRenderer& p_renderer,
	OvRendering::Features::EFeatureExecutionPolicy p_executionPolicy
) : 
	ARenderFeature(p_renderer, p_executionPolicy)
{
	static_assert(sizeof(EngineUBO) == 416, "EngineUBO must match the std140 layout of EngineUBO.ovfxh");
	m_startTime = std::chrono::high_resolution_clock::now();
}

void OvCore::Rendering::EngineBufferRenderFeature::SetCamera(const OvRendering::Entities::Camera& p_camera)
{
	m_frameCamera = {
		.view = p_camera.GetViewMatrix(),
		.projection = p_camera.GetProjectionMatrix(),
		.position = p_camera.GetPosition()
	};

	ApplyCamera(m_frameCamera);
	m_cameraOverrideActive = false;
}

void OvCore::Rendering::EngineBufferRenderFeature::ApplyCamera(const CameraData& p_camera)
{
	m_data.view = OvMaths::FMatrix4::Transpose(p_camera.view);
	m_data.projection = OvMaths::FMatrix4::Transpose(p_camera.projection);
	m_data.viewProjection = OvMaths::FMatrix4::Transpose(p_camera.projection * p_camera.view);
	m_data.viewPos = p_camera.position;
	m_dirty = true;
}

void OvCore::Rendering::EngineBufferRenderFeature::SetDepthOnly(bool p_depthOnly)
{
	m_data.depthOnly = p_depthOnly ? 1 : 0;
	m_dirty = true;
}

void OvCore::Rendering::EngineBufferRenderFeature::OnBeginFrame(const OvRendering::Data::FrameDescriptor& p_frameDescriptor)
{
	OVASSERT(p_frameDescriptor.camera.has_value(), "Camera is not set in the frame descriptor");

	m_engineBuffer.BeginFrame();

	auto currentTime = std::chrono::high_resolution_clock::now();
	auto elapsedTime = std::chrono::duration_cast<std::chrono::duration<float>>(currentTime - m_startTime);

	SetCamera(p_frameDescriptor.camera.value());
	m_data.time = elapsedTime.count();
}

void OvCore::Rendering::EngineBufferRenderFeature::OnEndFrame()
{
	m_cameraOverrideActive = false;
	m_engineBuffer.EndFrame();
}

void OvCore::Rendering::EngineBufferRenderFeature::OnBeforeDraw(OvRendering::Data::PipelineState& p_pso, const OvRendering::Entities::Drawable& p_drawable)
{
	ZoneScoped;

	OvTools::Utils::OptRef<const EngineDrawableDescriptor> descriptor;
	const bool hasDescriptor = p_drawable.TryGetDescriptor<EngineDrawableDescriptor>(descriptor);

	// Some drawables (e.g. screen space UI) are drawn with their own camera matrices,
	// the camera of the frame is restored for the next drawables.
	if (hasDescriptor && (descriptor->viewMatrixOverride || descriptor->projectionMatrixOverride))
	{
		ApplyCamera({
			.view = descriptor->viewMatrixOverride.value_or(m_frameCamera.view),
			.projection = descriptor->projectionMatrixOverride.value_or(m_frameCamera.projection),
			.position = descriptor->viewMatrixOverride ? OvMaths::FVector3::Zero : m_frameCamera.position
		});
		m_cameraOverrideActive = true;
	}
	else if (m_cameraOverrideActive)
	{
		ApplyCamera(m_frameCamera);
		m_cameraOverrideActive = false;
	}

	if (hasDescriptor)
	{
		const auto modelMatrix = OvMaths::FMatrix4::Transpose(descriptor->modelMatrix);

		// Consecutive draws often share the same matrices (e.g. meshes of the same model):
		// in that case, the currently bound data can be reused as is.
		if (m_dirty ||
			std::memcmp(&modelMatrix, &m_data.model, sizeof(modelMatrix)) != 0 ||
			std::memcmp(&descriptor->userMatrix, &m_data.userMatrix, sizeof(m_data.userMatrix)) != 0)
		{
			m_data.model = modelMatrix;
			m_data.normalMatrix = CalculateNormalMatrix(descriptor->modelMatrix);
			m_data.userMatrix = descriptor->userMatrix;
			m_dirty = true;
		}
	}

	// Drawables without descriptor keep the previous model/user matrices (only the camera data may have changed)
	if (m_dirty)
	{
		const auto range = m_engineBuffer.Push(&m_data, sizeof(m_data));
		m_engineBuffer.GetBuffer().Bind(baregl::types::EBufferType::UNIFORM, kEngineUBOBindingPoint, range);
		m_dirty = false;
	}
}
