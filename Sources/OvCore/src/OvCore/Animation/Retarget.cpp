/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cmath>
#include <format>

#include <OvCore/Animation/Retarget.h>
#include <OvRendering/Animation/SkeletalData.h>

namespace
{
	using OvMaths::FQuaternion;
	using OvMaths::FVector3;
	using OvRendering::Animation::Skeleton;

	constexpr float kRestTolerance = 0.0001f;

	bool CollectRequiredSkinningNodes(const Skeleton& p_skeleton, std::vector<bool>& p_requiredNodes)
	{
		p_requiredNodes.assign(p_skeleton.nodes.size(), false);

		for (const auto& bone : p_skeleton.bones)
		{
			if (bone.nodeIndex >= p_skeleton.nodes.size())
			{
				return false;
			}

			int32_t nodeIndex = static_cast<int32_t>(bone.nodeIndex);
			while (nodeIndex >= 0)
			{
				const auto currentNodeIndex = static_cast<size_t>(nodeIndex);
				if (currentNodeIndex >= p_skeleton.nodes.size())
				{
					return false;
				}

				if (p_requiredNodes[currentNodeIndex])
				{
					break;
				}

				p_requiredNodes[currentNodeIndex] = true;
				nodeIndex = p_skeleton.nodes[currentNodeIndex].parentIndex;
			}
		}

		return true;
	}

	// Each node's rest rotation from the skeleton's root, parents worked out before their children whatever the order
	std::vector<FQuaternion> GlobalRestRotations(const Skeleton& p_skeleton)
	{
		std::vector<FQuaternion> result(p_skeleton.nodes.size(), FQuaternion::Identity);
		std::vector<bool> done(p_skeleton.nodes.size(), false);

		const auto resolve = [&](auto& p_self, size_t p_index, size_t p_depth) -> FQuaternion
		{
			if (done[p_index] || p_depth > p_skeleton.nodes.size())
			{
				return result[p_index];
			}

			const auto& node = p_skeleton.nodes[p_index];
			const FQuaternion local = FQuaternion::Normalize(node.bindRotation);
			const bool hasParent = node.parentIndex >= 0 && static_cast<size_t>(node.parentIndex) < p_skeleton.nodes.size();

			result[p_index] = hasParent ? p_self(p_self, static_cast<size_t>(node.parentIndex), p_depth + 1) * local : local;
			done[p_index] = true;
			return result[p_index];
		};

		for (size_t i = 0; i < p_skeleton.nodes.size(); ++i)
		{
			resolve(resolve, i, 0);
		}

		return result;
	}

	// Two rotations are the same when their quaternions are, component by component, or opposite (q and -q turn alike):
	// about as tight as comparing the rest matrices element by element, as matching did before rests could turn
	bool AreRotationsClose(const FQuaternion& p_left, const FQuaternion& p_right)
	{
		const FQuaternion left = FQuaternion::Normalize(p_left);
		const FQuaternion right = FQuaternion::Normalize(p_right);
		const float sign = FQuaternion::DotProduct(left, right) < 0.0f ? -1.0f : 1.0f;

		return
			std::abs(left.x - sign * right.x) <= kRestTolerance &&
			std::abs(left.y - sign * right.y) <= kRestTolerance &&
			std::abs(left.z - sign * right.z) <= kRestTolerance &&
			std::abs(left.w - sign * right.w) <= kRestTolerance;
	}

	bool AreScalesClose(const FVector3& p_left, const FVector3& p_right)
	{
		return
			std::abs(p_left.x - p_right.x) <= kRestTolerance &&
			std::abs(p_left.y - p_right.y) <= kRestTolerance &&
			std::abs(p_left.z - p_right.z) <= kRestTolerance;
	}
}

