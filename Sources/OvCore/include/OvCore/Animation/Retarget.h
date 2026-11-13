/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <OvMaths/FQuaternion.h>
#include <OvMaths/FVector3.h>

namespace OvRendering::Animation { struct Skeleton; }

namespace OvCore::Animation
{
	/**
	* How a clip's node drives one of a model's nodes
	*/
	struct RetargetNode
	{
		int32_t source = -1;					// The clip's node driving it, or -1 when none does
		OvMaths::FVector3 restOffset;			// The model's rest position minus the clip's
		float lengthRatio = 1.0f;				// The model's rest length over the clip's
		bool turned = false;					// The rests differ in rotation: the keys are carried through pre and post
		OvMaths::FQuaternion pre;				// inverse(G_model(parent)) * G_clip(parent), G being a global rest rotation
		OvMaths::FQuaternion post;				// inverse(G_clip(node)) * G_model(node)
	};

	/**
	* Names of a model's bones, each with the name of the clip's bone that drives it
	*/
	using BoneMap = std::unordered_map<std::string, std::string>;

	/**
	* Matches a clip's skeleton to a model's, and carries the clip's keys over to the model's bones.
	*
	* A model's bone is driven by the clip's bone of the same name, or of the name the bone map gives it. Their parents
	* must match the same way, and their rest scales be the same. Their rest rotations may differ: a key is carried
	* through the two rests, so that the bone turns in the model's space as the clip's turns in its own:
	*   local_model = inverse(G_model(parent)) * G_clip(parent) * local_clip * inverse(G_clip(bone)) * G_model(bone)
	* A position key moves the bone from the model's rest as it moves the clip's from its own, turned into the model's
	* parent and scaled by the ratio of the two rest lengths (a longer leg dips its hips further).
	* The model may extend the clip's skeleton with bones of its own (hair, cloth) under a bone both share: no key drives
	* them.
	*/
	class Retarget
	{
	public:
		/**
		* Matches the clip's skeleton to the model's, one entry per node of the model. Returns nothing once matched,
		* else why not, naming the bone
		* @param p_model
		* @param p_clip
		* @param p_boneMap
		* @param p_nodes
		*/
		static std::optional<std::string> Build(
			const OvRendering::Animation::Skeleton& p_model,
			const OvRendering::Animation::Skeleton& p_clip,
			const BoneMap& p_boneMap,
			std::vector<RetargetNode>& p_nodes
		);

		/**
		* Returns the clip's rest position of a node, from the model's
		* @param p_node
		* @param p_modelRest
		*/
		static OvMaths::FVector3 ClipRest(const RetargetNode& p_node, const OvMaths::FVector3& p_modelRest);

		/**
		* Returns the model's local position for a clip's position key
		* @param p_node
		* @param p_modelRest
		* @param p_clipPosition
		*/
		static OvMaths::FVector3 Position(const RetargetNode& p_node, const OvMaths::FVector3& p_modelRest, const OvMaths::FVector3& p_clipPosition);

		/**
		* Returns the model's local rotation for a clip's rotation key
		* @param p_node
		* @param p_clipRotation
		*/
		static OvMaths::FQuaternion Rotation(const RetargetNode& p_node, const OvMaths::FQuaternion& p_clipRotation);
	};
}
