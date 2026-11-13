/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <numbers>

#include <OvCore/Animation/SpringSolver.h>

namespace
{
	using OvMaths::FQuaternion;
	using OvMaths::FVector3;

	constexpr float kMinLength = 0.000001f;
	constexpr int kMaxSteps = 8;
	constexpr float kStepRate = 120.0f;
	constexpr float kGravity = 9.81f;

	FQuaternion NormalizeOrIdentity(const FQuaternion& p_q)
	{
		const float length = std::sqrt(p_q.x * p_q.x + p_q.y * p_q.y + p_q.z * p_q.z + p_q.w * p_q.w);
		return length <= kMinLength ? FQuaternion::Identity : FQuaternion(p_q.x / length, p_q.y / length, p_q.z / length, p_q.w / length);
	}

	// Between two rotations, the short way round, normalised
	FQuaternion NLerp(const FQuaternion& p_a, FQuaternion p_b, float p_t)
	{
		if (p_a.x * p_b.x + p_a.y * p_b.y + p_a.z * p_b.z + p_a.w * p_b.w < 0.0f)
		{
			p_b = { -p_b.x, -p_b.y, -p_b.z, -p_b.w };
		}

		return NormalizeOrIdentity({
			p_a.x + (p_b.x - p_a.x) * p_t,
			p_a.y + (p_b.y - p_a.y) * p_t,
			p_a.z + (p_b.z - p_a.z) * p_t,
			p_a.w + (p_b.w - p_a.w) * p_t
		});
	}

	// The shortest turn bringing the unit vector a onto the unit vector b; half a turn round a normal of a when opposed
	FQuaternion FromTo(const FVector3& p_a, const FVector3& p_b)
	{
		const float d = FVector3::Dot(p_a, p_b);

		if (d < -0.999999f)
		{
			FVector3 normal = std::abs(p_a.x) < 0.9f ? FVector3{ 0.0f, p_a.z, -p_a.y } : FVector3{ -p_a.z, 0.0f, p_a.x };
			normal = FVector3::Normalize(normal);
			return { normal.x, normal.y, normal.z, 0.0f };
		}

		const FVector3 cross = FVector3::Cross(p_a, p_b);
		return NormalizeOrIdentity({ cross.x, cross.y, cross.z, 1.0f + d });
	}

	FVector3 NormalizeOr(const FVector3& p_v, const FVector3& p_fallback)
	{
		const float length = FVector3::Length(p_v);
		return length <= kMinLength ? p_fallback : p_v / length;
	}

	// A capsule placed part of the way through a frame, and how near a joint may come to its axis
	struct PlacedCapsule
	{
		FVector3 start;
		FVector3 end;
		FVector3 center;
		float reach = 0.0f;
		float boundSquared = 0.0f;
	};

	void Place(const std::vector<OvCore::Animation::SpringCapsule>& p_capsules, float p_alpha, float p_radius, std::vector<PlacedCapsule>& p_placed)
	{
		p_placed.clear();

		for (const auto& capsule : p_capsules)
		{
			PlacedCapsule placed;
			placed.start = capsule.start0 + (capsule.start1 - capsule.start0) * p_alpha;
			placed.end = capsule.end0 + (capsule.end1 - capsule.end0) * p_alpha;
			placed.center = (placed.start + placed.end) * 0.5f;
			placed.reach = capsule.radius + p_radius;
			const float bound = 0.5f * FVector3::Distance(placed.start, placed.end) + placed.reach;
			placed.boundSquared = bound * bound;
			p_placed.push_back(placed);
		}
	}

	// The point pushed out of every capsule placed
	FVector3 Collide(FVector3 p_point, const std::vector<PlacedCapsule>& p_placed)
	{
		for (const auto& capsule : p_placed)
		{
			const FVector3 fromCenter = p_point - capsule.center;
			if (FVector3::Dot(fromCenter, fromCenter) >= capsule.boundSquared)
			{
				continue;
			}

			const FVector3 axis = capsule.end - capsule.start;
			const float axisLength = FVector3::Dot(axis, axis);
			const float t = axisLength <= kMinLength ? 0.0f : std::clamp(FVector3::Dot(p_point - capsule.start, axis) / axisLength, 0.0f, 1.0f);
			const FVector3 closest = capsule.start + axis * t;
			const FVector3 away = p_point - closest;
			const float distance = FVector3::Length(away);

			if (distance < capsule.reach && distance > kMinLength)
			{
				p_point = closest + away / distance * capsule.reach;
			}
		}

		return p_point;
	}