std::optional<std::string> OvCore::Animation::Retarget::Build(
	const Skeleton& p_model,
	const Skeleton& p_clip,
	const BoneMap& p_boneMap,
	std::vector<RetargetNode>& p_nodes
)
{
	p_nodes.assign(p_model.nodes.size(), RetargetNode{});

	if (p_model.nodes.empty() || p_model.bones.empty())
	{
		return std::string{ "the model has no bones" };
	}

	if (p_clip.nodes.empty())
	{
		return std::string{ "the clip's model has no skeleton" };
	}

	std::vector<bool> requiredNodes;
	if (!CollectRequiredSkinningNodes(p_model, requiredNodes))
	{
		return std::string{ "the model's skeleton names a node it does not have" };
	}

	// The clip's name of a model's node: the bone map's, else its own
	const auto clipName = [&](const std::string& p_modelName) -> const std::string&
	{
		const auto found = p_boneMap.find(p_modelName);
		return found != p_boneMap.end() ? found->second : p_modelName;
	};

	const auto findClipNode = [&](size_t p_modelNode) -> std::optional<uint32_t>
	{
		const auto found = p_clip.FindNodeIndex(clipName(p_model.nodes[p_modelNode].name));
		return found.has_value() && *found < p_clip.nodes.size() ? found : std::nullopt;
	};

	// A node the clip lacks may hang under a bone both share: the first of its ancestors the clip knows must be a bone
	// on both sides, never the scene root, the armature or a mesh node
	const auto hangsUnderSharedBone = [&](size_t p_modelNode)
	{
		int32_t nodeIndex = p_model.nodes[p_modelNode].parentIndex;

		while (nodeIndex >= 0 && static_cast<size_t>(nodeIndex) < p_model.nodes.size())
		{
			const auto& node = p_model.nodes[static_cast<size_t>(nodeIndex)];

			if (p_clip.FindNodeIndex(clipName(node.name)).has_value())
			{
				return node.boneIndex >= 0 && p_clip.FindBoneIndex(clipName(node.name)).has_value();
			}

			nodeIndex = node.parentIndex;
		}

		return false;
	};

	const auto modelGlobals = GlobalRestRotations(p_model);
	const auto clipGlobals = GlobalRestRotations(p_clip);

	std::vector<int32_t> modelByClip(p_clip.nodes.size(), -1);
	std::vector<bool> extensionNodes(p_model.nodes.size(), false);

	for (size_t modelNodeIndex = 0; modelNodeIndex < requiredNodes.size(); ++modelNodeIndex)
	{
		if (!requiredNodes[modelNodeIndex])
		{
			continue;
		}

		const auto& modelNode = p_model.nodes[modelNodeIndex];
		const auto clipNodeIndex = findClipNode(modelNodeIndex);

		if (!clipNodeIndex.has_value())
		{
			// Only against a clip with bones, so that a misnamed bone is never taken for an extension
			if (p_clip.bones.empty() || !hangsUnderSharedBone(modelNodeIndex))
			{
				return std::format("the clip's rig has no node '{}' for the model's '{}'", clipName(modelNode.name), modelNode.name);
			}

			extensionNodes[modelNodeIndex] = true;
			continue;
		}

		const auto& clipNode = p_clip.nodes[*clipNodeIndex];

		if (const int32_t other = modelByClip[*clipNodeIndex]; other >= 0)
		{
			return std::format("the clip's node '{}' drives both the model's '{}' and '{}'", clipNode.name, p_model.nodes[static_cast<size_t>(other)].name, modelNode.name);
		}

		// Their parents must match: the model's parent drives from the clip's parent
		const int32_t modelParentIndex = modelNode.parentIndex;
		const int32_t clipParentIndex = clipNode.parentIndex;
		const bool hasRequiredModelParent =
			modelParentIndex >= 0 &&
			static_cast<size_t>(modelParentIndex) < requiredNodes.size() &&
			requiredNodes[static_cast<size_t>(modelParentIndex)];

		if (hasRequiredModelParent)
		{
			const auto expectedClipParent = findClipNode(static_cast<size_t>(modelParentIndex));
			if (!expectedClipParent.has_value() || clipParentIndex != static_cast<int32_t>(*expectedClipParent))
			{
				return std::format(
					"the model's '{}' hangs under '{}', but the clip's '{}' does not hang under its match",
					modelNode.name, p_model.nodes[static_cast<size_t>(modelParentIndex)].name, clipNode.name
				);
			}
		}
		else if (clipParentIndex >= 0)
		{
			return std::format("the model's '{}' is a root, but the clip's '{}' is not", modelNode.name, clipNode.name);
		}

		if (!AreScalesClose(modelNode.bindScale, clipNode.bindScale))
		{
			return std::format("the model's '{}' and the clip's '{}' have other rest scales", modelNode.name, clipNode.name);
		}

		RetargetNode& entry = p_nodes[modelNodeIndex];
		entry.source = static_cast<int32_t>(*clipNodeIndex);
		entry.restOffset = modelNode.bindPosition - clipNode.bindPosition;

		const float clipLength = FVector3::Length(clipNode.bindPosition);
		if (clipLength > 0.0001f)
		{
			entry.lengthRatio = FVector3::Length(modelNode.bindPosition) / clipLength;
		}

		// The rests' rotations, from the parents' and the node's own: none for the same rig
		const FQuaternion modelParentGlobal = modelParentIndex >= 0 ? modelGlobals[static_cast<size_t>(modelParentIndex)] : FQuaternion::Identity;
		const FQuaternion clipParentGlobal = clipParentIndex >= 0 ? clipGlobals[static_cast<size_t>(clipParentIndex)] : FQuaternion::Identity;
		entry.pre = FQuaternion::Normalize(FQuaternion::Conjugate(modelParentGlobal) * clipParentGlobal);
		entry.post = FQuaternion::Normalize(FQuaternion::Conjugate(clipGlobals[*clipNodeIndex]) * modelGlobals[modelNodeIndex]);
		entry.turned = !AreRotationsClose(entry.pre, FQuaternion::Identity) || !AreRotationsClose(entry.post, FQuaternion::Identity);

		modelByClip[*clipNodeIndex] = static_cast<int32_t>(modelNodeIndex);
	}

	if (!p_clip.bones.empty())
	{
		size_t extensionBones = 0;

		for (const auto& modelBone : p_model.bones)
		{
			extensionBones += extensionNodes[modelBone.nodeIndex] ? 1 : 0;
		}

		if (p_clip.bones.size() + extensionBones != p_model.bones.size())
		{
			// Most often a bone renamed on one side: the clip's bone then drives nothing, and the model's passes for one
			// of its own. Both are named
			std::string unmatched;
			for (const auto& clipBone : p_clip.bones)
			{
				if (clipBone.nodeIndex < modelByClip.size() && modelByClip[clipBone.nodeIndex] < 0)
				{
					unmatched += (unmatched.empty() ? "'" : ", '") + clipBone.name + "'";
				}
			}

			std::string own;
			for (const auto& modelBone : p_model.bones)
			{
				if (extensionNodes[modelBone.nodeIndex])
				{
					own += (own.empty() ? "'" : ", '") + modelBone.name + "'";
				}
			}

			return std::format(
				"the clip's bones {} drive none of the model's, whose {} were taken for bones of its own",
				unmatched.empty() ? std::string{ "(none)" } : unmatched,
				own.empty() ? std::string{ "(none)" } : own
			);
		}

		for (const auto& modelBone : p_model.bones)
		{
			if (extensionNodes[modelBone.nodeIndex])
			{
				continue;
			}

			const auto clipBoneIndex = p_clip.FindBoneIndex(clipName(modelBone.name));
			if (
				!clipBoneIndex.has_value() ||
				*clipBoneIndex >= p_clip.bones.size() ||
				p_clip.bones[*clipBoneIndex].nodeIndex >= modelByClip.size() ||
				modelByClip[p_clip.bones[*clipBoneIndex].nodeIndex] != static_cast<int32_t>(modelBone.nodeIndex)
			)
			{
				return std::format("the clip's rig has no bone '{}' for the model's bone '{}'", clipName(modelBone.name), modelBone.name);
			}
		}
	}

	return std::nullopt;
}

