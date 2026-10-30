/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <OvCore/ECS/Components/AComponent.h>
#include <OvMaths/FMatrix4.h>
#include <OvMaths/FQuaternion.h>
#include <OvMaths/FVector3.h>

namespace OvCore::ECS { class Actor; }
namespace OvPhysics::Entities { class Ragdoll; }
namespace OvRendering::Animation { struct SkeletalAnimation; }
namespace OvRendering::Resources { class Model; }

namespace OvCore::ECS::Components
{
	/**
	* Component responsible for skeletal animation playback and skinning data generation.
	* Any number of animations can play simultaneously, each layer owning its own animation source
	* model, animation, playback time, speed, loop mode and weight.
	* Bones are blended per node, and only layers holding a track for a given node contribute to it,
	* their weights being normalized against each other. Nodes no layer animates keep their bind pose.
	*/
	class CSkinnedMeshRenderer : public AComponent
	{
	public:
		/**
		* One part of a ragdoll, named after the bone it moves: a capsule from that bone to the bone named `to`, or, when
		* `to` is empty, running `length` metres on from the bone's parent (the head carries on from the neck)
		*/
		struct RagdollPart
		{
			std::string bone;
			std::string to;
			float length = 0.0f;
			float radius = 0.05f;
			float mass = 1.0f;
			float swing = 30.0f; // Degrees the part may swing away from its parent
			float twist = 15.0f; // Degrees it may twist about its own axis, either way
		};

		/**
		* Constructor
		* @param p_owner
		*/
		CSkinnedMeshRenderer(ECS::Actor& p_owner);

		/**
		* Destructor
		*/
		~CSkinnedMeshRenderer() override;

		/**
		* Returns the name of the component
		*/
		std::string GetName() override;

		/**
		* Returns the type name of the component
		*/
		virtual std::string GetTypeName() override;

		/**
		* Called by the model renderer when its model changes
		*/
		void NotifyModelChanged();

		/**
		* Returns true if the component has a valid skinning palette ready for rendering
		*/
		bool HasSkinningData() const;

		/**
		* Returns the number of active animation layers (always at least 1)
		*/
		uint32_t GetLayerCount() const;

		/**
		* Appends a new animation layer, returning its index
		*/
		uint32_t AddLayer();

		/**
		* Removes an animation layer, shifting the following layers down by one.
		* Fails when the index is invalid or when only one layer remains.
		* @param p_layer
		*/
		bool RemoveLayer(uint32_t p_layer);

		/**
		* Start animation playback on a layer
		* @param p_layer
		*/
		void Play(uint32_t p_layer = 0);

		/**
		* Pause animation playback on a layer
		* @param p_layer
		*/
		void Pause(uint32_t p_layer = 0);

		/**
		* Stop animation playback on a layer and reset its time to 0
		* @param p_layer
		*/
		void Stop(uint32_t p_layer = 0);

		/**
		* Returns true if playback is active on a layer
		* @param p_layer
		*/
		bool IsPlaying(uint32_t p_layer = 0) const;

		/**
		* Sets loop mode on a layer
		* @param p_value
		* @param p_layer
		*/
		void SetLooping(bool p_value, uint32_t p_layer = 0);

		/**
		* Returns true if loop mode is enabled on a layer
		* @param p_layer
		*/
		bool IsLooping(uint32_t p_layer = 0) const;

		/**
		* Set playback speed on a layer
		* @param p_value
		* @param p_layer
		*/
		void SetPlaybackSpeed(float p_value, uint32_t p_layer = 0);

		/**
		* Get playback speed of a layer
		* @param p_layer
		*/
		float GetPlaybackSpeed(uint32_t p_layer = 0) const;

		/**
		* Sets the blend weight of a layer, clamped to the [0,1] range
		* Weights are normalized per node against the other layers animating that node, so a layer
		* animating a node alone contributes fully until its weight reaches 0, where the node falls
		* back to its bind pose
		* @param p_value
		* @param p_layer
		*/
		void SetLayerWeight(float p_value, uint32_t p_layer = 0);

		/**
		* Returns the blend weight of a layer
		* @param p_layer
		*/
		float GetLayerWeight(uint32_t p_layer = 0) const;