	// The direction from a joint at height p_height, lowered no further than puts its end, p_length on, at p_floor
	FVector3 AboveGround(float p_height, const FVector3& p_direction, float p_length, float p_floor)
	{
		if (p_height + p_direction.y * p_length >= p_floor)
		{
			return p_direction;
		}

		const float up = std::clamp((p_floor - p_height) / p_length, -1.0f, 1.0f);
		const float flat = std::sqrt(p_direction.x * p_direction.x + p_direction.z * p_direction.z);
		const float across = std::sqrt(std::max(1.0f - up * up, 0.0f));

		if (flat <= kMinLength)
		{
			return { across, up, 0.0f };
		}

		return { p_direction.x / flat * across, up, p_direction.z / flat * across };
	}

	// What a row of one sub-step keeps per chain: the joint it hangs from and that joint's rotation, its rest
	// rotation and direction, and where it is predicted to go
	struct RowState
	{
		FVector3 head;
		FQuaternion rotation;
		FQuaternion rest;
		FVector3 direction;
		FVector3 predicted;
	};
}

void OvCore::Animation::SpringSolver::SetAnchor(SpringChain& p_chain, const FVector3& p_anchor, const FQuaternion& p_parentRotation)
{
	p_chain.anchor0 = p_chain.anchor1;
	p_chain.parent0 = p_chain.parent1;
	p_chain.anchor1 = p_anchor;
	p_chain.parent1 = p_parentRotation;
}

float OvCore::Animation::SpringSolver::GetAnchorMove(const SpringChain& p_chain)
{
	return FVector3::Distance(p_chain.anchor0, p_chain.anchor1);
}

void OvCore::Animation::SpringSolver::Reset(SpringChain& p_chain, float p_scale)
{
	const size_t count = p_chain.lengths.size();
	p_chain.positions.resize(count);
	p_chain.previous.resize(count);
	p_chain.rotations.resize(count);
	p_chain.linkRest.clear();

	p_chain.anchor0 = p_chain.anchor1;
	p_chain.parent0 = p_chain.parent1;

	FVector3 head = p_chain.anchor1;
	FQuaternion rotation = p_chain.parent1;

	for (size_t i = 0; i < count; ++i)
	{
		rotation = rotation * p_chain.binds[i];
		head = head + FQuaternion::RotatePoint(p_chain.axes[i], rotation) * (p_chain.lengths[i] * p_scale);
		p_chain.positions[i] = head;
		p_chain.previous[i] = head;
		p_chain.rotations[i] = rotation;
	}
}

void OvCore::Animation::SpringSolver::MeasureLinks(SpringGroup& p_group, float p_scale)
{
	for (size_t k = 0; k + 1 < p_group.chains.size(); ++k)
	{
		auto& a = p_group.chains[k];
		const auto& b = p_group.chains[k + 1];
		const size_t count = std::min(a.positions.size(), b.positions.size());

		a.linkRest.resize(count);
		for (size_t i = 0; i < count; ++i)
		{
			a.linkRest[i] = FVector3::Distance(a.positions[i], b.positions[i]) / p_scale;
		}
	}
}

