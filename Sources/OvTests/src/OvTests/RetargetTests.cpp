/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cmath>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <OvCore/Animation/Retarget.h>
#include <OvRendering/Animation/SkeletalData.h>

using OvCore::Animation::BoneMap;
using OvCore::Animation::Retarget;
using OvCore::Animation::RetargetNode;
using OvMaths::FQuaternion;
using OvMaths::FVector3;
using OvRendering::Animation::Skeleton;

namespace
{
	struct NodeSpec
	{
		std::string name;
		int32_t parent = -1;
		FVector3 position;
		FQuaternion rotation = FQuaternion::Identity;
		FVector3 scale = { 1.0f, 1.0f, 1.0f };
		bool bone = true;
	};

	Skeleton MakeSkeleton(const std::vector<NodeSpec>& p_specs)
	{
		Skeleton skeleton;

		for (const auto& spec : p_specs)
		{
			const auto index = static_cast<uint32_t>(skeleton.nodes.size());
			auto& node = skeleton.nodes.emplace_back();
			node.name = spec.name;
			node.parentIndex = spec.parent;
			node.bindPosition = spec.position;
			node.bindRotation = spec.rotation;
			node.bindScale = spec.scale;
			skeleton.nodeByName.emplace(spec.name, index);

			if (spec.bone)
			{
				node.boneIndex = static_cast<int32_t>(skeleton.bones.size());
				skeleton.boneByName.emplace(spec.name, static_cast<uint32_t>(skeleton.bones.size()));
				skeleton.bones.push_back({ spec.name, index });
			}
		}

		return skeleton;
	}

	// An armature (no bone) holding a column of three bones a metre apart, at rest without rotation
	Skeleton Column(const std::vector<std::string>& p_names = { "root", "mid", "tip" }, float p_midLength = 1.0f)
	{
		return MakeSkeleton({
			{ "Armature", -1, {}, FQuaternion::Identity, { 1.0f, 1.0f, 1.0f }, false },
			{ p_names[0], 0, { 0.0f, 0.0f, 0.0f } },
			{ p_names[1], 1, { 0.0f, 1.0f, 0.0f } },
			{ p_names[2], 2, { 0.0f, p_midLength, 0.0f } },
		});
	}

	// The same column, each bone's rest turned another way while its joint stands where the column's does
	Skeleton TurnedColumn()
	{
		const FQuaternion g1 = FQuaternion(FVector3{ 0.0f, 90.0f, 0.0f });
		const FQuaternion g2 = FQuaternion(FVector3{ -90.0f, 0.0f, 30.0f });
		const FQuaternion g3 = FQuaternion(FVector3{ 180.0f, 0.0f, 0.0f });

		return MakeSkeleton({
			{ "Armature", -1, {}, FQuaternion::Identity, { 1.0f, 1.0f, 1.0f }, false },
			{ "root", 0, { 0.0f, 0.0f, 0.0f }, g1 },
			{ "mid", 1, FQuaternion::RotatePoint({ 0.0f, 1.0f, 0.0f }, FQuaternion::Conjugate(g1)), FQuaternion::Conjugate(g1) * g2 },
			{ "tip", 2, FQuaternion::RotatePoint({ 0.0f, 1.0f, 0.0f }, FQuaternion::Conjugate(g2)), FQuaternion::Conjugate(g2) * g3 },
		});
	}

	struct Pose
	{
		std::vector<FVector3> positions;
		std::vector<FQuaternion> rotations;
	};

	// Global positions and rotations from local ones, parents first (they are, in these skeletons)
	Pose Globals(const Skeleton& p_skeleton, const std::vector<FVector3>& p_positions, const std::vector<FQuaternion>& p_rotations)
	{
		Pose pose;
		for (size_t i = 0; i < p_skeleton.nodes.size(); ++i)
		{
			const int32_t parent = p_skeleton.nodes[i].parentIndex;
			if (parent < 0)
			{
				pose.positions.push_back(p_positions[i]);
				pose.rotations.push_back(p_rotations[i]);
			}
			else
			{
				const auto p = static_cast<size_t>(parent);
				pose.positions.push_back(pose.positions[p] + FQuaternion::RotatePoint(p_positions[i], pose.rotations[p]));
				pose.rotations.push_back(pose.rotations[p] * p_rotations[i]);
			}
		}
		return pose;
	}