OvMaths::FVector3 OvCore::Animation::Retarget::ClipRest(const RetargetNode& p_node, const OvMaths::FVector3& p_modelRest)
{
	return p_modelRest - p_node.restOffset;
}

OvMaths::FVector3 OvCore::Animation::Retarget::Position(const RetargetNode& p_node, const OvMaths::FVector3& p_modelRest, const OvMaths::FVector3& p_clipPosition)
{
	if (!p_node.turned)
	{
		// The same arithmetic as before rotations were carried over, so that a rig played as it was moves the same
		return p_node.lengthRatio == 1.0f ?
			p_clipPosition + p_node.restOffset :
			p_clipPosition + p_node.restOffset + (p_clipPosition - ClipRest(p_node, p_modelRest)) * (p_node.lengthRatio - 1.0f);
	}

	const FVector3 move = (p_clipPosition - ClipRest(p_node, p_modelRest)) * p_node.lengthRatio;
	return p_modelRest + FQuaternion::RotatePoint(move, p_node.pre);
}

OvMaths::FQuaternion OvCore::Animation::Retarget::Rotation(const RetargetNode& p_node, const OvMaths::FQuaternion& p_clipRotation)
{
	return p_node.turned ? p_node.pre * p_clipRotation * p_node.post : p_clipRotation;
}
