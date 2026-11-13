/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <OvCore/Scripting/Lua/LuaComponentRef.h>

#include <OvCore/ECS/Actor.h>  
#include <OvCore/ECS/Components/CAmbientBoxLight.h>  
#include <OvCore/ECS/Components/CAmbientSphereLight.h>  
#include <OvCore/ECS/Components/CAudioListener.h>  
#include <OvCore/ECS/Components/CAudioSource.h>  
#include <OvCore/ECS/Components/CBoneAttachment.h>
#include <OvCore/ECS/Components/CCamera.h>  
#include <OvCore/ECS/Components/CDirectionalLight.h>  
#include <OvCore/ECS/Components/CMaterialRenderer.h>  
#include <OvCore/ECS/Components/CModelRenderer.h>  
#include <OvCore/ECS/Components/CPhysicalBox.h>  
#include <OvCore/ECS/Components/CCharacterController.h>
#include <OvCore/ECS/Components/CPhysicalCapsule.h>  
#include <OvCore/ECS/Components/CPhysicalHeightField.h>
#include <OvCore/ECS/Components/CPhysicalSphere.h>  
#include <OvCore/ECS/Components/CParticleSystem.h>
#include <OvCore/ECS/Components/CPointLight.h>  
#include <OvCore/ECS/Components/CPostProcessStack.h>  
#include <OvCore/ECS/Components/CReflectionProbe.h>  
#include <OvCore/ECS/Components/CSkinnedMeshRenderer.h>
#include <OvCore/ECS/Components/CSpotLight.h>
#include <OvCore/ECS/Components/CTransform.h>
#include <OvCore/ECS/Components/UI/CCanvas.h>
#include <OvCore/ECS/Components/UI/CHorizontalLayout.h>
#include <OvCore/ECS/Components/UI/CImage.h>
#include <OvCore/ECS/Components/UI/CLayoutGroup.h>
#include <OvCore/ECS/Components/UI/CText.h>
#include <OvCore/ECS/Components/UI/CVerticalLayout.h>
#include <OvCore/Scripting/Common/ScriptPropertyValue.h>
#include <OvPhysics/Settings/PhysicsSettings.h>

namespace
{
	using OvCore::ECS::Components::CParticleSystem;
	using OvCore::Particles::ParticleSettings;

	// A setting of a particle system, read from and written to a copy of its settings
	template<typename F>
	void ChangeParticleSettings(CParticleSystem& p_system, F&& p_change)
	{
		ParticleSettings settings = p_system.GetSettings();
		p_change(settings);
		p_system.SetSettings(settings);
	}

	uint32_t ToParticleCount(double p_count, const char* p_function)
	{
		if (!std::isfinite(p_count) || p_count < 0.0 || std::floor(p_count) != p_count || p_count > ParticleSettings::kMaxParticles)
		{
			throw sol::error(std::format("ParticleSystem:{} takes a whole number from 0 to {}", p_function, ParticleSettings::kMaxParticles));
		}

		return static_cast<uint32_t>(p_count);
	}

	// The overrides of ParticleSystem:Emit, from a table: a key it does not know raises an error, a misspelt key
	// would otherwise be silently dropped
	OvCore::Particles::ParticleOverrides ToParticleOverrides(const sol::table& p_table)
	{
		OvCore::Particles::ParticleOverrides overrides;

		for (const auto& [key, value] : p_table)
		{
			const auto name = key.is<std::string>() ? key.as<std::string>() : std::string{};

			const auto expect = [&]<typename T>(const char* p_type) -> T
			{
				if (!value.is<T>())
				{
					throw sol::error(std::format("ParticleSystem:Emit: '{}' must be a {}", name, p_type));
				}
				return value.as<T>();
			};

			if (name == "position") overrides.position = expect.operator()<OvMaths::FVector3>("Vector3");
			else if (name == "direction") overrides.direction = expect.operator()<OvMaths::FVector3>("Vector3");
			else if (name == "color") overrides.color = expect.operator()<OvMaths::FVector4>("Vector4");
			else if (name == "speed") overrides.speed = expect.operator()<float>("number");
			else if (name == "size") overrides.size = expect.operator()<float>("number");
			else if (name == "lifetime") overrides.lifetime = expect.operator()<float>("number");
			else throw sol::error(std::format("ParticleSystem:Emit: unknown override '{}' (position, direction, color, speed, size, lifetime)", name));
		}

		return overrides;
	}

	constexpr uint32_t kMinimumHeightFieldSampleCount = 3;
	constexpr uint32_t kMaximumHeightFieldSampleCount = 4096;

	// true, or false and why: the heights are only applied when every argument is valid
	std::tuple<bool, std::optional<std::string>> SetHeights(
		OvCore::ECS::Components::CPhysicalHeightField& p_heightField,
		const sol::object& p_sampleCount,
		const sol::object& p_spacing,
		const sol::object& p_heights
	)
	{
		const auto count = p_sampleCount.as<sol::optional<double>>();
		if (!count || *count != std::floor(*count) || *count < kMinimumHeightFieldSampleCount || *count > kMaximumHeightFieldSampleCount)
		{
			return { false, std::format("the sample count must be a whole number from {} to {}", kMinimumHeightFieldSampleCount, kMaximumHeightFieldSampleCount) };
		}

		const auto spacing = p_spacing.as<sol::optional<double>>();
		if (!spacing || !std::isfinite(*spacing) || *spacing <= 0.0)
		{
			return { false, "the spacing must be a positive number" };
		}

		if (p_heights.get_type() != sol::type::table)
		{
			return { false, "the heights must be an array of numbers" };
		}

		const auto sampleCount = static_cast<uint32_t>(*count);
		const sol::table table = p_heights.as<sol::table>();
		const size_t expectedSize = static_cast<size_t>(sampleCount) * sampleCount;
		const size_t size = table.size();

		if (size != 0 && size != expectedSize)
		{
			return { false, std::format("{} heights given for {} samples", size, expectedSize) };
		}

		std::vector<float> heights(size);

		for (size_t index = 0; index < size; ++index)
		{
			const auto height = table.raw_get<sol::optional<double>>(index + 1);
			if (!height || !std::isfinite(*height))
			{
				return { false, std::format("the height {} is not a finite number", index + 1) };
			}

			heights[index] = static_cast<float>(*height);
		}

		p_heightField.SetHeights(sampleCount, static_cast<float>(*spacing), std::move(heights));
		return { true, std::nullopt };
	}
}

