/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cmath>
#include <numbers>
#include <vector>

#include <doctest/doctest.h>

#include <OvCore/Animation/LookAtSolver.h>
#include <OvCore/Animation/SpringSolver.h>

using OvCore::Animation::LookAtSettings;
using OvCore::Animation::LookAtSolver;
using OvCore::Animation::SpringCapsule;
using OvCore::Animation::SpringChain;
using OvCore::Animation::SpringGroup;
using OvCore::Animation::SpringSettings;
using OvCore::Animation::SpringSolver;
using OvMaths::FQuaternion;
using OvMaths::FVector3;

namespace
{
	constexpr float kPi = std::numbers::pi_v<float>;
	constexpr float kDegrees = kPi / 180.0f;

	bool SameRotation(const FQuaternion& p_a, const FQuaternion& p_b, float p_tolerance = 1e-4f)
	{
		return std::abs(std::abs(FQuaternion::DotProduct(FQuaternion::Normalize(p_a), FQuaternion::Normalize(p_b))) - 1.0f) <= p_tolerance;
	}

	bool Near(const FVector3& p_a, const FVector3& p_b, float p_tolerance)
	{
		return FVector3::Distance(p_a, p_b) <= p_tolerance;
	}

	// A chain of p_count bones hanging straight down, each p_length long: a bone points along its +Y, turned upside down
	SpringChain HangingChain(size_t p_count, float p_length)
	{
		SpringChain chain;
		for (size_t i = 0; i < p_count; ++i)
		{
			chain.lengths.push_back(p_length);
			chain.binds.push_back(i == 0 ? FQuaternion(FVector3{ 180.0f, 0.0f, 0.0f }) : FQuaternion::Identity);
			chain.axes.push_back({ 0.0f, 1.0f, 0.0f });
		}
		return chain;
	}

	SpringGroup Group(std::vector<SpringChain> p_chains, SpringSettings p_settings = {})
	{
		SpringGroup group;
		group.settings = p_settings;
		group.chains = std::move(p_chains);
		return group;
	}

	// Anchors every chain of the group at p_anchors (one per chain), parent unturned, and puts it at rest there
	void Start(SpringGroup& p_group, const std::vector<FVector3>& p_anchors)
	{
		for (size_t k = 0; k < p_group.chains.size(); ++k)
		{
			SpringSolver::SetAnchor(p_group.chains[k], p_anchors[k], FQuaternion::Identity);
			SpringSolver::Reset(p_group.chains[k], 1.0f);
		}
		SpringSolver::MeasureLinks(p_group, 1.0f);
	}

	// Moves every anchor by p_move and steps the group by p_deltaTime
	void Advance(SpringGroup& p_group, const FVector3& p_move, float p_deltaTime, const std::vector<SpringCapsule>& p_capsules = {}, std::optional<float> p_ground = std::nullopt)
	{
		for (auto& chain : p_group.chains)
		{
			SpringSolver::SetAnchor(chain, chain.anchor1 + p_move, chain.parent1);
		}
		SpringSolver::Step(p_group, p_capsules, p_ground, 1.0f, p_deltaTime);
	}

	float SegmentLength(const SpringChain& p_chain, size_t p_index)
	{
		const FVector3 head = p_index == 0 ? p_chain.anchor1 : p_chain.positions[p_index - 1];
		return FVector3::Distance(head, p_chain.positions[p_index]);
	}
}

