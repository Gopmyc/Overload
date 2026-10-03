/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <format>
#include <ranges>

#include <baregl/Buffer.h>
#include <baregl/Texture.h>
#include <baregl/data/UniformInfo.h>
#include <baregl/math/Vec2.h>
#include <baregl/math/Vec3.h>
#include <baregl/math/Vec4.h>
#include <baregl/math/Mat3.h>
#include <baregl/math/Mat4.h>
#include <tracy/Tracy.hpp>

#include <OvDebug/Assertion.h>
#include <OvDebug/Logger.h>

#include <OvRendering/Data/Material.h>
#include <OvRendering/Resources/Texture.h>

#include <OvTools/Utils/OptRef.h>

namespace
{
	template<typename T>
	inline void HashCombine(std::size_t& seed, const T& value)
	{
		// A large odd constant derived from the golden ratio (64-bit variant).
		// It helps distribute bits more uniformly and reduce clustering/collisions.
		constexpr std::size_t kHashCombineConstant = 0x9e3779b97f4a7c15ULL;

		seed ^= std::hash<T>{}(value)
			+ kHashCombineConstant 
			// Bit mixing. They spread entropy from earlier values so order matters
			// and nearby hashes don't collapse into similar outputs.
			+ (seed << 6)
			+ (seed >> 2);
	}

	OvRendering::Data::MaterialPropertyType UniformToPropertyValue(const std::any& p_uniformValue)
	{
		ZoneScoped;

		using namespace OvMaths;
		using namespace OvRendering;
		using namespace baregl::math;

		auto as = [&]<typename T>() -> std::optional<T> {
			return
				p_uniformValue.type() == typeid(T) ?
				std::optional<T>{std::any_cast<T>(p_uniformValue)} :
				std::nullopt;
		};

		if (auto value = as.operator()<bool>()) return *value;
		if (auto value = as.operator()<int>()) return *value;
		if (auto value = as.operator()<float>()) return *value;
		if (auto value = as.operator()<Vec2>()) return (FVector2&)*value;
		if (auto value = as.operator()<Vec3>()) return (FVector3&)*value;
		if (auto value = as.operator()<Vec4>()) return (FVector4&)*value;
		if (auto value = as.operator()<Mat3>()) return (FMatrix3&)*value;
		if (auto value = as.operator()<Mat4>()) return (FMatrix4&)*value;
		if (auto value = as.operator()<baregl::Texture*>())
		{
			// If the baregl::Texture has a value use it as is
			if (*value) return *value;

			// If it's nullptr, default to an empty OvRendering::Resources::Texture*
			return static_cast<Resources::Texture*>(nullptr);
		}
		if (auto value = as.operator()<Resources::Texture*>()) return *value;

		return std::monostate{};
	}

	void BindTexture(
		baregl::ShaderProgram& p_shader,
		uint32_t p_uniformLocation,
		baregl::Texture* p_texture,
		baregl::Texture* p_fallback,
		uint32_t p_textureSlot
	)
	{
		if (auto target = p_texture ? p_texture : p_fallback)
		{
			target->Bind(p_textureSlot);
			p_shader.SetUniformAtLocation<int>(p_uniformLocation, p_textureSlot);
		}
	}
}

OvRendering::Data::Material::Material(OvRendering::Resources::Shader* p_shader)
{
	SetShader(p_shader);
}

void OvRendering::Data::Material::SetShader(OvRendering::Resources::Shader* p_shader)
{
	m_shader = p_shader;

	if (m_shader)
	{
		m_properties.clear();
		UpdateProperties();
	}
	else
	{
		m_properties.clear();
		++m_propertiesLayoutVersion;
		InvalidatePropertySignature();
	}
}

OvTools::Utils::OptRef<baregl::ShaderProgram> OvRendering::Data::Material::GetVariant(
	std::optional<const std::string_view> p_pass,
	OvTools::Utils::OptRef<const Data::FeatureSet> p_override
) const
{
	if (m_shader)
	{
		return m_shader->GetVariant(
			p_pass,
			p_override.value_or(m_features)
		);
	}
	
	return std::nullopt;
}