	Pose RestGlobals(const Skeleton& p_skeleton)
	{
		std::vector<FVector3> positions;
		std::vector<FQuaternion> rotations;
		for (const auto& node : p_skeleton.nodes)
		{
			positions.push_back(node.bindPosition);
			rotations.push_back(node.bindRotation);
		}
		return Globals(p_skeleton, positions, rotations);
	}

	bool Near(const FVector3& p_a, const FVector3& p_b, float p_tolerance = 1e-4f)
	{
		return FVector3::Distance(p_a, p_b) <= p_tolerance;
	}

	bool SameRotation(const FQuaternion& p_a, const FQuaternion& p_b, float p_tolerance = 1e-4f)
	{
		return std::abs(std::abs(FQuaternion::DotProduct(FQuaternion::Normalize(p_a), FQuaternion::Normalize(p_b))) - 1.0f) <= p_tolerance;
	}

	bool Mentions(const std::optional<std::string>& p_error, const std::string& p_text)
	{
		return p_error.has_value() && p_error->find(p_text) != std::string::npos;
	}

	// A clip's local keys on the column: the root rises and turns, mid and tip bend
	void ClipKeys(std::vector<FVector3>& p_positions, std::vector<FQuaternion>& p_rotations, const Skeleton& p_clip)
	{
		p_positions = { {}, { 0.1f, 0.5f, -0.2f }, p_clip.nodes[2].bindPosition, p_clip.nodes[3].bindPosition };
		p_rotations = {
			FQuaternion::Identity,
			FQuaternion(FVector3{ 10.0f, 30.0f, 0.0f }),
			FQuaternion(FVector3{ 0.0f, -20.0f, 45.0f }),
			FQuaternion(FVector3{ -60.0f, 15.0f, 5.0f }),
		};
	}

	// The model's local pose for the clip's keys, as the renderer works it out
	void Carry(const Skeleton& p_model, const std::vector<RetargetNode>& p_nodes, const std::vector<FVector3>& p_clipPositions, const std::vector<FQuaternion>& p_clipRotations, std::vector<FVector3>& p_positions, std::vector<FQuaternion>& p_rotations)
	{
		p_positions.clear();
		p_rotations.clear();
		for (size_t i = 0; i < p_model.nodes.size(); ++i)
		{
			const auto& node = p_model.nodes[i];
			const auto& entry = p_nodes[i];
			if (entry.source < 0)
			{
				p_positions.push_back(node.bindPosition);
				p_rotations.push_back(node.bindRotation);
				continue;
			}
			const auto s = static_cast<size_t>(entry.source);
			p_positions.push_back(Retarget::Position(entry, node.bindPosition, p_clipPositions[s]));
			p_rotations.push_back(Retarget::Rotation(entry, p_clipRotations[s]));
		}
	}
}