		/**
		* Limits a layer to the subtree under a node: the other nodes ignore the layer, as if it held no
		* track for them, and keep following the layers below it. An empty name lifts the limit; an unknown
		* one is reported and leaves the layer unlimited. On the nodes it keeps, a masked layer still blends
		* with the others by weight, so the layers below it should stay above zero to keep driving the rest
		* @param p_nodeName
		* @param p_layer
		*/
		void SetLayerMask(const std::string& p_nodeName, uint32_t p_layer = 0);

		/**
		* Returns the name of the node a layer is limited to, empty when it is not limited
		* @param p_layer
		*/
		std::string GetLayerMask(uint32_t p_layer = 0) const;

		/**
		* Returns the scale applied to mesh bounds during frustum culling
		* Returned value is always >= 1.0f
		*/
		float GetMeshBoundsScale() const;

		/**
		* Sets the scale applied to mesh bounds during frustum culling
		* Any value below 1.0f will be clamped to 1.0f
		* @param p_scale
		*/
		void SetMeshBoundsScale(float p_scale);

		/**
		* Sets the current playback time of a layer in seconds
		* @param p_timeSeconds
		* @param p_layer
		*/
		void SetTime(float p_timeSeconds, uint32_t p_layer = 0);

		/**
		* Returns the current playback time of a layer in seconds
		* @param p_layer
		*/
		float GetTime(uint32_t p_layer = 0) const;

		/**
		* Sets the external model used as animation source by a layer.
		* Pass nullptr to use the rendered model animations.
		* @param p_model
		* @param p_layer
		*/
		void SetAnimationSourceModel(OvRendering::Resources::Model* p_model, uint32_t p_layer = 0);

		/**
		* Sets the external model used as animation source by a layer, resolved from its asset path.
		* The model is loaded on first use and kept by the model manager, so repeated calls with the
		* same path are cheap. Pass an empty path to use the rendered model animations.
		* @param p_path
		* @param p_layer
		*/
		void SetAnimationSourceModel(const std::string& p_path, uint32_t p_layer = 0);

		/**
		* Returns the external animation source model of a layer, or nullptr when the rendered model is used
		* @param p_layer
		*/
		OvRendering::Resources::Model* GetAnimationSourceModel(uint32_t p_layer = 0) const;

		/**
		* Returns true if the animation source of a layer can be applied to the rendered model skeleton
		* @param p_layer
		*/
		bool IsAnimationSourceCompatible(uint32_t p_layer = 0) const;

		/**
		* Returns the number of animations available to a layer
		* @param p_layer
		*/
		uint32_t GetAnimationCount(uint32_t p_layer = 0) const;

		/**
		* Returns the animation name at index for a layer (std::nullopt if index is invalid)
		* @param p_index
		* @param p_layer
		*/
		std::optional<std::string> GetAnimationName(uint32_t p_index, uint32_t p_layer = 0) const;

		/**
		* Sets the active animation of a layer by index. Pass std::nullopt to clear the layer.
		* @param p_index
		* @param p_layer
		*/
		bool SetAnimation(std::optional<uint32_t> p_index, uint32_t p_layer = 0);

		/**
		* Sets the active animation of a layer by name
		* @param p_name
		* @param p_layer
		*/
		bool SetAnimation(const std::string& p_name, uint32_t p_layer = 0);

		/**
		* Returns the active animation index of a layer, or std::nullopt if none is set
		* @param p_layer
		*/
		std::optional<uint32_t> GetActiveAnimationIndex(uint32_t p_layer = 0) const;

		/**
		* Returns the active animation name of a layer (std::nullopt if none)
		* @param p_layer
		*/
		std::optional<std::string> GetActiveAnimationName(uint32_t p_layer = 0) const;

		/**
		* Returns the number of available bones
		*/
		uint32_t GetBoneCount() const;

		/**
		* Returns the bone name at index (std::nullopt if index is invalid)
		* @param p_index
		*/
		std::optional<std::string> GetBoneName(uint32_t p_index) const;

		/**
		* Returns the bone index by name (std::nullopt if not found)
		* @param p_name
		*/
		std::optional<uint32_t> GetBoneIndex(const std::string& p_name) const;

		/**
		* Returns the local bone position (std::nullopt if index is invalid)
		* @param p_boneIndex
		*/
		std::optional<OvMaths::FVector3> GetBoneLocalPosition(uint32_t p_boneIndex) const;

		/**
		* Returns the local bone rotation (std::nullopt if index is invalid)
		* @param p_boneIndex
		*/
		std::optional<OvMaths::FQuaternion> GetBoneLocalRotation(uint32_t p_boneIndex) const;