void OvRendering::Data::Material::UpdateProperties()
{
	InvalidatePropertySignature();
	++m_propertiesLayoutVersion;

	// Collect all uniform names currently used by the shader
	std::unordered_set<std::string> usedUniforms;

	auto variants_view = m_shader->GetVariants()
		| std::views::values
		| std::views::join
		| std::views::values;

	for (const auto& variant : variants_view)
	{
		for (const auto& [name, uniformInfo] : variant->GetUniforms())
		{
			usedUniforms.insert(name);

			if (!m_properties.contains(name))
			{
				const auto defaultValue = UniformToPropertyValue(uniformInfo.defaultValue);
				m_properties.emplace(name, MaterialProperty{
					.value = defaultValue,
					.defaultValue = defaultValue,
					.singleUse = false
				});
			}
		}
	}

	std::erase_if(m_properties, [&usedUniforms](const auto& property) {
		return !usedUniforms.contains(property.first);
	});
}

OvRendering::Data::MaterialSignatureSet OvRendering::Data::Material::Bind(
	std::optional<const std::string_view> p_pass,
	OvTools::Utils::OptRef<const Data::FeatureSet> p_featureSetOverride,
	baregl::Texture* p_emptyTexture2D,
	baregl::Texture* p_emptyTextureCube,
	std::optional<MaterialSignatureSet> p_previousMaterialSignature
)
{
	ZoneScoped;

	OVASSERT(IsValid(), "Attempting to bind an invalid material.");

	auto& program =
		p_featureSetOverride ?
		m_shader->GetVariant(p_pass, p_featureSetOverride.value()) :
		GetCachedVariant(p_pass);

	const auto signature = CalculateSignature(
		program,
		p_emptyTexture2D,
		p_emptyTextureCube
	);

	if (!p_previousMaterialSignature || signature.bindSignature != p_previousMaterialSignature->bindSignature)
	{
		program.Bind();
	}

	m_programInUse = program;

	return signature;
}

void OvRendering::Data::Material::UploadProperties(
	bool uploadStableProperties,
	bool uploadSingleUseProperties,
	baregl::Texture* p_emptyTexture2D,
	baregl::Texture* p_emptyTextureCube
)
{
	ZoneScoped;

	OVASSERT(m_programInUse.has_value(), "Cannot upload properties before binding");

	using namespace OvMaths;
	using namespace baregl::math;
	using enum baregl::types::EUniformType;

	auto& program = m_programInUse.value();

	// Only the properties mapping to a uniform of the current program are uploaded.
	// This list is cached, so we don't have to look up each property by name, for each draw.
	UpdateUniformBindings(program);

	for (const auto& binding : m_uniformBindings)
	{
		auto& prop = *binding.property;
		const auto& uniformInfo = *binding.uniform;

		if (!uploadStableProperties && !prop.singleUse) continue;
		if (!uploadSingleUseProperties && prop.singleUse) continue;

		const uint32_t location = uniformInfo.location;
		auto& value = prop.value;
		auto uniformType = uniformInfo.type;

		// Iterating over the properties to set them in the shader.
		// This could have been cleaner with a visitor, but the performance impact
		// is not worth it. This is a critical path in the rendering pipeline.

		if (uniformType == BOOL)
		{
			program.SetUniformAtLocation<int>(location, static_cast<int>(std::get<bool>(value)));
		}
		else if (uniformType == INT)
		{
			program.SetUniformAtLocation<int>(location, std::get<int>(value));
		}
		else if (uniformType == FLOAT)
		{
			program.SetUniformAtLocation<float>(location, std::get<float>(value));
		}
		else if (uniformType == FLOAT_VEC2)
		{
			program.SetUniformAtLocation<Vec2>(location, (Vec2&)(std::get<FVector2>(value)));
		}
		else if (uniformType == FLOAT_VEC3)
		{
			program.SetUniformAtLocation<Vec3>(location, (Vec3&)std::get<FVector3>(value));
		}
		else if (uniformType == FLOAT_VEC4)
		{
			program.SetUniformAtLocation<Vec4>(location, (Vec4&)std::get<FVector4>(value));
		}
		else if (uniformType == FLOAT_MAT3)
		{
			const auto t = FMatrix3::Transpose(std::get<FMatrix3>(value));
			program.SetUniformAtLocation<Mat3>(location, (Mat3&)t);
		}
		else if (uniformType == FLOAT_MAT4)
		{
			const auto t = FMatrix4::Transpose(std::get<FMatrix4>(value));
			program.SetUniformAtLocation<Mat4>(location, (Mat4&)t);
		}
		else if (uniformType == SAMPLER_2D || uniformType == SAMPLER_CUBE)
		{
			baregl::Texture* handle = nullptr;
			if (auto textureHandle = std::get_if<baregl::Texture*>(&value))
			{
				handle = *textureHandle;
			}
			else if (auto texture = std::get_if<Resources::Texture*>(&value))
			{
				if (*texture != nullptr)
				{
					handle = &(*texture)->GetTexture();
				}
			}
			
			const auto textureIndex = uniformInfo.textureIndex;

			OVASSERT(textureIndex.has_value(), std::format("No texture index found for uniform: {}", uniformInfo.name));

			BindTexture(
				program,
				location,
				handle,
				uniformType == SAMPLER_2D ?
					p_emptyTexture2D :
					p_emptyTextureCube,
				textureIndex.value()
			);
		}

		if (prop.singleUse)
		{
			value = prop.defaultValue;
		}
	}
}

