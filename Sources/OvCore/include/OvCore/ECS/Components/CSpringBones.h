/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <OvCore/Animation/SpringSolver.h>

#include "AComponent.h"

namespace OvCore::SceneSystem { class Scene; }
namespace OvRendering::Resources { class Model; }
namespace OvUI::Widgets::Layout { class Group; }

namespace OvCore::ECS::Components
{
	/**
	* Swings chains of bones of its actor's skinned mesh (hair, a cape, a tail) as the body moves, over the animation
	* (see Animation::SpringSolver). Chains are put in groups, each with its settings and the colliders it meets; a
	* chain starts at a bone and runs down its only child bone after bone. Colliders are capsules on the body's bones.
	* The ground can be a plane at the actor's height. Applied by the scene once a frame, after every script and every
	* look at, before the bone attachments. A model no render pass drew rests and does no work; back in view, or after a
	* jump further than a run would make (a teleport, a respawn), its chains start again from rest.
	*/
	class CSpringBones : public AComponent
	{
	public:
		/**
		* A capsule on the body: from one bone to another, or from a bone outwards, away from another, between two
		* distances past it (the head: from the head, away from the chest)
		*/
		struct Collider
		{
			std::string name;
			std::string from;
			std::string to;				// The capsule's other end, when set
			std::string away;			// Else, the bone it points away from
			float start = 0.0f;			// Where it starts past `from`, metres (away only)
			float end = 0.0f;			// Where it ends past `from`, metres (away only)
			float radius = 0.05f;
		};

		/**
		* Chains on one setting, side by side, and the names of the colliders they meet
		*/
		struct Group
		{
			std::string name;
			Animation::SpringSettings settings;
			std::vector<std::string> chains;
			std::vector<std::string> colliders;
		};

		/**
		* Constructor
		* @param p_owner
		*/
		CSpringBones(ECS::Actor& p_owner);

		/**
		* Returns the name of the component
		*/
		std::string GetName() override;

		/**
		* Returns the type name of the component
		*/
		virtual std::string GetTypeName() override;

		/**
		* Adds a group, or replaces the settings of the group of that name
		* @param p_name
		* @param p_settings
		*/
		void SetGroup(const std::string& p_name, const Animation::SpringSettings& p_settings);

		/**
		* Removes a group, returning false when there is none of that name
		* @param p_name
		*/
		bool RemoveGroup(const std::string& p_name);

		/**
		* Returns the groups
		*/
		const std::vector<Group>& GetGroups() const;

		/**
		* Returns a group by name
		* @param p_name
		*/
		const Group* FindGroup(const std::string& p_name) const;

		/**
		* Sets the chains of a group, each named by the bone it starts at, side by side in that order. Returns false
		* when there is no group of that name
		* @param p_group
		* @param p_chains
		*/
		bool SetGroupChains(const std::string& p_group, const std::vector<std::string>& p_chains);

		/**
		* Sets the colliders a group meets, by name. Returns false when there is no group of that name
		* @param p_group
		* @param p_colliders
		*/
		bool SetGroupColliders(const std::string& p_group, const std::vector<std::string>& p_colliders);

		/**
		* Adds a collider, or replaces the collider of that name
		* @param p_collider
		*/
		void SetCollider(const Collider& p_collider);

		/**
		* Removes a collider, returning false when there is none of that name
		* @param p_name
		*/
		bool RemoveCollider(const std::string& p_name);

		/**
		* Returns the colliders
		*/
		const std::vector<Collider>& GetColliders() const;

		/**
		* Sets whether the chains stay above a ground at the actor's height plus an offset, metres
		* @param p_enabled
		* @param p_offset
		*/
		void SetGround(bool p_enabled, float p_offset = 0.0f);

		/**
		* Returns true when the chains stay above the ground
		*/
		bool IsGroundEnabled() const;

		/**
		* Returns the ground's offset from the actor's height
		*/
		float GetGroundOffset() const;

		/**
		* Sets whether a model no render pass drew the frame before rests (true by default)
		* @param p_value
		*/
		void SetSleepWhenHidden(bool p_value);

		/**
		* Returns true when a model no render pass drew rests
		*/
		bool GetSleepWhenHidden() const;

		/**
		* Puts the chains back at rest from the next frame
		*/
		void Reset();

		/**
		* Returns true while the chains rest because the model was not drawn
		*/
		bool IsAsleep() const;

		/**
		* Returns how many chains are simulated (0 until the scene first applies the component)
		*/
		uint32_t GetChainCount() const;

		/**
		* Returns the end of a chain's bone in the world, as simulated (nothing for an unknown chain or bone)
		* @param p_chain (over every group, in their order)
		* @param p_bone
		*/
		std::optional<OvMaths::FVector3> GetJointPosition(uint32_t p_chain, uint32_t p_bone) const;

		/**
		* Returns why a chain or a collider cannot be made (an unknown bone, a chain of one bone), or nothing
		*/
		const std::optional<std::string>& GetError() const;

		/**
		* Moves the chains on and turns their bones: called by the scene once a frame
		* @param p_scene
		* @param p_deltaTime
		*/
		void Apply(const SceneSystem::Scene& p_scene, float p_deltaTime);

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
		struct RuntimeCollider
		{
			uint32_t from = 0;
			uint32_t other = 0;
			bool away = false;
			float start = 0.0f;
			float end = 0.0f;
			float radius = 0.0f;
			Animation::SpringCapsule capsule;
		};

		struct RuntimeGroup
		{
			Animation::SpringGroup group;
			std::vector<std::vector<uint32_t>> bones;		// Each chain's bones, from its root
			std::vector<size_t> colliders;					// Into m_runtimeColliders
			std::vector<Animation::SpringCapsule> capsules;	// The frame's, gathered for the solver
		};

		void MarkDirty();
		bool Build(const class CSkinnedMeshRenderer& p_renderer);
		void BuildWidgets(OvUI::Internal::WidgetContainer& p_container);

	private:
		std::vector<Group> m_groups;
		std::vector<Collider> m_colliders;
		bool m_groundEnabled = false;
		float m_groundOffset = 0.0f;
		bool m_sleepWhenHidden = true;

		// Runtime, rebuilt when the settings or the model change
		bool m_dirty = true;
		const OvRendering::Resources::Model* m_builtModel = nullptr;
		std::vector<RuntimeGroup> m_runtimeGroups;
		std::vector<RuntimeCollider> m_runtimeColliders;
		bool m_resetPending = true;
		bool m_asleep = false;
		std::optional<std::string> m_error;
	};

	template<>
	struct ComponentTraits<OvCore::ECS::Components::CSpringBones>
	{
		static constexpr std::string_view Name = "class OvCore::ECS::Components::CSpringBones";
	};
}