		/**
		* Returns the local bone scale (std::nullopt if index is invalid)
		* @param p_boneIndex
		*/
		std::optional<OvMaths::FVector3> GetBoneLocalScale(uint32_t p_boneIndex) const;

		/**
		* Returns the index of a node of the skeleton by name, a bone or any other node (std::nullopt if not found)
		* @param p_name
		*/
		std::optional<uint32_t> FindNodeIndex(const std::string& p_name) const;

		/**
		* Returns the current global transform of a node in skeleton/model space, excluding the owning actor's world
		* transform, as its position, rotation and scale (false if the node index is invalid or no pose was computed yet)
		* @param p_nodeIndex
		* @param p_position
		* @param p_rotation
		* @param p_scale
		*/
		bool GetNodeGlobalTransform(uint32_t p_nodeIndex, OvMaths::FVector3& p_position, OvMaths::FQuaternion& p_rotation, OvMaths::FVector3& p_scale) const;

		/**
		* Returns the current bone global position in skeleton/model space, excluding the owning actor's world transform
		* @param p_boneIndex
		* @return The bone global position, or std::nullopt if the bone index is invalid
		*/
		std::optional<OvMaths::FVector3> GetBoneGlobalPosition(uint32_t p_boneIndex) const;

		/**
		* Returns the current bone global rotation in skeleton/model space, excluding the owning actor's world transform
		* @param p_boneIndex
		* @return The bone global rotation, or std::nullopt if the bone index is invalid
		*/
		std::optional<OvMaths::FQuaternion> GetBoneGlobalRotation(uint32_t p_boneIndex) const;

		/**
		* Returns the current bone global scale in skeleton/model space, excluding the owning actor's world transform
		* @param p_boneIndex
		* @return The bone global scale, or std::nullopt if the bone index is invalid
		*/
		std::optional<OvMaths::FVector3> GetBoneGlobalScale(uint32_t p_boneIndex) const;

		/**
		* Sets the local bone position
		* @param p_boneIndex
		* @param p_position
		*/
		bool SetBoneLocalPosition(uint32_t p_boneIndex, const OvMaths::FVector3& p_position);

		/**
		* Sets the local bone rotation
		* @param p_boneIndex
		* @param p_rotation
		*/
		bool SetBoneLocalRotation(uint32_t p_boneIndex, const OvMaths::FQuaternion& p_rotation);

		/**
		* Sets the local bone scale
		* @param p_boneIndex
		* @param p_scale
		*/
		bool SetBoneLocalScale(uint32_t p_boneIndex, const OvMaths::FVector3& p_scale);

		/**
		* Returns the transposed skinning matrix palette ready for GPU upload
		*/
		const std::vector<OvMaths::FMatrix4>& GetBoneMatricesTransposed() const;

		/**
		* Returns an incrementing pose version, updated whenever bone matrices change
		*/
		uint64_t GetPoseVersion() const;

		/**
		* Hands the skeleton over to physics from the current pose: every part becomes a capsule joined to the nearest
		* part above it, and its bone follows the capsule while the others keep their pose relative to their parent.
		* The animation stops driving the skeleton until StopRagdoll. Returns false when a bone is unknown or the
		* ragdoll cannot be made
		* @param p_parts
		* @param p_linearVelocity (of every part at the start, world space)
		*/
		bool StartRagdoll(const std::vector<RagdollPart>& p_parts, const OvMaths::FVector3& p_linearVelocity);

		/**
		* Removes the ragdoll from the simulation and gives the skeleton back to the animation
		*/
		void StopRagdoll();

		/**
		* Returns true while a ragdoll drives the skeleton
		*/
		bool IsRagdoll() const;

		/**
		* Pushes the ragdoll's part that moves the given bone, or the nearest part above it
		* @param p_boneName
		* @param p_impulse (world space)
		*/
		bool AddRagdollImpulse(const std::string& p_boneName, const OvMaths::FVector3& p_impulse);

		/**
		* Called each frame by the actor
		* @param p_deltaTime
		*/
		void OnUpdate(float p_deltaTime) override;

		/**
		* Called when the component or its actor is disabled: the ragdoll leaves the simulation
		*/
		void OnDisable() override;

		/**
		* Called when the component is destroyed
		*/
		void OnDestroy() override;

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
		* Draw component inspector widgets
		* @param p_root
		*/
		virtual void OnInspector(OvUI::Internal::WidgetContainer& p_root) override;