void OvRendering::Data::Material::Unbind(bool p_resetBoundProgram)
{
	ZoneScoped;

	OVASSERT(IsValid(), "Attempting to unbind an invalid material.");
	OVASSERT(m_programInUse.has_value(), "Attempting to unbind a material that hasn't been bound");

	if (p_resetBoundProgram)
	{
		m_programInUse->Unbind();
	}

	m_programInUse.reset();
}

bool OvRendering::Data::Material::HasProperty(const std::string& p_name) const
{
	OVASSERT(IsValid(), "Attempting to call HasProperty on an invalid material.");
	return m_properties.contains(p_name);
}

void OvRendering::Data::Material::SetProperty(const std::string& p_name, const MaterialPropertyType& p_value, bool p_singleUse)
{
	OVASSERT(IsValid(), "Attempting to SetProperty on an invalid material.");
	OVASSERT(HasProperty(p_name), "Attempting to SetProperty on a non-existing property.");

	auto [it, inserted] = m_properties.try_emplace(p_name);

	if (inserted)
	{
		++m_propertiesLayoutVersion;
	}

	it->second.value = p_value;
	it->second.singleUse = p_singleUse;

	if (p_singleUse)
	{
		++m_singleUsePropertySignatureVersion;
	}
	else
	{
		++m_stablePropertySignatureVersion;
	}
}

bool OvRendering::Data::Material::TrySetProperty(const std::string& p_name, const MaterialPropertyType& p_value, bool p_singleUse)
{
	if (HasProperty(p_name))
	{
		SetProperty(p_name, p_value, p_singleUse);
		return true;
	}

	return false;
}

OvTools::Utils::OptRef<const OvRendering::Data::MaterialProperty> OvRendering::Data::Material::GetProperty(const std::string& p_key) const
{
	OVASSERT(IsValid(), "Attempting to GetProperty on an invalid material.");

	if (auto it = m_properties.find(p_key); it != m_properties.end())
	{
		return it->second;
	}

	return std::nullopt;
}

OvRendering::Resources::Shader*& OvRendering::Data::Material::GetShader()
{
	return m_shader;
}

bool OvRendering::Data::Material::HasShader() const
{
	return m_shader;
}

bool OvRendering::Data::Material::IsValid() const
{
	return HasShader();
}

void OvRendering::Data::Material::SetOrthographicSupport(bool p_supportOrthographic)
{
	m_supportOrthographic = p_supportOrthographic;
}

void OvRendering::Data::Material::SetPerspectiveSupport(bool p_supportPerspective)
{
	m_supportPerspective = p_supportPerspective;
}

void OvRendering::Data::Material::SetDrawOrder(int p_order)
{
	m_drawOrder = p_order;
}

void OvRendering::Data::Material::SetBlendable(bool p_transparent)
{
	m_blendable = p_transparent;
}

void OvRendering::Data::Material::SetUserInterface(bool p_userInterface)
{
	m_userInterface = p_userInterface;
}

void OvRendering::Data::Material::SetBackfaceCulling(bool p_backfaceCulling)
{
	m_backfaceCulling = p_backfaceCulling;
}