TEST_SUITE("Look at")
{
	TEST_CASE("a look's rotation turns the front towards its yaw and pitch")
	{
		const FVector3 front = { 0.0f, 0.0f, 1.0f };
		CHECK(Near(LookAtSolver::Rotation(90.0f * kDegrees, 0.0f) * front, { 1.0f, 0.0f, 0.0f }, 1e-5f));
		CHECK(Near(LookAtSolver::Rotation(0.0f, 30.0f * kDegrees) * front, { 0.0f, 0.5f, std::cos(30.0f * kDegrees) }, 1e-5f));

		const FVector3 both = LookAtSolver::Rotation(45.0f * kDegrees, 20.0f * kDegrees) * front;
		CHECK(std::atan2(both.x, both.z) == doctest::Approx(45.0f * kDegrees).epsilon(1e-4));
		CHECK(both.y == doctest::Approx(std::sin(20.0f * kDegrees)).epsilon(1e-4));
	}

	TEST_CASE("the look reaches its target without passing it, and goes back to the front without one")
	{
		LookAtSolver solver;
		const LookAtSettings settings;
		const FVector3 target = { std::sin(40.0f * kDegrees), 0.0f, std::cos(40.0f * kDegrees) };

		float previous = 0.0f;
		bool monotonic = true;
		for (int frame = 0; frame < 120; ++frame)
		{
			solver.Step(0.0f, target, 1.0f / 60.0f, settings);
			monotonic = monotonic && solver.GetYaw() >= previous - 1e-6f && solver.GetYaw() <= 40.0f * kDegrees + 1e-5f;
			previous = solver.GetYaw();
		}

		CHECK(monotonic);
		CHECK(solver.GetYaw() == doctest::Approx(40.0f * kDegrees).epsilon(1e-3));
		CHECK(solver.GetPitch() == doctest::Approx(0.0f).epsilon(1e-4));

		for (int frame = 0; frame < 240; ++frame)
		{
			solver.Step(0.0f, std::nullopt, 1.0f / 60.0f, settings);
		}
		CHECK(solver.IsAtRest());
	}

	TEST_CASE("the bounds hold the look: behind is as far as the yaw goes, the pitch within its own")
	{
		LookAtSolver solver;
		LookAtSettings settings;
		settings.turnTime = 0.0f;

		solver.Step(0.0f, FVector3{ 0.3f, 0.0f, -1.0f }, 1.0f / 60.0f, settings);
		CHECK(solver.GetYaw() == doctest::Approx(70.0f * kDegrees).epsilon(1e-4));

		solver.Step(0.0f, FVector3{ 0.0f, 5.0f, 1.0f }, 1.0f / 60.0f, settings);
		CHECK(solver.GetPitch() == doctest::Approx(40.0f * kDegrees).epsilon(1e-4));

		solver.Step(0.0f, FVector3{ 0.0f, -5.0f, 1.0f }, 1.0f / 60.0f, settings);
		CHECK(solver.GetPitch() == doctest::Approx(-30.0f * kDegrees).epsilon(1e-4));

		// Straight above: the yaw stays where it was
		solver.Step(0.0f, FVector3{ 0.3f, 0.0f, 1.0f }, 1.0f / 60.0f, settings);
		const float yaw = solver.GetYaw();
		solver.Step(0.0f, FVector3{ 0.0f, 1.0f, 0.0f }, 1.0f / 60.0f, settings);
		CHECK(solver.GetPitch() == doctest::Approx(40.0f * kDegrees).epsilon(1e-4));
		CHECK(solver.GetYaw() == doctest::Approx(yaw).epsilon(1e-4));
	}

	TEST_CASE("a speed limit slows the turn")
	{
		LookAtSettings fast;
		LookAtSettings slow;
		slow.maxTurnSpeed = 30.0f;

		LookAtSolver a, b;
		const FVector3 target = { 1.0f, 0.0f, 0.2f };
		for (int frame = 0; frame < 6; ++frame)
		{
			a.Step(0.0f, target, 1.0f / 60.0f, fast);
			b.Step(0.0f, target, 1.0f / 60.0f, slow);
		}
		CHECK(std::abs(b.GetYaw()) < std::abs(a.GetYaw()));
	}

	TEST_CASE("while it has a target, the look holds its direction as the body turns under it")
	{
		LookAtSolver solver;
		LookAtSettings settings;
		settings.turnTime = 0.0f;

		// Looking 30 degrees right; the body then turns 20 degrees right, the target staying where it was in the world
		solver.Step(0.0f, FVector3{ std::sin(30.0f * kDegrees), 0.0f, std::cos(30.0f * kDegrees) }, 1.0f / 60.0f, settings);
		CHECK(solver.GetYaw() == doctest::Approx(30.0f * kDegrees).epsilon(1e-4));

		// Paused (no time), the turn alone: the yaw relative to the body drops by the body's turn
		solver.Step(20.0f * kDegrees, FVector3{ std::sin(10.0f * kDegrees), 0.0f, std::cos(10.0f * kDegrees) }, 0.0f, settings);
		CHECK(solver.GetYaw() == doctest::Approx(10.0f * kDegrees).epsilon(1e-4));
	}

	TEST_CASE("each bone takes its part: the tip looks the whole way, the bones before it a share")
	{
		// Three bones in a column, each turned a little at rest, globals and locals consistent
		const std::vector<FQuaternion> locals = {
			FQuaternion(FVector3{ 5.0f, 0.0f, 0.0f }),
			FQuaternion(FVector3{ 0.0f, 10.0f, 0.0f }),
			FQuaternion(FVector3{ -8.0f, 0.0f, 3.0f }),
		};
		const std::vector<FQuaternion> globals = { locals[0], locals[0] * locals[1], locals[0] * locals[1] * locals[2] };
		const std::vector<float> parts = { 0.2f, 0.3f, 0.5f };
		const float yaw = 50.0f * kDegrees;
		const float pitch = 20.0f * kDegrees;

		std::vector<FQuaternion> result;
		LookAtSolver::Distribute(globals, locals, parts, yaw, pitch, result);
		REQUIRE(result.size() == 3);

		const FQuaternion g0 = result[0];
		const FQuaternion g1 = g0 * result[1];
		const FQuaternion g2 = g1 * result[2];
		CHECK(SameRotation(g0, LookAtSolver::Rotation(yaw * 0.2f, pitch * 0.2f) * globals[0]));
		CHECK(SameRotation(g1, LookAtSolver::Rotation(yaw * 0.5f, pitch * 0.5f) * globals[1]));
		CHECK(SameRotation(g2, LookAtSolver::Rotation(yaw, pitch) * globals[2]));

		// No look leaves the pose alone
		LookAtSolver::Distribute(globals, locals, parts, 0.0f, 0.0f, result);
		for (size_t i = 0; i < 3; ++i)
		{
			CHECK(SameRotation(result[i], locals[i]));
		}
	}
}