TEST_SUITE("Retargeting")
{
	TEST_CASE("the same rig carries every key over unchanged, bit for bit")
	{
		const Skeleton rig = Column();
		std::vector<RetargetNode> nodes;
		REQUIRE_FALSE(Retarget::Build(rig, rig, {}, nodes).has_value());
		REQUIRE(nodes.size() == rig.nodes.size());

		for (size_t i = 0; i < nodes.size(); ++i)
		{
			CHECK(nodes[i].source == static_cast<int32_t>(i));
			CHECK_FALSE(nodes[i].turned);
		}

		// The rotation is the key itself, and the position the arithmetic the renderer always did
		const FQuaternion key = FQuaternion(FVector3{ 12.5f, -40.0f, 3.0f });
		const FQuaternion carried = Retarget::Rotation(nodes[2], key);
		CHECK((carried.x == key.x && carried.y == key.y && carried.z == key.z && carried.w == key.w));

		const FVector3 position = { 0.3f, 1.7f, -0.1f };
		const FVector3 moved = Retarget::Position(nodes[2], rig.nodes[2].bindPosition, position);
		const FVector3 before = position + nodes[2].restOffset;
		CHECK((moved.x == before.x && moved.y == before.y && moved.z == before.z));
	}

	TEST_CASE("bones of other names are matched through the bone map, and refused without it")
	{
		const Skeleton clip = Column();
		const Skeleton model = Column({ "Hip", "Spine", "Head" });
		std::vector<RetargetNode> nodes;

		const auto refused = Retarget::Build(model, clip, {}, nodes);
		CHECK(Mentions(refused, "Hip"));

		REQUIRE_FALSE(Retarget::Build(model, clip, { { "Hip", "root" }, { "Spine", "mid" }, { "Head", "tip" } }, nodes).has_value());
		CHECK(nodes[1].source == 1);
		CHECK(nodes[2].source == 2);
		CHECK(nodes[3].source == 3);
		CHECK_FALSE(nodes[3].turned);

		// A partial map leaves the other bones to their own names
		const Skeleton half = Column({ "Hip", "mid", "tip" });
		CHECK_FALSE(Retarget::Build(half, clip, { { "Hip", "root" } }, nodes).has_value());
	}

	TEST_CASE("rests turned otherwise: the model turns in its space as the clip does in its own")
	{
		const Skeleton clip = Column();
		const Skeleton model = TurnedColumn();
		std::vector<RetargetNode> nodes;
		REQUIRE_FALSE(Retarget::Build(model, clip, {}, nodes).has_value());
		CHECK(nodes[1].turned);
		CHECK(nodes[2].turned);
		CHECK(nodes[3].turned);

		// At rest, both columns have their joints in the same places
		const Pose clipRest = RestGlobals(clip);
		const Pose modelRest = RestGlobals(model);
		for (size_t i = 0; i < 4; ++i)
		{
			CHECK(Near(clipRest.positions[i], modelRest.positions[i]));
		}

		std::vector<FVector3> clipPositions, modelPositions;
		std::vector<FQuaternion> clipRotations, modelRotations;
		ClipKeys(clipPositions, clipRotations, clip);
		Carry(model, nodes, clipPositions, clipRotations, modelPositions, modelRotations);

		const Pose clipPose = Globals(clip, clipPositions, clipRotations);
		const Pose modelPose = Globals(model, modelPositions, modelRotations);

		for (size_t i = 1; i < 4; ++i)
		{
			CAPTURE(i);
			CHECK(Near(clipPose.positions[i], modelPose.positions[i]));

			// Each bone turned away from its rest the same way, in world space
			const FQuaternion clipTurn = clipPose.rotations[i] * FQuaternion::Conjugate(clipRest.rotations[i]);
			const FQuaternion modelTurn = modelPose.rotations[i] * FQuaternion::Conjugate(modelRest.rotations[i]);
			CHECK(SameRotation(clipTurn, modelTurn));
		}

		// No key at all leaves the model at its rest
		std::vector<FVector3> restPositions, carriedPositions;
		std::vector<FQuaternion> restRotations, carriedRotations;
		for (const auto& node : clip.nodes)
		{
			restPositions.push_back(node.bindPosition);
			restRotations.push_back(node.bindRotation);
		}
		Carry(model, nodes, restPositions, restRotations, carriedPositions, carriedRotations);
		for (size_t i = 1; i < 4; ++i)
		{
			CHECK(Near(carriedPositions[i], model.nodes[i].bindPosition));
			CHECK(SameRotation(carriedRotations[i], model.nodes[i].bindRotation));
		}
	}

	TEST_CASE("a longer bone moves further, by the ratio of the rest lengths")
	{
		const Skeleton clip = Column();
		const Skeleton model = Column({ "root", "mid", "tip" }, 2.0f);
		std::vector<RetargetNode> nodes;
		REQUIRE_FALSE(Retarget::Build(model, clip, {}, nodes).has_value());
		CHECK(nodes[3].lengthRatio == doctest::Approx(2.0f));
		CHECK_FALSE(nodes[3].turned);

		// The clip stretches tip from 1 to 1.2: the model's, twice as long at rest, from 2 to 2.4
		const FVector3 moved = Retarget::Position(nodes[3], model.nodes[3].bindPosition, { 0.0f, 1.2f, 0.0f });
		CHECK(Near(moved, { 0.0f, 2.4f, 0.0f }));
	}

	TEST_CASE("a model's own bones under a shared bone are driven by nothing")
	{
		const Skeleton clip = Column();
		const Skeleton model = MakeSkeleton({
			{ "Armature", -1, {}, FQuaternion::Identity, { 1.0f, 1.0f, 1.0f }, false },
			{ "root", 0, {} },
			{ "mid", 1, { 0.0f, 1.0f, 0.0f } },
			{ "tip", 2, { 0.0f, 1.0f, 0.0f } },
			{ "hair.a.1", 3, { 0.0f, 0.2f, 0.0f } },
			{ "hair.a.2", 4, { 0.0f, 0.2f, 0.0f } },
		});
		std::vector<RetargetNode> nodes;
		REQUIRE_FALSE(Retarget::Build(model, clip, {}, nodes).has_value());
		CHECK(nodes[4].source == -1);
		CHECK(nodes[5].source == -1);

		// But never right under the armature, where a misnamed bone would hide
		const Skeleton loose = MakeSkeleton({
			{ "Armature", -1, {}, FQuaternion::Identity, { 1.0f, 1.0f, 1.0f }, false },
			{ "root", 0, {} },
			{ "mid", 1, { 0.0f, 1.0f, 0.0f } },
			{ "tip", 2, { 0.0f, 1.0f, 0.0f } },
			{ "stray", 0, { 1.0f, 0.0f, 0.0f } },
		});
		CHECK(Mentions(Retarget::Build(loose, clip, {}, nodes), "stray"));
	}

	TEST_CASE("what cannot be carried over is refused, naming the bone")
	{
		const Skeleton clip = Column();
		std::vector<RetargetNode> nodes;

		SUBCASE("another rest scale")
		{
			Skeleton model = Column();
			model.nodes[2].bindScale = { 1.5f, 1.5f, 1.5f };
			CHECK(Mentions(Retarget::Build(model, clip, {}, nodes), "'mid'"));
			CHECK(nodes.size() == model.nodes.size());
		}

		SUBCASE("another parent")
		{
			const Skeleton model = MakeSkeleton({
				{ "Armature", -1, {}, FQuaternion::Identity, { 1.0f, 1.0f, 1.0f }, false },
				{ "root", 0, {} },
				{ "mid", 1, { 0.0f, 1.0f, 0.0f } },
				{ "tip", 1, { 0.0f, 2.0f, 0.0f } },
			});
			CHECK(Mentions(Retarget::Build(model, clip, {}, nodes), "'tip'"));
		}

		SUBCASE("a clip bone driving two of the model's")
		{
			const Skeleton model = Column({ "Hip", "Spine", "Head" });
			CHECK(Mentions(Retarget::Build(model, clip, { { "Hip", "root" }, { "Spine", "mid" }, { "Head", "mid" } }, nodes), "'mid'"));
		}

		SUBCASE("a bone the clip's rig lacks")
		{
			const Skeleton model = Column({ "root", "mid", "head" });
			CHECK(Mentions(Retarget::Build(model, clip, {}, nodes), "'head'"));
		}

		SUBCASE("no bones at all")
		{
			CHECK(Retarget::Build(Skeleton{}, clip, {}, nodes).has_value());
			CHECK(Retarget::Build(clip, Skeleton{}, {}, nodes).has_value());
		}
	}
}