void OvRendering::Data::Material::SetFrontfaceCulling(bool p_frontfaceCulling)
{
	m_frontfaceCulling = p_frontfaceCulling;
}

void OvRendering::Data::Material::SetDepthTest(bool p_depthTest)
{
	m_depthTest = p_depthTest;
}

void OvRendering::Data::Material::SetDepthWriting(bool p_depthWriting)
{
	m_depthWriting = p_depthWriting;
}

void OvRendering::Data::Material::SetColorWriting(bool p_colorWriting)
{
	m_colorWriting = p_colorWriting;
}

void OvRendering::Data::Material::SetCastShadows(bool p_castShadows)
{
	m_castShadows = p_castShadows;
}

void OvRendering::Data::Material::SetReceiveShadows(bool p_receiveShadows)
{
	m_receiveShadows = p_receiveShadows;
}

void OvRendering::Data::Material::SetCapturedByReflectionProbes(bool p_capturedByReflectionProbes)
{
	m_capturedByReflectionProbes = p_capturedByReflectionProbes;
}

void OvRendering::Data::Material::SetReceiveReflections(bool p_receiveReflections)
{
	m_receiveReflections = p_receiveReflections;
}

void OvRendering::Data::Material::SetGPUInstances(int p_instances)
{
	m_gpuInstances = p_instances;
}

int OvRendering::Data::Material::GetDrawOrder() const
{
	return m_drawOrder;
}

bool OvRendering::Data::Material::IsBlendable() const
{
	return m_blendable;
}

bool OvRendering::Data::Material::IsUserInterface() const
{
	return m_userInterface;
}

bool OvRendering::Data::Material::HasBackfaceCulling() const
{
	return m_backfaceCulling;
}

bool OvRendering::Data::Material::HasFrontfaceCulling() const
{
	return m_frontfaceCulling;
}

bool OvRendering::Data::Material::HasDepthTest() const
{
	return m_depthTest;
}

bool OvRendering::Data::Material::HasDepthWriting() const
{
	return m_depthWriting;
}

bool OvRendering::Data::Material::HasColorWriting() const
{
	return m_colorWriting;
}

bool OvRendering::Data::Material::IsShadowCaster() const
{
	return m_castShadows;
}

bool OvRendering::Data::Material::IsShadowReceiver() const
{
	return m_receiveShadows;
}

bool OvRendering::Data::Material::IsCapturedByReflectionProbes() const
{
	return m_capturedByReflectionProbes;
}

bool OvRendering::Data::Material::IsReflectionReceiver() const
{
	return m_receiveReflections;
}

int OvRendering::Data::Material::GetGPUInstances() const
{
	return m_gpuInstances;
}

const OvRendering::Data::StateMask OvRendering::Data::Material::GenerateStateMask() const
{
	StateMask stateMask;
	stateMask.depthWriting = m_depthWriting;
	stateMask.colorWriting = m_colorWriting;
	stateMask.blendable = m_blendable;
	stateMask.depthTest = m_depthTest;
	stateMask.frontfaceCulling = m_frontfaceCulling;
	stateMask.backfaceCulling = m_backfaceCulling;
	return stateMask;
}

OvRendering::Data::Material::PropertyMap& OvRendering::Data::Material::GetProperties()
{
	// The caller can add or remove properties through the returned reference
	++m_propertiesLayoutVersion;
	return m_properties;
}

const OvRendering::Data::FeatureSet& OvRendering::Data::Material::GetFeatures() const
{
	return m_features;
}

void OvRendering::Data::Material::SetFeatures(const Data::FeatureSet& p_features)
{
	m_features = p_features;
	++m_featuresVersion;
}

void OvRendering::Data::Material::AddFeature(const std::string& p_feature)
{
	m_features.insert(p_feature);
	++m_featuresVersion;
}

void OvRendering::Data::Material::RemoveFeature(const std::string& p_feature)
{
	m_features.erase(p_feature);
	++m_featuresVersion;
}

bool OvRendering::Data::Material::HasFeature(const std::string& p_feature) const
{
	return m_features.contains(p_feature);
}

bool OvRendering::Data::Material::SupportsFeature(const std::string& p_feature) const
{
	return m_shader &&
		(m_shader->GetFeatures().contains(p_feature) ||
		 m_shader->GetEngineFeatures().contains(p_feature));
}

