/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <optional>
#include <string>

#include <OvMaths/FQuaternion.h>
#include <OvMaths/FVector3.h>

#include "AComponent.h"

namespace OvCore::ECS { class Actor; }
namespace OvCore::SceneSystem { class Scene; }

namespace OvCore::ECS::Components
{
	class CSkinnedMeshRenderer;

	/**
	* Keeps its actor on a bone (or any node) of a skinned mesh: a weapon in a hand, a flame on a head. The mesh is the
	* skinned mesh renderer of the target actor when one is set, else of the nearest ancestor that has one. Every frame,
	* once every script has run, the scene places the actor at the bone's pose times the offset, in a single update of
	* its world transform: no script has to read the bone. Attachments are placed parents first, so one can ride on
	* another.
	*/
	class CBoneAttachment : public AComponent
	{
	public:
		/**
		* Constructor
		* @param p_owner
		*/
		CBoneAttachment(ECS::Actor& p_owner);

		/**
		* Returns the name of the component
		*/
		std::string GetName() override;

		/**
		* Returns the type name of the component
		*/
		virtual std::string GetTypeName() override;

		/**
		* Sets the bone (or node) to follow, by name
		* @param p_bone
		*/
		void SetBone(const std::string& p_bone);

		/**
		* Returns the name of the bone (or node) followed
		*/
		const std::string& GetBone() const;

		/**
		* Sets the offset from the bone, in the bone's space
		* @param p_position
		*/
		void SetOffsetPosition(const OvMaths::FVector3& p_position);

		/**
		* Returns the offset from the bone, in the bone's space
		*/
		const OvMaths::FVector3& GetOffsetPosition() const;

		/**
		* Sets the rotation applied after the bone's
		* @param p_rotation
		*/
		void SetOffsetRotation(const OvMaths::FQuaternion& p_rotation);

		/**
		* Returns the rotation applied after the bone's
		*/
		const OvMaths::FQuaternion& GetOffsetRotation() const;

		/**
		* Sets the scale applied after the bone's
		* @param p_scale
		*/
		void SetOffsetScale(const OvMaths::FVector3& p_scale);

		/**
		* Returns the scale applied after the bone's
		*/
		const OvMaths::FVector3& GetOffsetScale() const;

		/**
		* Sets the actor whose skinned mesh renderer holds the bone, instead of the nearest ancestor with one
		* @param p_target (nullptr: back to the nearest ancestor)
		*/
		void SetTarget(ECS::Actor* p_target);

		/**
		* Returns the GUID of the target actor, 0 when the nearest ancestor with a skinned mesh renderer is used
		*/
		uint64_t GetTargetGUID() const;

		/**
		* Returns true if the last placement found its skinned mesh and its bone
		*/
		bool IsAttached() const;

		/**
		* Places the actor on its bone. Called by the scene once every script has run.
		* @param p_scene
		*/
		void Apply(const SceneSystem::Scene& p_scene);

		/**
		* Serialize the component
		* @param p_doc
		* @param p_node
		*/
		virtual void OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node) override;

		/**
		* Deserialize the component
		* @param p_doc
		* @param p_node
		*/
		virtual void OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node) override;

		/**
		* Defines how the component should be drawn in the inspector
		* @param p_root
		*/
		virtual void OnInspector(OvUI::Internal::WidgetContainer& p_root) override;

	private:
		CSkinnedMeshRenderer* FindRenderer(const SceneSystem::Scene& p_scene) const;

	private:
		std::string m_bone;
		OvMaths::FVector3 m_offsetPosition = OvMaths::FVector3::Zero;
		OvMaths::FQuaternion m_offsetRotation = OvMaths::FQuaternion::Identity;
		OvMaths::FVector3 m_offsetScale = OvMaths::FVector3::One;
		uint64_t m_targetGUID = 0;
		bool m_attached = false;
	};

	template<>
	struct ComponentTraits<OvCore::ECS::Components::CBoneAttachment>
	{
		static constexpr std::string_view Name = "class OvCore::ECS::Components::CBoneAttachment";
	};
}
