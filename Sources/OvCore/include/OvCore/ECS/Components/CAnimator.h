/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <OvCore/Animation/AnimationGraph.h>
#include <OvCore/Animation/Retarget.h>
#include <OvCore/ECS/Components/CSkinnedMeshRenderer.h>

#include "AComponent.h"

namespace OvCore::SceneSystem { class Scene; }
namespace OvRendering::Resources { class Model; }

namespace OvCore::ECS::Components
{
	/**
	* Plays an animation graph (a JSON file of the project, see Docs/Animation.md) on its actor's skinned mesh: layers of
	* states, each a clip or clips blended along a parameter, and transitions between them on the parameters a script
	* sets. While it plays, the skinned mesh renderer samples the graph's clips instead of its own layers, with the same
	* bone matching. Moved on by the scene once a frame, after every OnUpdate and before every OnLateUpdate.
	*/
	class CAnimator : public AComponent
	{
	public:
		/**
		* Constructor
		* @param p_owner
		*/
		CAnimator(ECS::Actor& p_owner);

		/**
		* Returns the name of the component
		*/
		std::string GetName() override;

		/**
		* Returns the type name of the component
		*/
		virtual std::string GetTypeName() override;

		/**
		* Sets the graph file (an asset path), loaded again on next use
		* @param p_path
		*/
		void SetGraphPath(const std::string& p_path);

		/**
		* Returns the graph file
		*/
		const std::string& GetGraphPath() const;

		/**
		* Sets the model whose clips the graph plays, or nothing for the rendered model
		* @param p_model
		*/
		void SetAnimationSource(OvRendering::Resources::Model* p_model);

		/**
		* Returns the model whose clips the graph plays, or nothing for the rendered model
		*/
		OvRendering::Resources::Model* GetAnimationSource() const;

		/**
		* Sets which clip bone drives each model bone, for clips of another rig (see SkinnedMeshRenderer)
		* @param p_boneMap
		*/
		void SetBoneMap(const Animation::BoneMap& p_boneMap);

		/**
		* Returns which clip bone drives each model bone
		*/
		const Animation::BoneMap& GetBoneMap() const;

		/**
		* Reads the graph file again, from its default states
		*/
		void Reload();

		/**
		* Returns true when the graph is loaded and plays
		*/
		bool IsLoaded();

		/**
		* Returns why the graph does not play (no file, a mistake in it with its JSON path, an unknown clip, no skinned
		* mesh), or nothing
		*/
		const std::optional<std::string>& GetError() const;

		/**
		* Sets a number parameter, returning false for an unknown parameter or one of another kind
		* @param p_name
		* @param p_value
		*/
		bool SetFloat(const std::string& p_name, float p_value);

		/**
		* Returns a number parameter, or nothing
		* @param p_name
		*/
		std::optional<float> GetFloat(const std::string& p_name);

		/**
		* Sets a boolean parameter, returning false for an unknown parameter or one of another kind
		* @param p_name
		* @param p_value
		*/
		bool SetBool(const std::string& p_name, bool p_value);

		/**
		* Returns a boolean parameter, or nothing
		* @param p_name
		*/
		std::optional<bool> GetBool(const std::string& p_name);

		/**
		* Sets a trigger, until a transition uses it up or ResetTrigger. Returns false for an unknown trigger
		* @param p_name
		*/
		bool SetTrigger(const std::string& p_name);

		/**
		* Clears a trigger. Returns false for an unknown trigger
		* @param p_name
		*/
		bool ResetTrigger(const std::string& p_name);

		/**
		* Returns the names of the parameters
		*/
		std::vector<std::string> GetParameterNames();

		/**
		* Returns the names of the layers
		*/
		std::vector<std::string> GetLayerNames();

		/**
		* Returns the index of a layer by name
		* @param p_name
		*/
		std::optional<size_t> FindLayer(const std::string& p_name);

		/**
		* Returns the state a layer is in (the one it goes to while it fades), or nothing
		* @param p_layer
		*/
		std::optional<std::string> GetState(size_t p_layer = 0);

		/**
		* Returns how far a layer's state has played, in its normalized time (1 per loop)
		* @param p_layer
		*/
		float GetStateTime(size_t p_layer = 0);

		/**
		* Returns true while a layer fades from a state to another
		* @param p_layer
		*/
		bool IsInTransition(size_t p_layer = 0);

		/**
		* Sets how much a layer covers the layers before it (0 to 1), returning false for an unknown layer
		* @param p_layer
		* @param p_weight
		*/
		bool SetLayerWeight(size_t p_layer, float p_weight);

		/**
		* Returns how much a layer covers the layers before it, or nothing
		* @param p_layer
		*/
		std::optional<float> GetLayerWeight(size_t p_layer);

		/**
		* Moves to a state at once, over a fade (seconds), whatever the transitions say: in the given layer, or in the
		* first layer that has it. Returns false for an unknown state
		* @param p_state
		* @param p_fade
		* @param p_layer
		*/
		bool Play(const std::string& p_state, float p_fade = 0.0f, std::optional<size_t> p_layer = std::nullopt);

		/**
		* Puts every parameter back to its default and every layer in its default state
		*/
		void Reset();

		/**
		* Moves the graph on and has the skinned mesh renderer sample it: called by the scene once a frame
		* @param p_scene
		* @param p_deltaTime
		*/
		void Apply(const SceneSystem::Scene& p_scene, float p_deltaTime);

		/**
		* Gives the skinned mesh renderer its own layers back: called when the animator is disabled, and by the scene
		* when it is removed
		*/
		void ReleaseRenderer();

		/**
		* Gives the skinned mesh renderer its own layers back
		*/
		void OnDisable() override;

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
		Animation::AnimationGraph* EnsureLoaded(const SceneSystem::Scene* p_scene = nullptr);
		void Load(const SceneSystem::Scene& p_scene);
		void Fail(std::string p_error);
		std::optional<size_t> FindParameter(const std::string& p_name, Animation::EGraphParameterType p_type);
		void BuildParameterWidgets(OvUI::Internal::WidgetContainer& p_container);

	private:
		std::string m_graphPath;
		OvRendering::Resources::Model* m_source = nullptr;
		Animation::BoneMap m_boneMap;

		// Runtime, read again when the file, the source or the model change
		bool m_dirty = true;
		bool m_driving = false;
		bool m_layersSent = false;
		bool m_changed = true; // A script changed the graph since the last pose
		uint32_t m_loadCount = 0; // How many times the graph was read, for the inspector to follow
		uint32_t m_inspectedLoadCount = 0;
		const OvRendering::Resources::Model* m_builtModel = nullptr;
		std::optional<Animation::AnimationGraph> m_graph;
		std::vector<uint32_t> m_clipIndices;
		std::vector<float> m_layerWeights;
		std::vector<CSkinnedMeshRenderer::DrivenSample> m_drivenSamples;
		std::optional<std::string> m_error;
		std::string m_boneMapText; // As the inspector shows and edits it
	};

	template<>
	struct ComponentTraits<OvCore::ECS::Components::CAnimator>
	{
		static constexpr std::string_view Name = "class OvCore::ECS::Components::CAnimator";
	};
}