void BindLuaComponents(sol::state& p_luaState)
{
	using namespace OvMaths;
	using namespace OvCore::ECS;
	using namespace OvCore::ECS::Components;

	// Lua holds components through handles checked on every use (LuaComponentRef.h)
	p_luaState.new_usertype<AComponent>("Component",
		"GetOwner", [](AComponent& p_component) { return OvCore::Scripting::ActorRef{ p_component.owner.GetGUID() }; },
		"IsAlive", &OvCore::Scripting::LuaComponentIsAlive
	);

	p_luaState.new_usertype<CTransform>("Transform",
		sol::base_classes, sol::bases<AComponent>(),
		"SetPosition", &CTransform::SetLocalPosition,
		"SetRotation", &CTransform::SetLocalRotation,
		"SetScale", &CTransform::SetLocalScale,
		"SetLocalPosition", &CTransform::SetLocalPosition,
		"SetLocalRotation", &CTransform::SetLocalRotation,
		"SetLocalScale", &CTransform::SetLocalScale,
		"SetWorldPosition", &CTransform::SetWorldPosition,
		"SetWorldRotation", &CTransform::SetWorldRotation,
		"SetWorldScale", &CTransform::SetWorldScale,
		"GetPosition", [](CTransform& p_this) -> FVector3 { return p_this.GetWorldPosition(); },
		"GetRotation", [](CTransform& p_this) -> FQuaternion { return p_this.GetWorldRotation(); },
		"GetScale", [](CTransform& p_this) -> FVector3 { return p_this.GetWorldScale(); },
		"GetLocalPosition", [](CTransform& p_this) -> FVector3 { return p_this.GetLocalPosition(); },
		"GetLocalRotation", [](CTransform& p_this) -> FQuaternion { return p_this.GetLocalRotation(); },
		"GetLocalScale", [](CTransform& p_this) -> FVector3 { return p_this.GetLocalScale(); },
		"GetWorldPosition", [](CTransform& p_this) -> FVector3 { return p_this.GetWorldPosition(); },
		"GetWorldRotation", [](CTransform& p_this) -> FQuaternion { return p_this.GetWorldRotation(); },
		"GetWorldScale", [](CTransform& p_this) -> FVector3 { return p_this.GetWorldScale(); },
		"GetForward", &CTransform::GetWorldForward,
		"GetUp", &CTransform::GetWorldUp,
		"GetRight", &CTransform::GetWorldRight,
		"GetLocalForward", &CTransform::GetLocalForward,
		"GetLocalUp", &CTransform::GetLocalUp,
		"GetLocalRight", &CTransform::GetLocalRight,
		"GetWorldForward", &CTransform::GetWorldForward,
		"GetWorldUp", &CTransform::GetWorldUp,
		"GetWorldRight", &CTransform::GetWorldRight,
		"EnableUIData", &CTransform::EnableUIData,
		"DisableUIData", &CTransform::DisableUIData,
		"HasUIData", &CTransform::HasUIData,
		"HasActiveUIData", &CTransform::HasActiveUIData,
		"GetUIPosition", [](CTransform& p_this) -> FVector2 { return p_this.GetUIPosition(); },
		"SetUIPosition", &CTransform::SetUIPosition,
		"GetUIRotation", &CTransform::GetUIRotation,
		"SetUIRotation", &CTransform::SetUIRotation,
		"GetUIScale", [](CTransform& p_this) -> FVector2 { return p_this.GetUIScale(); },
		"SetUIScale", &CTransform::SetUIScale,
		"GetUISize", [](CTransform& p_this) -> FVector2 { return p_this.GetUISize(); },
		"SetUISize", &CTransform::SetUISize,
		"GetUIPivot", [](CTransform& p_this) -> FVector2 { return p_this.GetUIPivot(); },
		"SetUIPivot", &CTransform::SetUIPivot,
		"GetUIAnchorPreset", &CTransform::GetUIAnchorPreset,
		"SetUIAnchorPreset", &CTransform::SetUIAnchorPreset,
		"GetUIClipChildren", &CTransform::GetUIClipChildren,
		"SetUIClipChildren", &CTransform::SetUIClipChildren
	);

	p_luaState.new_enum<OvCore::ECS::Components::CModelRenderer::EFrustumBehaviour>("FrustumBehaviour", {
		{"DISABLED", OvCore::ECS::Components::CModelRenderer::EFrustumBehaviour::DISABLED},
		// {"DEPRECATED_MODEL_BOUNDS", OvCore::ECS::Components::CModelRenderer::EFrustumBehaviour::DEPRECATED_MODEL_BOUNDS},
		{"MESH_BOUNDS", OvCore::ECS::Components::CModelRenderer::EFrustumBehaviour::MESH_BOUNDS},
		{"CUSTOM_BOUNDS", OvCore::ECS::Components::CModelRenderer::EFrustumBehaviour::CUSTOM_BOUNDS}
	});

	p_luaState.new_usertype<CModelRenderer>("ModelRenderer",
		sol::base_classes, sol::bases<AComponent>(),
		"GetModel", &CModelRenderer::GetModel,
		"SetModel", [](CModelRenderer& p_this, OvRendering::Resources::Model* p_model)
		{
			p_this.SetModel(p_model);
		},
		"GetFrustumBehaviour", &CModelRenderer::GetFrustumBehaviour,
		"SetFrustumBehaviour", &CModelRenderer::SetFrustumBehaviour,
		"GetMaxDrawDistance", &CModelRenderer::GetMaxDrawDistance,
		"SetMaxDrawDistance", &CModelRenderer::SetMaxDrawDistance
	);

	p_luaState.new_enum<OvCore::Rendering::EVisibilityFlags>("VisibilityFlags", {
		{ "NONE", OvCore::Rendering::EVisibilityFlags::NONE },
		{ "GEOMETRY", OvCore::Rendering::EVisibilityFlags::GEOMETRY },
		{ "SHADOW", OvCore::Rendering::EVisibilityFlags::SHADOW },
		{ "REFLECTION", OvCore::Rendering::EVisibilityFlags::REFLECTION },
		{ "ALL", OvCore::Rendering::EVisibilityFlags::ALL }
	});

	p_luaState.new_usertype<CMaterialRenderer>("MaterialRenderer",
		sol::base_classes, sol::bases<AComponent>(),
		"SetMaterial", &CMaterialRenderer::SetMaterialAtIndex,
		"SetUserMatrixElement", &CMaterialRenderer::SetUserMatrixElement,
		"GetUserMatrixElement", &CMaterialRenderer::GetUserMatrixElement,
		"SetVisibilityFlags", &CMaterialRenderer::SetVisibilityFlags,
		"GetVisibilityFlags", &CMaterialRenderer::GetVisibilityFlags
	);

	p_luaState.new_usertype<CBoneAttachment>("BoneAttachment",
		sol::base_classes, sol::bases<AComponent>(),
		"SetBone", &CBoneAttachment::SetBone,
		"GetBone", &CBoneAttachment::GetBone,
		"SetOffsetPosition", &CBoneAttachment::SetOffsetPosition,
		"GetOffsetPosition", &CBoneAttachment::GetOffsetPosition,
		"SetOffsetRotation", &CBoneAttachment::SetOffsetRotation,
		"GetOffsetRotation", &CBoneAttachment::GetOffsetRotation,
		"SetOffsetScale", &CBoneAttachment::SetOffsetScale,
		"GetOffsetScale", &CBoneAttachment::GetOffsetScale,
		"SetTarget", [](CBoneAttachment& p_this, std::optional<OvCore::Scripting::ActorRef> p_target) {
			p_this.SetTarget(p_target && p_target->guid != 0 ? &p_target->Resolve() : nullptr);
		},
		"GetTarget", [](CBoneAttachment& p_this) -> std::optional<OvCore::Scripting::ActorRef> {
			if (p_this.GetTargetGUID() == 0) return std::nullopt;
			return OvCore::Scripting::ActorRef{ p_this.GetTargetGUID() };
		},
		"IsAttached", &CBoneAttachment::IsAttached
	);

	p_luaState.new_enum<CSkinnedMeshRenderer::EUpdateMode>("AnimationUpdateMode", {
		{ "ALWAYS", CSkinnedMeshRenderer::EUpdateMode::ALWAYS },
		{ "WHEN_VISIBLE", CSkinnedMeshRenderer::EUpdateMode::WHEN_VISIBLE }
	});

	p_luaState.new_usertype<CSkinnedMeshRenderer>("SkinnedMeshRenderer",
		sol::base_classes, sol::bases<AComponent>(),
		"GetLayerCount", &CSkinnedMeshRenderer::GetLayerCount,
		"AddLayer", &CSkinnedMeshRenderer::AddLayer,
		"RemoveLayer", &CSkinnedMeshRenderer::RemoveLayer,
		"Play", [](CSkinnedMeshRenderer& p_this, std::optional<uint32_t> p_layer) { p_this.Play(p_layer.value_or(0)); },
		"Pause", [](CSkinnedMeshRenderer& p_this, std::optional<uint32_t> p_layer) { p_this.Pause(p_layer.value_or(0)); },
		"Stop", [](CSkinnedMeshRenderer& p_this, std::optional<uint32_t> p_layer) { p_this.Stop(p_layer.value_or(0)); },
		"IsPlaying", [](CSkinnedMeshRenderer& p_this, std::optional<uint32_t> p_layer) { return p_this.IsPlaying(p_layer.value_or(0)); },
		"SetLooping", [](CSkinnedMeshRenderer& p_this, bool p_value, std::optional<uint32_t> p_layer) { p_this.SetLooping(p_value, p_layer.value_or(0)); },
		"IsLooping", [](CSkinnedMeshRenderer& p_this, std::optional<uint32_t> p_layer) { return p_this.IsLooping(p_layer.value_or(0)); },
		"SetPlaybackSpeed", [](CSkinnedMeshRenderer& p_this, float p_value, std::optional<uint32_t> p_layer) { p_this.SetPlaybackSpeed(p_value, p_layer.value_or(0)); },
		"GetPlaybackSpeed", [](CSkinnedMeshRenderer& p_this, std::optional<uint32_t> p_layer) { return p_this.GetPlaybackSpeed(p_layer.value_or(0)); },
		"SetLayerWeight", [](CSkinnedMeshRenderer& p_this, float p_value, std::optional<uint32_t> p_layer) { p_this.SetLayerWeight(p_value, p_layer.value_or(0)); },
		"GetLayerWeight", [](CSkinnedMeshRenderer& p_this, std::optional<uint32_t> p_layer) { return p_this.GetLayerWeight(p_layer.value_or(0)); },
		"SetLayerMask", [](CSkinnedMeshRenderer& p_this, const std::string& p_nodeName, std::optional<uint32_t> p_layer) { p_this.SetLayerMask(p_nodeName, p_layer.value_or(0)); },
		"GetLayerMask", [](CSkinnedMeshRenderer& p_this, std::optional<uint32_t> p_layer) { return p_this.GetLayerMask(p_layer.value_or(0)); },
		"SetMeshBoundsScale", &CSkinnedMeshRenderer::SetMeshBoundsScale,
		"GetMeshBoundsScale", &CSkinnedMeshRenderer::GetMeshBoundsScale,
		"SetUpdateMode", &CSkinnedMeshRenderer::SetUpdateMode,
		"GetUpdateMode", &CSkinnedMeshRenderer::GetUpdateMode,
		"WasVisible", &CSkinnedMeshRenderer::WasVisible,
		"GetPoseEvaluationCount", &CSkinnedMeshRenderer::GetPoseEvaluationCount,
		"GetSkeletonUpdateCount", &CSkinnedMeshRenderer::GetSkeletonUpdateCount,
		"SetTime", [](CSkinnedMeshRenderer& p_this, float p_timeSeconds, std::optional<uint32_t> p_layer) { p_this.SetTime(p_timeSeconds, p_layer.value_or(0)); },
		"GetTime", [](CSkinnedMeshRenderer& p_this, std::optional<uint32_t> p_layer) { return p_this.GetTime(p_layer.value_or(0)); },
		"SetAnimationSourceModel", sol::overload(
			[](CSkinnedMeshRenderer& p_this, OvRendering::Resources::Model* p_model, std::optional<uint32_t> p_layer) { p_this.SetAnimationSourceModel(p_model, p_layer.value_or(0)); },
			[](CSkinnedMeshRenderer& p_this, const OvCore::Scripting::AssetRef& p_asset, std::optional<uint32_t> p_layer) { p_this.SetAnimationSourceModel(p_asset.path, p_layer.value_or(0)); }
		),
		"GetAnimationSourceModel", [](CSkinnedMeshRenderer& p_this, std::optional<uint32_t> p_layer) { return p_this.GetAnimationSourceModel(p_layer.value_or(0)); },
		"IsAnimationSourceCompatible", [](CSkinnedMeshRenderer& p_this, std::optional<uint32_t> p_layer) { return p_this.IsAnimationSourceCompatible(p_layer.value_or(0)); },
		"GetAnimationCount", [](CSkinnedMeshRenderer& p_this, std::optional<uint32_t> p_layer) { return p_this.GetAnimationCount(p_layer.value_or(0)); },
		"GetAnimationName", [](CSkinnedMeshRenderer& p_this, uint32_t p_index, std::optional<uint32_t> p_layer) { return p_this.GetAnimationName(p_index, p_layer.value_or(0)); },
		"SetAnimation", sol::overload(
			[](CSkinnedMeshRenderer& p_this, const std::string& p_name, std::optional<uint32_t> p_layer)
			{
				return p_this.SetAnimation(p_name, p_layer.value_or(0));
			},
			[](CSkinnedMeshRenderer& p_this, std::optional<uint32_t> p_index, std::optional<uint32_t> p_layer)
			{
				return p_this.SetAnimation(p_index, p_layer.value_or(0));
			}
		),
		"GetActiveAnimationIndex", [](CSkinnedMeshRenderer& p_this, std::optional<uint32_t> p_layer) { return p_this.GetActiveAnimationIndex(p_layer.value_or(0)); },
		"GetActiveAnimationName", [](CSkinnedMeshRenderer& p_this, std::optional<uint32_t> p_layer) { return p_this.GetActiveAnimationName(p_layer.value_or(0)); },
		"GetBoneCount", &CSkinnedMeshRenderer::GetBoneCount,
		"GetBoneName", &CSkinnedMeshRenderer::GetBoneName,
		"GetBoneIndex", &CSkinnedMeshRenderer::GetBoneIndex,
		"GetBoneLocalPosition", &CSkinnedMeshRenderer::GetBoneLocalPosition,
		"GetBoneLocalRotation", &CSkinnedMeshRenderer::GetBoneLocalRotation,
		"GetBoneLocalScale", &CSkinnedMeshRenderer::GetBoneLocalScale,
		"GetBoneGlobalPosition", &CSkinnedMeshRenderer::GetBoneGlobalPosition,
		"GetBoneGlobalRotation", &CSkinnedMeshRenderer::GetBoneGlobalRotation,
		"GetBoneGlobalScale", &CSkinnedMeshRenderer::GetBoneGlobalScale,
		"SetBoneLocalPosition", &CSkinnedMeshRenderer::SetBoneLocalPosition,
		"SetBoneLocalRotation", &CSkinnedMeshRenderer::SetBoneLocalRotation,
		"SetBoneLocalScale", &CSkinnedMeshRenderer::SetBoneLocalScale,
		"StartRagdoll", [](CSkinnedMeshRenderer& p_this, sol::table p_parts, sol::optional<FVector3> p_linearVelocity)
		{
			std::vector<CSkinnedMeshRenderer::RagdollPart> parts;

			for (size_t index = 1; index <= p_parts.size(); ++index)
			{
				const sol::optional<sol::table> entry = p_parts[index];
				if (!entry.has_value())
				{
					continue;
				}

				CSkinnedMeshRenderer::RagdollPart part;
				part.bone = entry->get_or<std::string>("Bone", "");
				part.to = entry->get_or<std::string>("To", "");
				part.length = entry->get_or("Length", part.length);
				part.radius = entry->get_or("Radius", part.radius);
				part.mass = entry->get_or("Mass", part.mass);
				part.swing = entry->get_or("Swing", part.swing);
				part.twist = entry->get_or("Twist", part.twist);
				parts.push_back(std::move(part));
			}

			return p_this.StartRagdoll(parts, p_linearVelocity.value_or(FVector3::Zero));
		},
		"StopRagdoll", &CSkinnedMeshRenderer::StopRagdoll,
		"IsRagdoll", &CSkinnedMeshRenderer::IsRagdoll,
		"AddRagdollImpulse", &CSkinnedMeshRenderer::AddRagdollImpulse
	);

	p_luaState.new_enum<OvPhysics::Entities::PhysicalObject::ECollisionDetectionMode>("CollisionDetectionMode", {
		{"DISCRETE", OvPhysics::Entities::PhysicalObject::ECollisionDetectionMode::DISCRETE},
		{"CONTINUOUS", OvPhysics::Entities::PhysicalObject::ECollisionDetectionMode::CONTINUOUS}
	});

	p_luaState.new_usertype<CPhysicalObject>("PhysicalObject",
		sol::base_classes, sol::bases<AComponent>(),
		"GetMass", &CPhysicalObject::GetMass,
		"SetMass", &CPhysicalObject::SetMass,
		"GetFriction", &CPhysicalObject::GetFriction,
		"SetFriction", &CPhysicalObject::SetFriction,
		"GetBounciness", &CPhysicalObject::GetBounciness,
		"SetBounciness", &CPhysicalObject::SetBounciness,
		"SetLinearVelocity", &CPhysicalObject::SetLinearVelocity,
		"SetAngularVelocity", &CPhysicalObject::SetAngularVelocity,
		"GetLinearVelocity", &CPhysicalObject::GetLinearVelocity,
		"GetAngularVelocity", &CPhysicalObject::GetAngularVelocity,
		"SetLinearFactor", &CPhysicalObject::SetLinearFactor,
		"SetAngularFactor", &CPhysicalObject::SetAngularFactor,
		"GetLinearFactor", &CPhysicalObject::GetLinearFactor,
		"GetAngularFactor", &CPhysicalObject::GetAngularFactor,
		"IsTrigger", &CPhysicalObject::IsTrigger,
		"SetTrigger", &CPhysicalObject::SetTrigger,
		"AddForce", &CPhysicalObject::AddForce,
		"AddImpulse", &CPhysicalObject::AddImpulse,
		"ClearForces", &CPhysicalObject::ClearForces,
		"SetCollisionDetectionMode", &CPhysicalObject::SetCollisionDetectionMode,
		"GetCollisionMode", &CPhysicalObject::GetCollisionDetectionMode,
		"SetKinematic", &CPhysicalObject::SetKinematic,
		"IsStatic", &CPhysicalObject::IsStatic,
		"SetStatic", &CPhysicalObject::SetStatic,
		"IsInterpolated", &CPhysicalObject::IsInterpolated,
		"SetInterpolated", &CPhysicalObject::SetInterpolated,
		"GetCollisionLayer", [](CPhysicalObject& p_this) { return static_cast<int>(p_this.GetCollisionLayer()); },
		"SetCollisionLayer", [](CPhysicalObject& p_this, int p_layer)
		{
			constexpr int kLastLayer = OvPhysics::Settings::PhysicsSettings::kLayerCount - 1;
			p_this.SetCollisionLayer(static_cast<uint16_t>(std::clamp(p_layer, 0, kLastLayer)));
		}
	);

	p_luaState.new_usertype<CPhysicalBox>("PhysicalBox",
		sol::base_classes, sol::bases<CPhysicalObject, AComponent>(),
		"GetSize", &CPhysicalBox::GetSize,
		"SetSize", &CPhysicalBox::SetSize
	);

	p_luaState.new_usertype<CPhysicalSphere>("PhysicalSphere",
		sol::base_classes, sol::bases<CPhysicalObject, AComponent>(),
		"GetRadius", &CPhysicalSphere::GetRadius,
		"SetRadius", &CPhysicalSphere::SetRadius
	);

	p_luaState.new_usertype<CPhysicalCapsule>("PhysicalCapsule",
		sol::base_classes, sol::bases<CPhysicalObject, AComponent>(),
		"GetRadius", &CPhysicalCapsule::GetRadius,
		"SetRadius", &CPhysicalCapsule::SetRadius,
		"GetHeight", &CPhysicalCapsule::GetHeight,
		"SetHeight", &CPhysicalCapsule::SetHeight
	);

	p_luaState.new_usertype<CPhysicalHeightField>("PhysicalHeightField",
		sol::base_classes, sol::bases<CPhysicalObject, AComponent>(),
		"SetHeights", &SetHeights,
		"GetSampleCount", &CPhysicalHeightField::GetSampleCount,
		"GetSpacing", &CPhysicalHeightField::GetSpacing
	);

	p_luaState.new_enum<CCharacterController::EGroundState>("CharacterGroundState", {
		{"ON_GROUND", CCharacterController::EGroundState::ON_GROUND},
		{"ON_STEEP_GROUND", CCharacterController::EGroundState::ON_STEEP_GROUND},
		{"NOT_SUPPORTED", CCharacterController::EGroundState::NOT_SUPPORTED},
		{"IN_AIR", CCharacterController::EGroundState::IN_AIR}
	});

	p_luaState.new_usertype<CCharacterController>("CharacterController",
		sol::base_classes, sol::bases<CPhysicalObject, AComponent>(),
		"GetRadius", &CCharacterController::GetRadius,
		"SetRadius", &CCharacterController::SetRadius,
		"GetHeight", &CCharacterController::GetHeight,
		"SetHeight", &CCharacterController::SetHeight,
		"GetMaxSlopeAngle", &CCharacterController::GetMaxSlopeAngle,
		"SetMaxSlopeAngle", &CCharacterController::SetMaxSlopeAngle,
		"GetStepHeight", &CCharacterController::GetStepHeight,
		"SetStepHeight", &CCharacterController::SetStepHeight,
		"GetGroundSnapDistance", &CCharacterController::GetGroundSnapDistance,
		"SetGroundSnapDistance", &CCharacterController::SetGroundSnapDistance,
		"GetGravityFactor", &CCharacterController::GetGravityFactor,
		"SetGravityFactor", &CCharacterController::SetGravityFactor,
		"GetPushStrength", &CCharacterController::GetPushStrength,
		"SetPushStrength", &CCharacterController::SetPushStrength,
		"GetRealVelocity", &CCharacterController::GetRealVelocity,
		"GetGroundState", &CCharacterController::GetGroundState,
		"IsGrounded", &CCharacterController::IsGrounded,
		"GetGroundNormal", &CCharacterController::GetGroundNormal,
		"GetGroundPoint", &CCharacterController::GetGroundPoint,
		"GetGroundVelocity", &CCharacterController::GetGroundVelocity,
		"GetGroundObject", &CCharacterController::GetGroundObject
	);

	p_luaState.new_enum<OvRendering::Settings::EProjectionMode>("ProjectionMode", {
		{"ORTHOGRAPHIC", OvRendering::Settings::EProjectionMode::ORTHOGRAPHIC},
		{"PERSPECTIVE", OvRendering::Settings::EProjectionMode::PERSPECTIVE}
		});

	p_luaState.new_usertype<CCamera>("Camera",
		sol::base_classes, sol::bases<AComponent>(),
		"GetFov", &CCamera::GetFov,
		"GetSize", &CCamera::GetSize,
		"GetNear", &CCamera::GetNear,
		"GetFar", &CCamera::GetFar,
		"GetClearColor", &CCamera::GetClearColor,
		"SetFov", &CCamera::SetFov,
		"SetSize", &CCamera::SetSize,
		"SetNear", &CCamera::SetNear,
		"SetFar", &CCamera::SetFar,
		"SetClearColor", &CCamera::SetClearColor,
		"HasFrustumGeometryCulling", &CCamera::HasFrustumGeometryCulling,
		"HasFrustumLightCulling", &CCamera::HasFrustumLightCulling,
		"HasDepthPrePass", &CCamera::HasDepthPrePass,
		"GetProjectionMode", &CCamera::GetProjectionMode,
		"SetFrustumGeometryCulling", &CCamera::SetFrustumGeometryCulling,
		"SetFrustumLightCulling", &CCamera::SetFrustumLightCulling,
		"SetDepthPrePass", &CCamera::SetDepthPrePass,
		"SetProjectionMode", &CCamera::SetProjectionMode
	);

	p_luaState.new_enum<UI::CCanvas::EScalerMode>("CanvasScalerMode", {
		{"CONSTANT_PIXEL_SIZE", UI::CCanvas::EScalerMode::CONSTANT_PIXEL_SIZE},
		{"SCALE_WITH_SCREEN_SIZE", UI::CCanvas::EScalerMode::SCALE_WITH_SCREEN_SIZE}
	});

	p_luaState.new_enum<UI::CCanvas::EScreenMatchMode>("CanvasScreenMatchMode", {
		{"MATCH_WIDTH_OR_HEIGHT", UI::CCanvas::EScreenMatchMode::MATCH_WIDTH_OR_HEIGHT},
		{"EXPAND", UI::CCanvas::EScreenMatchMode::EXPAND},
		{"SHRINK", UI::CCanvas::EScreenMatchMode::SHRINK}
	});

	p_luaState.new_enum<UI::CCanvas::ERenderMode>("CanvasRenderMode", {
		{"SCREEN_SPACE", UI::CCanvas::ERenderMode::SCREEN_SPACE},
		{"WORLD_SPACE", UI::CCanvas::ERenderMode::WORLD_SPACE}
	});

	p_luaState.new_enum<CTransform::EUIAnchorPreset>("AnchorPreset", {
		{"TOP_LEFT", CTransform::EUIAnchorPreset::TOP_LEFT},
		{"TOP_CENTER", CTransform::EUIAnchorPreset::TOP_CENTER},
		{"TOP_RIGHT", CTransform::EUIAnchorPreset::TOP_RIGHT},
		{"MIDDLE_LEFT", CTransform::EUIAnchorPreset::MIDDLE_LEFT},
		{"CENTER", CTransform::EUIAnchorPreset::CENTER},
		{"MIDDLE_RIGHT", CTransform::EUIAnchorPreset::MIDDLE_RIGHT},
		{"BOTTOM_LEFT", CTransform::EUIAnchorPreset::BOTTOM_LEFT},
		{"BOTTOM_CENTER", CTransform::EUIAnchorPreset::BOTTOM_CENTER},
		{"BOTTOM_RIGHT", CTransform::EUIAnchorPreset::BOTTOM_RIGHT},
		{"HORIZONTAL_STRETCH_TOP", CTransform::EUIAnchorPreset::HORIZONTAL_STRETCH_TOP},
		{"HORIZONTAL_STRETCH_MIDDLE", CTransform::EUIAnchorPreset::HORIZONTAL_STRETCH_MIDDLE},
		{"HORIZONTAL_STRETCH_BOTTOM", CTransform::EUIAnchorPreset::HORIZONTAL_STRETCH_BOTTOM},
		{"VERTICAL_STRETCH_LEFT", CTransform::EUIAnchorPreset::VERTICAL_STRETCH_LEFT},
		{"VERTICAL_STRETCH_CENTER", CTransform::EUIAnchorPreset::VERTICAL_STRETCH_CENTER},
		{"VERTICAL_STRETCH_RIGHT", CTransform::EUIAnchorPreset::VERTICAL_STRETCH_RIGHT},
		{"STRETCH_BOTH", CTransform::EUIAnchorPreset::STRETCH_BOTH}
	});

	p_luaState.new_enum<UI::CLayoutGroup::EDirection>("LayoutDirection", {
		{"HORIZONTAL", UI::CLayoutGroup::EDirection::HORIZONTAL},
		{"VERTICAL", UI::CLayoutGroup::EDirection::VERTICAL}
	});

	p_luaState.new_enum<UI::CLayoutGroup::EHorizontalAlignment>("LayoutHorizontalAlignment", {
		{"LEFT", UI::CLayoutGroup::EHorizontalAlignment::LEFT},
		{"CENTER", UI::CLayoutGroup::EHorizontalAlignment::CENTER},
		{"RIGHT", UI::CLayoutGroup::EHorizontalAlignment::RIGHT}
	});

	p_luaState.new_enum<UI::CLayoutGroup::EVerticalAlignment>("LayoutVerticalAlignment", {
		{"TOP", UI::CLayoutGroup::EVerticalAlignment::TOP},
		{"CENTER", UI::CLayoutGroup::EVerticalAlignment::CENTER},
		{"BOTTOM", UI::CLayoutGroup::EVerticalAlignment::BOTTOM}
	});

	p_luaState.new_enum<UI::CText::EHorizontalAlignment>("TextHorizontalAlignment", {
		{"LEFT", UI::CText::EHorizontalAlignment::LEFT},
		{"CENTER", UI::CText::EHorizontalAlignment::CENTER},
		{"RIGHT", UI::CText::EHorizontalAlignment::RIGHT}
	});

	p_luaState.new_enum<UI::CText::EVerticalAlignment>("TextVerticalAlignment", {
		{"TOP", UI::CText::EVerticalAlignment::TOP},
		{"CENTER", UI::CText::EVerticalAlignment::CENTER},
		{"BOTTOM", UI::CText::EVerticalAlignment::BOTTOM}
	});

	p_luaState.new_usertype<UI::CCanvas>("Canvas",
		sol::base_classes, sol::bases<AComponent>(),
		"GetReferenceResolution", [](UI::CCanvas& p_this) -> FVector2 { return p_this.GetReferenceResolution(); },
		"SetReferenceResolution", &UI::CCanvas::SetReferenceResolution,
		"GetScaleFactor", &UI::CCanvas::GetScaleFactor,
		"SetScaleFactor", &UI::CCanvas::SetScaleFactor,
		"GetScalerMode", &UI::CCanvas::GetScalerMode,
		"SetScalerMode", &UI::CCanvas::SetScalerMode,
		"GetScreenMatchMode", &UI::CCanvas::GetScreenMatchMode,
		"SetScreenMatchMode", &UI::CCanvas::SetScreenMatchMode,
		"GetMatchWidthOrHeight", &UI::CCanvas::GetMatchWidthOrHeight,
		"SetMatchWidthOrHeight", &UI::CCanvas::SetMatchWidthOrHeight,
		"GetRenderMode", &UI::CCanvas::GetRenderMode,
		"SetRenderMode", &UI::CCanvas::SetRenderMode,
		"GetWorldScale", &UI::CCanvas::GetWorldScale,
		"SetWorldScale", &UI::CCanvas::SetWorldScale
	);

	p_luaState.new_usertype<UI::CImage>("Image",
		sol::base_classes, sol::bases<AComponent>(),
		"GetTexture", &UI::CImage::GetTexture,
		"SetTexture", &UI::CImage::SetTexture,
		"GetSize", [](UI::CImage& p_this) -> FVector2 { return p_this.GetSize(); },
		"SetSize", &UI::CImage::SetSize,
		"GetTint", [](UI::CImage& p_this) -> FVector4 { return p_this.GetTint(); },
		"SetTint", &UI::CImage::SetTint,
		"GetPreserveAspect", &UI::CImage::GetPreserveAspect,
		"SetPreserveAspect", &UI::CImage::SetPreserveAspect
	);

	p_luaState.new_usertype<UI::CLayoutGroup>("LayoutGroup",
		sol::base_classes, sol::bases<AComponent>(),
		"GetDirection", &UI::CLayoutGroup::GetDirection,
		"SetDirection", &UI::CLayoutGroup::SetDirection,
		"GetSpacing", &UI::CLayoutGroup::GetSpacing,
		"SetSpacing", &UI::CLayoutGroup::SetSpacing,
		"GetPadding", [](UI::CLayoutGroup& p_this) -> FVector4 { return p_this.GetPadding(); },
		"SetPadding", &UI::CLayoutGroup::SetPadding,
		"GetHorizontalAlignment", &UI::CLayoutGroup::GetHorizontalAlignment,
		"SetHorizontalAlignment", &UI::CLayoutGroup::SetHorizontalAlignment,
		"GetVerticalAlignment", &UI::CLayoutGroup::GetVerticalAlignment,
		"SetVerticalAlignment", &UI::CLayoutGroup::SetVerticalAlignment,
		"GetControlChildrenWidth", &UI::CLayoutGroup::GetControlChildrenWidth,
		"SetControlChildrenWidth", &UI::CLayoutGroup::SetControlChildrenWidth,
		"GetControlChildrenHeight", &UI::CLayoutGroup::GetControlChildrenHeight,
		"SetControlChildrenHeight", &UI::CLayoutGroup::SetControlChildrenHeight,
		"GetForceExpandWidth", &UI::CLayoutGroup::GetForceExpandWidth,
		"SetForceExpandWidth", &UI::CLayoutGroup::SetForceExpandWidth,
		"GetForceExpandHeight", &UI::CLayoutGroup::GetForceExpandHeight,
		"SetForceExpandHeight", &UI::CLayoutGroup::SetForceExpandHeight
	);

	p_luaState.new_usertype<UI::CHorizontalLayout>("HorizontalLayout",
		sol::base_classes, sol::bases<AComponent, UI::CLayoutGroup>(),
		"GetSpacing", &UI::CHorizontalLayout::GetSpacing,
		"SetSpacing", &UI::CHorizontalLayout::SetSpacing,
		"GetPadding", [](UI::CHorizontalLayout& p_this) -> FVector4 { return p_this.GetPadding(); },
		"SetPadding", &UI::CHorizontalLayout::SetPadding,
		"GetHorizontalAlignment", &UI::CHorizontalLayout::GetHorizontalAlignment,
		"SetHorizontalAlignment", &UI::CHorizontalLayout::SetHorizontalAlignment,
		"GetVerticalAlignment", &UI::CHorizontalLayout::GetVerticalAlignment,
		"SetVerticalAlignment", &UI::CHorizontalLayout::SetVerticalAlignment,
		"GetControlChildrenWidth", &UI::CHorizontalLayout::GetControlChildrenWidth,
		"SetControlChildrenWidth", &UI::CHorizontalLayout::SetControlChildrenWidth,
		"GetControlChildrenHeight", &UI::CHorizontalLayout::GetControlChildrenHeight,
		"SetControlChildrenHeight", &UI::CHorizontalLayout::SetControlChildrenHeight,
		"GetForceExpandWidth", &UI::CHorizontalLayout::GetForceExpandWidth,
		"SetForceExpandWidth", &UI::CHorizontalLayout::SetForceExpandWidth,
		"GetForceExpandHeight", &UI::CHorizontalLayout::GetForceExpandHeight,
		"SetForceExpandHeight", &UI::CHorizontalLayout::SetForceExpandHeight
	);

	p_luaState.new_usertype<UI::CVerticalLayout>("VerticalLayout",
		sol::base_classes, sol::bases<AComponent, UI::CLayoutGroup>(),
		"GetSpacing", &UI::CVerticalLayout::GetSpacing,
		"SetSpacing", &UI::CVerticalLayout::SetSpacing,
		"GetPadding", [](UI::CVerticalLayout& p_this) -> FVector4 { return p_this.GetPadding(); },
		"SetPadding", &UI::CVerticalLayout::SetPadding,
		"GetHorizontalAlignment", &UI::CVerticalLayout::GetHorizontalAlignment,
		"SetHorizontalAlignment", &UI::CVerticalLayout::SetHorizontalAlignment,
		"GetVerticalAlignment", &UI::CVerticalLayout::GetVerticalAlignment,
		"SetVerticalAlignment", &UI::CVerticalLayout::SetVerticalAlignment,
		"GetControlChildrenWidth", &UI::CVerticalLayout::GetControlChildrenWidth,
		"SetControlChildrenWidth", &UI::CVerticalLayout::SetControlChildrenWidth,
		"GetControlChildrenHeight", &UI::CVerticalLayout::GetControlChildrenHeight,
		"SetControlChildrenHeight", &UI::CVerticalLayout::SetControlChildrenHeight,
		"GetForceExpandWidth", &UI::CVerticalLayout::GetForceExpandWidth,
		"SetForceExpandWidth", &UI::CVerticalLayout::SetForceExpandWidth,
		"GetForceExpandHeight", &UI::CVerticalLayout::GetForceExpandHeight,
		"SetForceExpandHeight", &UI::CVerticalLayout::SetForceExpandHeight
	);

	p_luaState.new_usertype<UI::CText>("Text",
		sol::base_classes, sol::bases<AComponent>(),
		"GetText", &UI::CText::GetText,
		"SetText", &UI::CText::SetText,
		"GetContentSize", &UI::CText::GetContentSize,
		"GetFontPath", &UI::CText::GetFontPath,
		"SetFontPath", &UI::CText::SetFontPath,
		"GetFontSize", &UI::CText::GetFontSize,
		"SetFontSize", &UI::CText::SetFontSize,
		"GetColor", [](UI::CText& p_this) -> FVector4 { return p_this.GetColor(); },
		"SetColor", &UI::CText::SetColor,
		"GetHorizontalAlignment", &UI::CText::GetHorizontalAlignment,
		"SetHorizontalAlignment", &UI::CText::SetHorizontalAlignment,
		"GetVerticalAlignment", &UI::CText::GetVerticalAlignment,
		"SetVerticalAlignment", &UI::CText::SetVerticalAlignment
	);

	p_luaState.new_usertype<CLight>("Light",
		sol::base_classes, sol::bases<AComponent>(),
		"GetColor", &CPointLight::GetColor,
		"GetIntensity", &CPointLight::GetIntensity,
		"SetColor", &CPointLight::SetColor,
		"SetIntensity", &CPointLight::SetIntensity
	);

	p_luaState.new_usertype<CPointLight>("PointLight",
		sol::base_classes, sol::bases<CLight, AComponent>(),
		"GetConstant", &CPointLight::GetConstant,
		"GetLinear", &CPointLight::GetLinear,
		"GetQuadratic", &CPointLight::GetQuadratic,
		"SetConstant", &CPointLight::SetConstant,
		"SetLinear", &CPointLight::SetLinear,
		"SetQuadratic", &CPointLight::SetQuadratic
	);

	p_luaState.new_usertype<CSpotLight>("SpotLight",
		sol::base_classes, sol::bases<CLight, AComponent>(),
		"GetConstant", &CSpotLight::GetConstant,
		"GetLinear", &CSpotLight::GetLinear,
		"GetQuadratic", &CSpotLight::GetQuadratic,
		"GetCutOff", &CSpotLight::GetCutoff,
		"GetOuterCutOff", &CSpotLight::GetOuterCutoff,
		"SetConstant", &CSpotLight::SetConstant,
		"SetLinear", &CSpotLight::SetLinear,
		"SetQuadratic", &CSpotLight::SetQuadratic,
		"SetCutOff", &CSpotLight::SetCutoff,
		"SetOuterCutOff", &CSpotLight::SetOuterCutoff
	);

	p_luaState.new_usertype<CAmbientBoxLight>("AmbientBoxLight",
		sol::base_classes, sol::bases<CLight, AComponent>(),
		"GetSize", &CAmbientBoxLight::GetSize,
		"SetSize", &CAmbientBoxLight::SetSize
	);

	p_luaState.new_usertype<CAmbientSphereLight>("AmbientSphereLight",
		sol::base_classes, sol::bases<CLight, AComponent>(),
		"GetRadius", &CAmbientSphereLight::GetRadius,
		"SetRadius", &CAmbientSphereLight::SetRadius
	);

	p_luaState.new_enum<OvRendering::Settings::EShadowMapUpdateMode>("ShadowMapUpdateMode", {
		{ "REALTIME", OvRendering::Settings::EShadowMapUpdateMode::REALTIME },
		{ "ON_CHANGE", OvRendering::Settings::EShadowMapUpdateMode::ON_CHANGE }
	});

	p_luaState.new_usertype<CDirectionalLight>("DirectionalLight",
		sol::base_classes, sol::bases<CLight, AComponent>(),
		"GetCastShadow", &CDirectionalLight::GetCastShadows,
		"SetCastShadow", &CDirectionalLight::SetCastShadows,
		"GetShadowAreaSize", &CDirectionalLight::GetShadowAreaSize,
		"SetShadowAreaSize", &CDirectionalLight::SetShadowAreaSize,
		"GetShadowFollowCamera", &CDirectionalLight::GetShadowFollowCamera,
		"SetShadowFollowCamera", &CDirectionalLight::SetShadowFollowCamera,
		"GetShadowMapResolution", &CDirectionalLight::GetShadowMapResolution,
		"SetShadowMapResolution", &CDirectionalLight::SetShadowMapResolution,
		"GetShadowMapUpdateMode", &CDirectionalLight::GetShadowMapUpdateMode,
		"SetShadowMapUpdateMode", &CDirectionalLight::SetShadowMapUpdateMode,
		"RequestShadowMapUpdate", &CDirectionalLight::RequestShadowMapUpdate
	);

	p_luaState.new_usertype<CAudioSource>("AudioSource",
		sol::base_classes, sol::bases<AComponent>(),
		"Play", &CAudioSource::Play,
		"Stop", &CAudioSource::Stop,
		"Pause", &CAudioSource::Pause,
		"Resume", &CAudioSource::Resume,
		"GetSound", &CAudioSource::GetSound,
		"GetVolume", &CAudioSource::GetVolume,
		"GetPan", &CAudioSource::GetPan,
		"IsLooped", &CAudioSource::IsLooped,
		"GetPitch", &CAudioSource::GetPitch,
		"IsPlaying", &CAudioSource::IsPlaying,
		"IsSpatial", &CAudioSource::IsSpatial,
		"GetAttenuationThreshold", &CAudioSource::GetAttenuationThreshold,
		"SetSound", &CAudioSource::SetSound,
		"SetVolume", &CAudioSource::SetVolume,
		"SetPan", &CAudioSource::SetPan,
		"SetLooped", &CAudioSource::SetLooped,
		"SetPitch", &CAudioSource::SetPitch,
		"SetSpatial", &CAudioSource::SetSpatial,
		"SetAttenuationThreshold", &CAudioSource::SetAttenuationThreshold
	);

	p_luaState.new_usertype<CAudioListener>("AudioListener",
		sol::base_classes, sol::bases<AComponent>()
	);

	p_luaState.new_usertype<OvCore::Rendering::PostProcess::EffectSettings>("EffectSettings",
		"Enabled", &OvCore::Rendering::PostProcess::EffectSettings::enabled
	);

	p_luaState.new_usertype<OvCore::Rendering::PostProcess::BloomSettings>("BloomSettings",
		sol::base_classes, sol::bases<OvCore::Rendering::PostProcess::EffectSettings>(),
		"Intensity", &OvCore::Rendering::PostProcess::BloomSettings::intensity,
		"Passes", &OvCore::Rendering::PostProcess::BloomSettings::passes
	);

	p_luaState.new_usertype<OvCore::Rendering::PostProcess::AutoExposureSettings>("AutoExposureSettings",
		sol::base_classes, sol::bases<OvCore::Rendering::PostProcess::EffectSettings>(),
		"CenterWeightBias", &OvCore::Rendering::PostProcess::AutoExposureSettings::centerWeightBias,
		"MinLuminanceEV", &OvCore::Rendering::PostProcess::AutoExposureSettings::minLuminanceEV,
		"MaxLuminanceEV", &OvCore::Rendering::PostProcess::AutoExposureSettings::maxLuminanceEV,
		"ExposureCompensationEV", &OvCore::Rendering::PostProcess::AutoExposureSettings::exposureCompensationEV,
		"Progressive", &OvCore::Rendering::PostProcess::AutoExposureSettings::progressive,
		"SpeedDown", &OvCore::Rendering::PostProcess::AutoExposureSettings::speedDown,
		"SpeedUp", &OvCore::Rendering::PostProcess::AutoExposureSettings::speedUp
	);

	p_luaState.new_enum<OvCore::Rendering::PostProcess::ETonemappingMode>("TonemappingMode", {
		{"NEUTRAL", OvCore::Rendering::PostProcess::ETonemappingMode::NEUTRAL},
		{"REINHARD", OvCore::Rendering::PostProcess::ETonemappingMode::REINHARD},
		{"REINHARD_JODIE", OvCore::Rendering::PostProcess::ETonemappingMode::REINHARD_JODIE},
		{"UNCHARTED2", OvCore::Rendering::PostProcess::ETonemappingMode::UNCHARTED2},
		{"UNCHARTED2_FILMIC", OvCore::Rendering::PostProcess::ETonemappingMode::UNCHARTED2_FILMIC},
		{"ACES", OvCore::Rendering::PostProcess::ETonemappingMode::ACES}
		});

	p_luaState.new_usertype<OvCore::Rendering::PostProcess::TonemappingSettings>("TonemappingSettings",
		sol::base_classes, sol::bases<OvCore::Rendering::PostProcess::EffectSettings>(),
		"Exposure", &OvCore::Rendering::PostProcess::TonemappingSettings::exposure,
		"Mode", &OvCore::Rendering::PostProcess::TonemappingSettings::mode,
		"GammaCorrection", &OvCore::Rendering::PostProcess::TonemappingSettings::gammaCorrection,
		"Temperature", &OvCore::Rendering::PostProcess::TonemappingSettings::temperature,
		"Tint", &OvCore::Rendering::PostProcess::TonemappingSettings::tint,
		"Saturation", &OvCore::Rendering::PostProcess::TonemappingSettings::saturation,
		"Contrast", &OvCore::Rendering::PostProcess::TonemappingSettings::contrast,
		"VignetteIntensity", &OvCore::Rendering::PostProcess::TonemappingSettings::vignetteIntensity,
		"VignetteSmoothness", &OvCore::Rendering::PostProcess::TonemappingSettings::vignetteSmoothness
	);

	p_luaState.new_usertype<OvCore::Rendering::PostProcess::FXAASettings>("FXAASettings",
		sol::base_classes, sol::bases<OvCore::Rendering::PostProcess::EffectSettings>()
	);

	p_luaState.new_usertype<OvCore::Rendering::FogSettings>("FogSettings",
		"Enabled", &OvCore::Rendering::FogSettings::enabled,
		"Color", &OvCore::Rendering::FogSettings::color,
		"Intensity", &OvCore::Rendering::FogSettings::intensity,
		"Start", &OvCore::Rendering::FogSettings::start,
		"Density", &OvCore::Rendering::FogSettings::density,
		"MaxOpacity", &OvCore::Rendering::FogSettings::maxOpacity
	);

	p_luaState.new_usertype<CPostProcessStack>("PostProcessStack",
		sol::base_classes, sol::bases<AComponent>(),
		"GetTonemappingSettings", &CPostProcessStack::GetTonemappingSettings,
		"GetBloomSettings", &CPostProcessStack::GetBloomSettings,
		"GetAutoExposureSettings", &CPostProcessStack::GetAutoExposureSettings,
		"GetFXAASettings", &CPostProcessStack::GetFXAASettings,
		"SetTonemappingSettings", &CPostProcessStack::SetTonemappingSettings,
		"SetBloomSettings", &CPostProcessStack::SetBloomSettings,
		"SetAutoExposureSettings", &CPostProcessStack::SetAutoExposureSettings,
		"SetFXAASettings", &CPostProcessStack::SetFXAASettings,
		"GetFogSettings", &CPostProcessStack::GetFogSettings,
		"SetFogSettings", &CPostProcessStack::SetFogSettings
	);

	p_luaState.new_enum<CReflectionProbe::ERefreshMode>("ReflectionProbeRefreshMode", {
		{ "REALTIME", CReflectionProbe::ERefreshMode::REALTIME },
		{ "ONCE", CReflectionProbe::ERefreshMode::ONCE },
		{ "ON_DEMAND", CReflectionProbe::ERefreshMode::ON_DEMAND }
	});

	p_luaState.new_enum<CReflectionProbe::ECaptureSpeed>("ReflectionProbeCaptureSpeed", {
		{ "ONE_FACE", CReflectionProbe::ECaptureSpeed::ONE_FACE },
		{ "TWO_FACES", CReflectionProbe::ECaptureSpeed::TWO_FACES },
		{ "THREE_FACES", CReflectionProbe::ECaptureSpeed::THREE_FACES },
		{ "SIX_FACES", CReflectionProbe::ECaptureSpeed::SIX_FACES }
	});

	p_luaState.new_enum<CReflectionProbe::EInfluencePolicy>("ReflectionProbeInfluencePolicy", {
		{ "GLOBAL", CReflectionProbe::EInfluencePolicy::GLOBAL },
		{ "LOCAL", CReflectionProbe::EInfluencePolicy::LOCAL }
	});

	// Add bindings for the new CReflectionProbe component
	p_luaState.new_usertype<CReflectionProbe>("ReflectionProbe",
		sol::base_classes, sol::bases<AComponent>(),
		"SetRefreshMode", &CReflectionProbe::SetRefreshMode,
		"GetRefreshMode", &CReflectionProbe::GetRefreshMode,
		"SetCaptureSpeed", &CReflectionProbe::SetCaptureSpeed,
		"GetCaptureSpeed", &CReflectionProbe::GetCaptureSpeed,
		"SetCubemapResolution", &CReflectionProbe::SetCubemapResolution,
		"GetCubemapResolution", &CReflectionProbe::GetCubemapResolution,
		"SetCapturePosition", &CReflectionProbe::SetCapturePosition,
		"GetCapturePosition", &CReflectionProbe::GetCapturePosition,
		"SetInfluencePolicy", &CReflectionProbe::SetInfluencePolicy,
		"GetInfluencePolicy", &CReflectionProbe::GetInfluencePolicy,
		"SetInfluenceSize", &CReflectionProbe::SetInfluenceSize,
		"GetInfluenceSize", &CReflectionProbe::GetInfluenceSize,
		"SetBoxProjection", &CReflectionProbe::SetBoxProjection,
		"IsBoxProjectionEnabled", &CReflectionProbe::IsBoxProjectionEnabled,
		"RequestCapture", &CReflectionProbe::RequestCapture,
		"GetCubemap", &CReflectionProbe::GetCubemap
	);

	p_luaState.new_enum<OvCore::Particles::EParticleShape>("ParticleShape", {
		{ "POINT", OvCore::Particles::EParticleShape::POINT },
		{ "SPHERE", OvCore::Particles::EParticleShape::SPHERE },
		{ "HEMISPHERE", OvCore::Particles::EParticleShape::HEMISPHERE },
		{ "CONE", OvCore::Particles::EParticleShape::CONE },
		{ "BOX", OvCore::Particles::EParticleShape::BOX }
	});

	p_luaState.new_enum<OvCore::Particles::EParticleSpace>("ParticleSpace", {
		{ "WORLD", OvCore::Particles::EParticleSpace::WORLD },
		{ "LOCAL", OvCore::Particles::EParticleSpace::LOCAL }
	});

	p_luaState.new_usertype<CParticleSystem>("ParticleSystem",
		sol::base_classes, sol::bases<AComponent>(),
		"Play", &CParticleSystem::Play,
		"Stop", &CParticleSystem::Stop,
		"Clear", &CParticleSystem::Clear,
		"IsPlaying", &CParticleSystem::IsPlaying,
		"GetAliveCount", &CParticleSystem::GetAliveCount,
		"Emit", [](CParticleSystem& p_this, double p_count, sol::optional<sol::table> p_overrides) {
			const uint32_t count = ToParticleCount(p_count, "Emit");
			return p_this.Emit(count, p_overrides ? ToParticleOverrides(*p_overrides) : OvCore::Particles::ParticleOverrides{});
		},
		"GetModel", &CParticleSystem::GetModel,
		"SetModel", [](CParticleSystem& p_this, OvRendering::Resources::Model* p_model) { p_this.SetModel(p_model); },
		"GetMaterial", &CParticleSystem::GetMaterial,
		"SetMaterial", [](CParticleSystem& p_this, OvCore::Resources::Material* p_material) { p_this.SetMaterial(p_material); },
		"GetMaxParticles", [](CParticleSystem& p_this) { return p_this.GetSettings().maxParticles; },
		"SetMaxParticles", [](CParticleSystem& p_this, double p_count) {
			const uint32_t count = ToParticleCount(p_count, "SetMaxParticles");
			ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.maxParticles = count; });
		},
		"GetRate", [](CParticleSystem& p_this) { return p_this.GetSettings().rate; },
		"SetRate", [](CParticleSystem& p_this, float p_rate) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.rate = p_rate; }); },
		"GetBursts", [](CParticleSystem& p_this, sol::this_state p_state) {
			sol::state_view lua(p_state);
			sol::table bursts = lua.create_table();
			for (const auto& burst : p_this.GetSettings().bursts)
			{
				bursts.add(lua.create_table_with("time", burst.time, "count", burst.count));
			}
			return bursts;
		},
		"SetBursts", [](CParticleSystem& p_this, sol::table p_bursts) {
			std::vector<OvCore::Particles::ParticleBurst> bursts;
			for (size_t i = 1; i <= p_bursts.size(); ++i)
			{
				const sol::optional<sol::table> entry = p_bursts[i];
				const sol::optional<float> time = entry ? (*entry)["time"] : sol::optional<float>{};
				const sol::optional<double> count = entry ? (*entry)["count"] : sol::optional<double>{};
				if (!time || !count)
				{
					throw sol::error(std::format("ParticleSystem:SetBursts: burst {} must be a table {{ time = seconds, count = particles }}", i));
				}
				bursts.push_back({ *time, ToParticleCount(*count, "SetBursts") });
			}
			ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.bursts = std::move(bursts); });
		},
		"GetDuration", [](CParticleSystem& p_this) { return p_this.GetSettings().duration; },
		"SetDuration", [](CParticleSystem& p_this, float p_value) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.duration = p_value; }); },
		"IsLooping", [](CParticleSystem& p_this) { return p_this.GetSettings().looping; },
		"SetLooping", [](CParticleSystem& p_this, bool p_value) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.looping = p_value; }); },
		"GetPlayOnStart", [](CParticleSystem& p_this) { return p_this.GetSettings().playOnStart; },
		"SetPlayOnStart", [](CParticleSystem& p_this, bool p_value) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.playOnStart = p_value; }); },
		"GetLifetime", [](CParticleSystem& p_this) { return std::make_tuple(p_this.GetSettings().lifetimeMin, p_this.GetSettings().lifetimeMax); },
		"SetLifetime", [](CParticleSystem& p_this, float p_min, sol::optional<float> p_max) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.lifetimeMin = p_min; s.lifetimeMax = p_max.value_or(p_min); }); },
		"GetSpeed", [](CParticleSystem& p_this) { return std::make_tuple(p_this.GetSettings().speedMin, p_this.GetSettings().speedMax); },
		"SetSpeed", [](CParticleSystem& p_this, float p_min, sol::optional<float> p_max) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.speedMin = p_min; s.speedMax = p_max.value_or(p_min); }); },
		"GetShape", [](CParticleSystem& p_this) { return p_this.GetSettings().shape; },
		"SetShape", [](CParticleSystem& p_this, OvCore::Particles::EParticleShape p_value) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.shape = p_value; }); },
		"GetShapeRadius", [](CParticleSystem& p_this) { return p_this.GetSettings().shapeRadius; },
		"SetShapeRadius", [](CParticleSystem& p_this, float p_value) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.shapeRadius = p_value; }); },
		"GetShapeAngle", [](CParticleSystem& p_this) { return p_this.GetSettings().shapeAngle; },
		"SetShapeAngle", [](CParticleSystem& p_this, float p_value) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.shapeAngle = p_value; }); },
		"GetShapeSize", [](CParticleSystem& p_this) { return p_this.GetSettings().shapeSize; },
		"SetShapeSize", [](CParticleSystem& p_this, const OvMaths::FVector3& p_value) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.shapeSize = p_value; }); },
		"GetGravity", [](CParticleSystem& p_this) { return p_this.GetSettings().gravity; },
		"SetGravity", [](CParticleSystem& p_this, const OvMaths::FVector3& p_value) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.gravity = p_value; }); },
		"GetDrag", [](CParticleSystem& p_this) { return p_this.GetSettings().drag; },
		"SetDrag", [](CParticleSystem& p_this, float p_value) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.drag = p_value; }); },
		"GetSize", [](CParticleSystem& p_this) { return std::make_tuple(p_this.GetSettings().sizeStart, p_this.GetSettings().sizeEnd); },
		"SetSize", [](CParticleSystem& p_this, float p_start, sol::optional<float> p_end) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.sizeStart = p_start; s.sizeEnd = p_end.value_or(p_start); }); },
		"GetColor", [](CParticleSystem& p_this) { return std::make_tuple(p_this.GetSettings().colorStart, p_this.GetSettings().colorEnd); },
		"SetColor", [](CParticleSystem& p_this, const OvMaths::FVector4& p_start, sol::optional<OvMaths::FVector4> p_end) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.colorStart = p_start; s.colorEnd = p_end.value_or(p_start); }); },
		"GetFade", [](CParticleSystem& p_this) { return std::make_tuple(p_this.GetSettings().fadeIn, p_this.GetSettings().fadeOut); },
		"SetFade", [](CParticleSystem& p_this, float p_in, float p_out) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.fadeIn = p_in; s.fadeOut = p_out; }); },
		"GetSpin", [](CParticleSystem& p_this) { return std::make_tuple(p_this.GetSettings().spinMin, p_this.GetSettings().spinMax); },
		"SetSpin", [](CParticleSystem& p_this, float p_min, sol::optional<float> p_max) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.spinMin = p_min; s.spinMax = p_max.value_or(p_min); }); },
		"HasRandomRotation", [](CParticleSystem& p_this) { return p_this.GetSettings().randomRotation; },
		"SetRandomRotation", [](CParticleSystem& p_this, bool p_value) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.randomRotation = p_value; }); },
		"GetSpace", [](CParticleSystem& p_this) { return p_this.GetSettings().space; },
		"SetSpace", [](CParticleSystem& p_this, OvCore::Particles::EParticleSpace p_value) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.space = p_value; }); },
		"IsUnscaled", [](CParticleSystem& p_this) { return p_this.GetSettings().unscaled; },
		"SetUnscaled", [](CParticleSystem& p_this, bool p_value) { ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.unscaled = p_value; }); },
		"GetSeed", [](CParticleSystem& p_this) { return p_this.GetSettings().seed; },
		"SetSeed", [](CParticleSystem& p_this, double p_seed) {
			if (!std::isfinite(p_seed) || p_seed < 0.0 || std::floor(p_seed) != p_seed || p_seed > std::numeric_limits<uint32_t>::max())
			{
				throw sol::error("ParticleSystem:SetSeed takes a whole number from 0 to 4294967295");
			}
			ChangeParticleSettings(p_this, [&](ParticleSettings& s) { s.seed = static_cast<uint32_t>(p_seed); });
		}
	);

	OvCore::Scripting::MarkComponentUsertypes<
		AComponent, CTransform, CModelRenderer, CMaterialRenderer, CBoneAttachment, CSkinnedMeshRenderer,
		CPhysicalObject, CPhysicalBox, CPhysicalSphere, CPhysicalCapsule, CPhysicalHeightField, CCharacterController,
		CCamera, UI::CCanvas, UI::CImage, UI::CLayoutGroup, UI::CHorizontalLayout, UI::CVerticalLayout, UI::CText,
		CLight, CPointLight, CSpotLight, CAmbientBoxLight, CAmbientSphereLight, CDirectionalLight,
		CAudioSource, CAudioListener, CPostProcessStack, CReflectionProbe, CParticleSystem
	>(p_luaState.lua_state());
}