	private:
		struct AnimationLayer
		{
			OvRendering::Resources::Model* animationSourceModel = nullptr;
			std::optional<uint32_t> animationIndex = std::nullopt;
			std::string deserializedAnimationName;
			float timeTicks = 0.0f;
			float speed = 1.0f;
			float weight = 1.0f;
			bool playing = true;
			bool looping = true;
			std::string maskRootName;

			// Runtime data, rebuilt whenever the model or the layer animation source changes.
			// sourceNodeByTargetNode maps a rendered skeleton node to the source node driving it
			// (-1 when unanimated), and is empty when the source is incompatible.
			// restOffsetByTargetNode is the rendered node's rest position minus its source node's: added to the
			// clip's positions, it plays the clip on a rig whose bones have other lengths (zero for the same rig).
			// restScaleByTargetNode is the ratio of their rest lengths: the clip's move away from its own rest is
			// scaled by it, so a longer leg dips its hips further (one for the same rig).
			std::vector<std::string> animationNames;
			std::vector<int32_t> sourceNodeByTargetNode;
			std::vector<OvMaths::FVector3> restOffsetByTargetNode;
			std::vector<float> restScaleByTargetNode;
		};

		// Per-layer data sampled once per pose evaluation, before blending the skeleton node by node
		struct ActiveLayerSample
		{
			const OvRendering::Animation::SkeletalAnimation* animation = nullptr;
			float sampleTime = 0.0f;
			float duration = 0.0f;
			float weight = 1.0f;
			bool looping = true;
			const std::vector<int32_t>* sourceNodeByTargetNode = nullptr;
			const std::vector<OvMaths::FVector3>* restOffsetByTargetNode = nullptr;
			const std::vector<float>* restScaleByTargetNode = nullptr;
		};

		bool HasCompatibleModel() const;
		bool IsLayerCompatible(const AnimationLayer& p_layer) const;
		const OvRendering::Resources::Model* GetLayerAnimationModel(const AnimationLayer& p_layer) const;
		AnimationLayer* FindLayer(uint32_t p_layer);
		const AnimationLayer* FindLayer(uint32_t p_layer) const;
		void SyncWithModel();
		void RebuildRuntimeData();
		void RebuildLayerRuntimeData(AnimationLayer& p_layer, std::vector<int32_t>& p_nodeMapScratch);
		void ApplyLayerMask(AnimationLayer& p_layer) const;
		void ResolveLayerAnimation(AnimationLayer& p_layer);
		void EvaluatePose();
		std::optional<uint32_t> GetNodeIndexFromBoneIndex(uint32_t p_boneIndex) const;
		int32_t FindRagdollPart(uint32_t p_nodeIndex) const;
		void RecomputeBoneMatricesFromLocalPose();
		float GetAnimationDurationSeconds(uint32_t p_layer) const;
		void UpdatePlayback(AnimationLayer& p_layer, float p_deltaTime);
		void BuildLayerWidgets(OvUI::Internal::WidgetContainer& p_container);

	private:
		const OvRendering::Resources::Model* m_model = nullptr;

		float m_meshBoundsScale = 1.5f;
		float m_poseEvaluationRate = 60.0f;
		float m_poseEvaluationAccumulator = 0.0f;

		std::vector<AnimationLayer> m_layers;

		// Reused across pose evaluations to keep them free of per-frame allocations
		std::vector<ActiveLayerSample> m_activeLayerSamples;

		uint64_t m_poseVersion = 0;
		bool m_manualPoseOverride = false;

		std::vector<OvMaths::FMatrix4> m_localPose;
		std::vector<OvMaths::FMatrix4> m_globalPose;
		std::vector<OvMaths::FMatrix4> m_boneMatrices;
		std::vector<OvMaths::FMatrix4> m_boneMatricesTransposed;

		// While set, the ragdoll's parts drive their nodes (m_ragdollPartByNode, -1 for the others) in place of the
		// animation, each with the world scale its joint had when the ragdoll started
		std::unique_ptr<OvPhysics::Entities::Ragdoll> m_ragdoll;
		std::vector<int32_t> m_ragdollPartByNode;
		std::vector<OvMaths::FVector3> m_ragdollPartScales;
	};

	template<>
	struct ComponentTraits<OvCore::ECS::Components::CSkinnedMeshRenderer>
	{
		static constexpr std::string_view Name = "class OvCore::ECS::Components::CSkinnedMeshRenderer";
	};
}