TEST_SUITE("Spring bones")
{
	TEST_CASE("a chain at rest under a still anchor stays at rest")
	{
		SpringGroup group = Group({ HangingChain(4, 0.25f) });
		Start(group, { { 0.0f, 2.0f, 0.0f } });
		const auto rest = group.chains[0].positions;

		for (int frame = 0; frame < 120; ++frame)
		{
			Advance(group, FVector3::Zero, 1.0f / 60.0f);
		}

		for (size_t i = 0; i < rest.size(); ++i)
		{
			CHECK(Near(group.chains[0].positions[i], rest[i], 1e-4f));
		}
		CHECK(Near(rest.back(), { 0.0f, 1.0f, 0.0f }, 1e-5f));
	}

	TEST_CASE("a body that starts off leaves the chain behind, and it comes back under once the body stops")
	{
		SpringGroup group = Group({ HangingChain(4, 0.25f) });
		Start(group, { { 0.0f, 2.0f, 0.0f } });

		for (int frame = 0; frame < 15; ++frame)
		{
			Advance(group, { 0.05f, 0.0f, 0.0f }, 1.0f / 60.0f);
		}

		const auto& chain = group.chains[0];
		CHECK(chain.positions.back().x < chain.anchor1.x - 0.05f);

		for (int frame = 0; frame < 600; ++frame)
		{
			Advance(group, FVector3::Zero, 1.0f / 60.0f);
		}
		CHECK(std::abs(chain.positions.back().x - chain.anchor1.x) < 0.01f);
	}

	TEST_CASE("segments keep their lengths, and never stray past the maximum angle")
	{
		SpringSettings settings;
		settings.maxAngle = 30.0f;
		settings.stiffness = 5.0f;
		SpringGroup group = Group({ HangingChain(3, 0.3f) }, settings);
		Start(group, { { 0.0f, 3.0f, 0.0f } });

		bool lengths = true;
		bool angles = true;
		for (int frame = 0; frame < 90; ++frame)
		{
			Advance(group, { frame < 30 ? 0.2f : -0.2f, 0.0f, 0.0f }, 1.0f / 60.0f);
			const auto& chain = group.chains[0];

			FVector3 rest = chain.parent1 * FVector3{ 0.0f, 1.0f, 0.0f };
			FQuaternion parent = chain.parent1;
			for (size_t i = 0; i < chain.positions.size(); ++i)
			{
				lengths = lengths && std::abs(SegmentLength(chain, i) - 0.3f) < 1e-4f;

				// The segment against its rest, from the parent segment as simulated
				const FQuaternion restRotation = parent * chain.binds[i];
				rest = FQuaternion::RotatePoint(chain.axes[i], restRotation);
				const FVector3 head = i == 0 ? chain.anchor1 : chain.positions[i - 1];
				const FVector3 segment = FVector3::Normalize(chain.positions[i] - head);
				angles = angles && FVector3::Dot(segment, rest) >= std::cos(30.0f * kDegrees) - 1e-4f;
				parent = chain.rotations[i];
			}
		}

		CHECK(lengths);
		CHECK(angles);
	}

	TEST_CASE("joints stay out of a capsule, and above the ground, which wins")
	{
		SpringSettings settings;
		settings.radius = 0.02f;

		// A capsule across the chain, just beside its second joint: the joint goes round it. A joint is pushed out, then
		// brought back to its segment's length from the joint before, so it may end a hair inside
		const SpringCapsule across{ { 0.06f, 1.5f, -1.0f }, { 0.06f, 1.5f, 1.0f }, { 0.06f, 1.5f, -1.0f }, { 0.06f, 1.5f, 1.0f }, 0.1f };
		SpringGroup still = Group({ HangingChain(4, 0.25f) }, settings);
		Start(still, { { 0.0f, 2.0f, 0.0f } });

		bool outside = true;
		for (int frame = 0; frame < 120; ++frame)
		{
			Advance(still, FVector3::Zero, 1.0f / 60.0f, { across });
			for (const auto& position : still.chains[0].positions)
			{
				const float dx = position.x - 0.06f;
				const float dy = position.y - 1.5f;
				outside = outside && std::sqrt(dx * dx + dy * dy) >= 0.1f + 0.02f - 0.003f;
			}
		}
		CHECK(outside);
		CHECK(still.chains[0].positions[1].x < -0.05f);

		// A ground above the chain's end lifts it: no joint under the ground plus the radius
		SpringGroup grounded = Group({ HangingChain(4, 0.25f) }, settings);
		Start(grounded, { { 0.0f, 2.0f, 0.0f } });
		bool above = true;
		for (int frame = 0; frame < 120; ++frame)
		{
			Advance(grounded, FVector3::Zero, 1.0f / 60.0f, {}, 1.5f);
			for (const auto& position : grounded.chains[0].positions)
			{
				above = above && position.y >= 1.5f + 0.02f - 1e-4f;
			}
		}
		CHECK(above);

		// The ground wins over a capsule that pushes a joint down
		const SpringCapsule overhead{ { 0.0f, 1.62f, -1.0f }, { 0.0f, 1.62f, 1.0f }, { 0.0f, 1.62f, -1.0f }, { 0.0f, 1.62f, 1.0f }, 0.05f };
		SpringGroup both = Group({ HangingChain(4, 0.25f) }, settings);
		Start(both, { { 0.0f, 2.0f, 0.0f } });
		bool ground = true;
		for (int frame = 0; frame < 120; ++frame)
		{
			Advance(both, FVector3::Zero, 1.0f / 60.0f, { overhead }, 1.5f);
			for (const auto& position : both.chains[0].positions)
			{
				ground = ground && position.y >= 1.5f + 0.02f - 1e-4f;
			}
		}
		CHECK(ground);
	}

	TEST_CASE("a reset puts the chain back at rest where the anchor is")
	{
		SpringGroup group = Group({ HangingChain(4, 0.25f) });
		Start(group, { { 0.0f, 2.0f, 0.0f } });
		for (int frame = 0; frame < 20; ++frame)
		{
			Advance(group, { 0.05f, 0.0f, 0.0f }, 1.0f / 60.0f);
		}

		// A jump far away, then a reset: at rest under the new anchor, and still there a frame on
		SpringSolver::SetAnchor(group.chains[0], { 10.0f, 2.0f, 0.0f }, FQuaternion::Identity);
		SpringSolver::Reset(group.chains[0], 1.0f);
		CHECK(SpringSolver::GetAnchorMove(group.chains[0]) == doctest::Approx(0.0f));
		CHECK(Near(group.chains[0].positions.back(), { 10.0f, 1.0f, 0.0f }, 1e-5f));

		Advance(group, FVector3::Zero, 1.0f / 60.0f);
		CHECK(Near(group.chains[0].positions.back(), { 10.0f, 1.0f, 0.0f }, 1e-4f));
		for (size_t i = 0; i < 4; ++i)
		{
			CHECK(SameRotation(SpringSolver::GetLocal(group.chains[0], i), group.chains[0].binds[i], 1e-5f));
		}
	}

	TEST_CASE("linked chains keep their spacing as one cloth")
	{
		SpringSettings loose;
		loose.stiffness = 2.0f;
		SpringSettings linked = loose;
		linked.link = 1.0f;

		const auto spread = [](SpringSettings p_settings)
		{
			SpringGroup group = Group({ HangingChain(4, 0.25f), HangingChain(4, 0.25f) }, p_settings);
			Start(group, { { 0.0f, 2.0f, 0.0f }, { 0.2f, 2.0f, 0.0f } });

			// The second chain's anchor alone swings away and back
			float worst = 0.0f;
			for (int frame = 0; frame < 60; ++frame)
			{
				auto& a = group.chains[0];
				auto& b = group.chains[1];
				SpringSolver::SetAnchor(a, a.anchor1, a.parent1);
				SpringSolver::SetAnchor(b, b.anchor1 + FVector3{ 0.0f, 0.0f, frame < 20 ? 0.05f : 0.0f }, b.parent1);
				SpringSolver::Step(group, {}, std::nullopt, 1.0f, 1.0f / 60.0f);
				worst = std::max(worst, FVector3::Distance(a.positions.back(), b.positions.back()));
			}
			return worst;
		};

		CHECK(spread(linked) < spread(loose));
	}

	TEST_CASE("the same motion at 30 and at 144 frames a second ends in nearly the same place")
	{
		const auto run = [](int p_rate)
		{
			SpringGroup group = Group({ HangingChain(4, 0.25f) });
			Start(group, { { 0.0f, 2.0f, 0.0f } });
			const float dt = 1.0f / static_cast<float>(p_rate);
			for (int frame = 0; frame < p_rate; ++frame)
			{
				const float t = static_cast<float>(frame) * dt;
				Advance(group, { t < 0.5f ? 1.5f * dt : 0.0f, 0.0f, 0.0f }, dt);
			}
			return group.chains[0].positions.back();
		};

		CHECK(Near(run(30), run(144), 0.02f));
	}
}
