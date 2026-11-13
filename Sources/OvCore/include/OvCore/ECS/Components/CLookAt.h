/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <OvCore/Animation/LookAtSolver.h>
#include <OvMaths/FVector3.h>

#include "AComponent.h"

namespace OvCore::SceneSystem { class Scene; }

namespace OvCore::ECS::Components
{
	/**
	* Turns a chain of bones of its actor's skinned mesh (the spine, the neck, the head) towards a target, over the
	* animation: a yaw and a pitch in the model's space (front +Z, up +Y), kept within bounds and following the target
	* on a critically damped spring. Each bone takes its part of the look, from the root to the tip, so that the tip
	* (the head) looks the whole way. While it has a target, the look holds its direction in the world as the body turns
	* under it; without one, it goes back to the front. Applied by the scene once a frame, after every script and before
	* the bone attachments, from the frame's animated pose.
	*/
	class CLookAt : public AComponent
	{
	public:
		/**
		* A bone and its part of the look
		*/
		struct BonePart
		{
			std::string bone;
			float weight = 1.0f;
		};

		/**
		* Constructor
		* @param p_owner
		*/
		CLookAt(ECS::Actor& p_owner);

		/**
		* Returns the name of the component
		*/
		std::string GetName() override;

		/**
		* Returns the type name of the component
		*/
		virtual std::string GetTypeName() override;

		/**
		* Sets the bones that turn and their parts of the look (taken over their sum), from the root to the tip
		* @param p_bones
		*/
		void SetBones(const std::vector<BonePart>& p_bones);

		/**
		* Returns the bones that turn and their parts of the look
		*/
		const std::vector<BonePart>& GetBones() const;

		/**
		* Looks at an actor's position (plus the target offset), or at nothing
		* @param p_target
		*/
		void SetTarget(ECS::Actor* p_target);

		/**
		* Returns the GUID of the actor looked at, or 0
		*/
		uint64_t GetTargetGUID() const;

		/**
		* Looks at a point in the world (plus the target offset), instead of an actor
		* @param p_position
		*/
		void SetTargetPosition(const OvMaths::FVector3& p_position);

		/**
		* Returns the point looked at when no actor is, if any
		*/
		std::optional<OvMaths::FVector3> GetTargetPosition() const;

		/**
		* Looks at nothing: the look goes back to the front
		*/
		void ClearTarget();

		/**
		* Returns true while there is something to look at
		*/
		bool HasTarget() const;

		/**
		* Sets an offset added to the target, world space (an actor's eyes above its feet)
		* @param p_offset
		*/
		void SetTargetOffset(const OvMaths::FVector3& p_offset);

		/**
		* Returns the target offset
		*/
		const OvMaths::FVector3& GetTargetOffset() const;

		/**
		* Sets the bounds and the speed of the look
		* @param p_settings
		*/
		void SetSettings(const Animation::LookAtSettings& p_settings);

		/**
		* Returns the bounds and the speed of the look
		*/
		const Animation::LookAtSettings& GetSettings() const;

		/**
		* Sets how much of the look applies (0 to 1), reached over the fade time
		* @param p_weight
		*/
		void SetWeight(float p_weight);

		/**
		* Returns the weight asked for
		*/
		float GetWeight() const;

		/**
		* Returns the weight applying now, on its way to the one asked for
		*/
		float GetCurrentWeight() const;

		/**
		* Sets how long the weight takes to go from 0 to 1, seconds
		* @param p_fadeTime
		*/
		void SetFadeTime(float p_fadeTime);

		/**
		* Returns how long the weight takes to go from 0 to 1, seconds
		*/
		float GetFadeTime() const;

		/**
		* Returns the look's yaw, degrees, positive towards the model's +X
		*/
		float GetYaw() const;

		/**
		* Returns the look's pitch, degrees, positive up
		*/
		float GetPitch() const;

		/**
		* Returns the direction looked in, world space (where the eyes point)
		*/
		OvMaths::FVector3 GetLookDirection() const;

		/**
		* Returns true when the look is at the front and still
		*/
		bool IsAtRest() const;

		/**
		* Puts the look back to the front at once
		*/
		void Reset();

		/**
		* Returns why the look cannot apply (no skinned mesh, an unknown bone, bones that are no chain), or nothing
		*/
		const std::optional<std::string>& GetError() const;

		/**
		* Moves the look on and turns the bones: called by the scene once a frame
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
		std::string FormatBones() const;
		void ParseBones(const std::string& p_text);

	private:
		std::vector<BonePart> m_bones;
		std::string m_bonesText; // As the inspector shows and edits it
		Animation::LookAtSettings m_settings;
		Animation::LookAtSolver m_solver;
		uint64_t m_targetGUID = 0;
		std::optional<OvMaths::FVector3> m_targetPosition;
		OvMaths::FVector3 m_targetOffset = OvMaths::FVector3::Zero;
		float m_weight = 1.0f;
		float m_currentWeight = 1.0f;
		float m_fadeTime = 0.3f;
		std::optional<std::string> m_error;
	};

	template<>
	struct ComponentTraits<OvCore::ECS::Components::CLookAt>
	{
		static constexpr std::string_view Name = "class OvCore::ECS::Components::CLookAt";
	};
}