bool OvRendering::Data::Material::HasPass(const std::string& p_pass) const
{
	return m_shader->GetPasses().contains(p_pass);
}

bool OvRendering::Data::Material::SupportsOrthographic() const
{
	return m_supportOrthographic;
}

bool OvRendering::Data::Material::SupportsPerspective() const
{
	return m_supportPerspective;
}

bool OvRendering::Data::Material::SupportsProjectionMode(OvRendering::Settings::EProjectionMode p_projectionMode) const
{
	using enum OvRendering::Settings::EProjectionMode;

	switch (p_projectionMode)
	{
	case ORTHOGRAPHIC: return SupportsOrthographic();
	case PERSPECTIVE: return SupportsPerspective();
	}

	return true;
}

void OvRendering::Data::Material::InvalidatePropertySignature()
{
	++m_stablePropertySignatureVersion;
	++m_singleUsePropertySignatureVersion;
}

OvRendering::Data::MaterialSignatureSet OvRendering::Data::Material::CalculateSignature(
	baregl::ShaderProgram& p_selectedProgram,
	baregl::Texture* p_emptyTexture2D,
	baregl::Texture* p_emptyTextureCube
)
{
	ZoneScoped;

	MaterialSignatureSet signature;

	// Bind
	HashCombine(signature.bindSignature, this);
	HashCombine(signature.bindSignature, &p_selectedProgram);

	// Stable Properties
	signature.stablePropertySignature = signature.bindSignature;
	HashCombine(signature.stablePropertySignature, m_stablePropertySignatureVersion);
	HashCombine(signature.stablePropertySignature, p_emptyTexture2D);
	HashCombine(signature.stablePropertySignature, p_emptyTextureCube);

	// Single Use Properties
	signature.singleUsePropertySignature = signature.bindSignature;
	HashCombine(signature.singleUsePropertySignature, m_singleUsePropertySignatureVersion);
	HashCombine(signature.singleUsePropertySignature, p_emptyTexture2D);
	HashCombine(signature.singleUsePropertySignature, p_emptyTextureCube);

	return signature;
}

baregl::ShaderProgram& OvRendering::Data::Material::GetCachedVariant(std::optional<const std::string_view> p_pass)
{
	OVASSERT(m_shader, "Cannot get a variant without a shader");

	const bool isCacheValid =
		m_variantCacheShader == m_shader &&
		m_variantCacheShaderGeneration == m_shader->GetGeneration() &&
		m_variantCacheFeaturesVersion == m_featuresVersion;

	if (!isCacheValid)
	{
		m_variantCache.clear();
		m_variantCacheShader = m_shader;
		m_variantCacheShaderGeneration = m_shader->GetGeneration();
		m_variantCacheFeaturesVersion = m_featuresVersion;
	}

	const std::string_view pass = p_pass.value_or(std::string_view{});

	for (const auto& entry : m_variantCache)
	{
		if (entry.pass == pass)
		{
			return *entry.program;
		}
	}

	auto& program = m_shader->GetVariant(p_pass, m_features);
	m_variantCache.push_back({ std::string{ pass }, &program });
	return program;
}

void OvRendering::Data::Material::UpdateUniformBindings(const baregl::ShaderProgram& p_program)
{
	const bool isCacheValid =
		m_uniformBindingsOwner == &m_properties &&
		m_uniformBindingsProgram == &p_program &&
		m_uniformBindingsProgramLinkID == p_program.GetLinkID() &&
		m_uniformBindingsLayoutVersion == m_propertiesLayoutVersion;

	if (isCacheValid)
	{
		return;
	}

	m_uniformBindings.clear();

	for (auto& [name, prop] : m_properties)
	{
		// Skip properties that the program isn't using
		if (const auto uniformInfo = p_program.GetUniformInfo(name))
		{
			m_uniformBindings.push_back({ &prop, &uniformInfo.value().get() });
		}
	}

	m_uniformBindingsOwner = &m_properties;
	m_uniformBindingsProgram = &p_program;
	m_uniformBindingsProgramLinkID = p_program.GetLinkID();
	m_uniformBindingsLayoutVersion = m_propertiesLayoutVersion;
}