void OvCore::Animation::SpringSolver::Step(SpringGroup& p_group, const std::vector<SpringCapsule>& p_capsules, std::optional<float> p_ground, float p_scale, float p_deltaTime)
{
	auto& chains = p_group.chains;
	const auto& settings = p_group.settings;

	if (p_deltaTime <= 0.0f || chains.empty())
	{
		return;
	}

	const int steps = std::min(kMaxSteps, std::max(1, static_cast<int>(std::ceil(p_deltaTime * kStepRate - 0.001f))));
	const float h = p_deltaTime / static_cast<float>(steps);
	const float h2 = h * h;
	const float keep = std::exp(-settings.damping * h);
	const float radius = settings.radius * p_scale;
	const float weight = settings.gravity * kGravity;
	const float maxAngle = settings.maxAngle * std::numbers::pi_v<float> / 180.0f;
	const float cosMax = std::cos(maxAngle);
	const float sinMax = std::sin(maxAngle);

	size_t rows = 0;
	for (const auto& chain : chains)
	{
		rows = std::max(rows, chain.lengths.size());
	}

	std::vector<RowState> state(chains.size());
	std::vector<PlacedCapsule> placed;

	for (int j = 1; j <= steps; ++j)
	{
		const float alpha = static_cast<float>(j) / static_cast<float>(steps);

		for (size_t k = 0; k < chains.size(); ++k)
		{
			auto& chain = chains[k];
			state[k].head = chain.anchor0 + (chain.anchor1 - chain.anchor0) * alpha;
			state[k].rotation = NLerp(chain.parent0, chain.parent1, alpha);

			// The body's own move over this sub-step, which the joints' damping leaves alone
			chain.bodyMove = (chain.anchor1 - chain.anchor0) / static_cast<float>(steps);
		}

		const float ratio = p_group.previousStep > 0.0f ? h / p_group.previousStep : 1.0f;

		Place(p_capsules, alpha, radius, placed);

		for (size_t i = 0; i < rows; ++i)
		{
			// Predicted
			for (size_t k = 0; k < chains.size(); ++k)
			{
				auto& chain = chains[k];
				if (i >= chain.lengths.size())
				{
					continue;
				}

				auto& row = state[k];
				row.rest = row.rotation * chain.binds[i];
				row.direction = FQuaternion::RotatePoint(chain.axes[i], row.rest);

				const float length = chain.lengths[i] * p_scale;
				const FVector3& position = chain.positions[i];
				const FVector3& body = chain.bodyMove;
				const FVector3 velocity = (position - chain.previous[i]) * ratio;
				const FVector3 pull = row.direction * (-weight * row.direction.y);

				row.predicted =
					position + body + (velocity - body) * keep +
					((row.head + row.direction * length - position - body) * settings.stiffness + pull) * h2;
			}

			// Held at their spacing, on stretch only
			if (settings.link > 0.0f)
			{
				for (size_t k = 0; k + 1 < chains.size(); ++k)
				{
					if (i >= chains[k].linkRest.size() || i >= chains[k + 1].lengths.size())
					{
						continue;
					}

					const FVector3 between = state[k + 1].predicted - state[k].predicted;
					const float distance = FVector3::Length(between);
					const float excess = distance - chains[k].linkRest[i] * p_scale;

					if (excess > 0.0f && distance > kMinLength)
					{
						const float share = 0.5f * settings.link * excess / distance;
						state[k].predicted = state[k].predicted + between * share;
						state[k + 1].predicted = state[k + 1].predicted - between * share;
					}
				}
			}

			// Its length, the colliders, the ground, its angle; then its rotation, and the next row hangs from it
			for (size_t k = 0; k < chains.size(); ++k)
			{
				auto& chain = chains[k];
				if (i >= chain.lengths.size())
				{
					continue;
				}

				auto& row = state[k];
				const float length = chain.lengths[i] * p_scale;

				FVector3 direction = NormalizeOr(row.predicted - row.head, row.direction);
				const FVector3 pushed = Collide(row.head + direction * length, placed);
				direction = NormalizeOr(pushed - row.head, row.direction);

				const float dot = FVector3::Dot(row.direction, direction);
				if (dot < cosMax)
				{
					const FVector3 side = direction - row.direction * dot;
					const float sideLength = FVector3::Length(side);
					direction = sideLength > kMinLength ? row.direction * cosMax + side / sideLength * sinMax : row.direction;
				}

				if (p_ground)
				{
					direction = AboveGround(row.head.y, direction, length, *p_ground + radius);
				}

				const FVector3 position = row.head + direction * length;
				chain.previous[i] = chain.positions[i];
				chain.positions[i] = position;
				chain.rotations[i] = NormalizeOrIdentity(FromTo(row.direction, direction) * row.rest);

				row.head = position;
				row.rotation = chain.rotations[i];
			}
		}

		p_group.previousStep = h;
	}
}

OvMaths::FQuaternion OvCore::Animation::SpringSolver::GetLocal(const SpringChain& p_chain, size_t p_index)
{
	const FQuaternion& parent = p_index == 0 ? p_chain.parent1 : p_chain.rotations[p_index - 1];
	return NormalizeOrIdentity(FQuaternion::Conjugate(parent) * p_chain.rotations[p_index]);
}
