/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>

#include <OvCore/Particles/ParticleEmitter.h>

namespace
{
	using OvMaths::FMatrix4;
	using OvMaths::FVector3;
	using OvMaths::FVector4;

	constexpr float kPi = 3.14159265358979f;
	constexpr float kDegreesToRadians = kPi / 180.0f;

	// Below it, drag is none: the formula with drag divides by it
	constexpr float kMinimumDrag = 1e-4f;

	// The clock's origin moves forward once it is this far behind, so that times stay precise as floats on the GPU
	constexpr double kRebaseAfter = 600.0;

	// A frame never runs more cycles than this, whatever their length
	constexpr uint32_t kMaxCyclesPerStep = 64;

	std::atomic<uint64_t> g_seedCounter{ 0x9E3779B97F4A7C15ull };

	FVector3 TransformPoint(const FMatrix4& p_matrix, const FVector3& p_point)
	{
		const float* m = p_matrix.data;
		return {
			m[0] * p_point.x + m[1] * p_point.y + m[2] * p_point.z + m[3],
			m[4] * p_point.x + m[5] * p_point.y + m[6] * p_point.z + m[7],
			m[8] * p_point.x + m[9] * p_point.y + m[10] * p_point.z + m[11]
		};
	}

	FVector3 TransformDirection(const FMatrix4& p_matrix, const FVector3& p_direction)
	{
		const float* m = p_matrix.data;
		return {
			m[0] * p_direction.x + m[1] * p_direction.y + m[2] * p_direction.z,
			m[4] * p_direction.x + m[5] * p_direction.y + m[6] * p_direction.z,
			m[8] * p_direction.x + m[9] * p_direction.y + m[10] * p_direction.z
		};
	}

	FVector3 Origin(const FMatrix4& p_matrix)
	{
		return { p_matrix.data[3], p_matrix.data[7], p_matrix.data[11] };
	}

	FVector3 NormalizeOr(const FVector3& p_vector, const FVector3& p_fallback)
	{
		const float length = std::sqrt(p_vector.x * p_vector.x + p_vector.y * p_vector.y + p_vector.z * p_vector.z);
		return length > 1e-6f ? FVector3{ p_vector.x / length, p_vector.y / length, p_vector.z / length } : p_fallback;
	}

	float Clamp01(float p_value)
	{
		return std::clamp(p_value, 0.0f, 1.0f);
	}

	float FiniteOr(float p_value, float p_fallback)
	{
		return std::isfinite(p_value) ? p_value : p_fallback;
	}

	void OrderPair(float& p_min, float& p_max)
	{
		if (p_min > p_max)
		{
			std::swap(p_min, p_max);
		}
	}
}

OvCore::Particles::ParticleEmitter::ParticleEmitter(const ParticleSettings& p_settings)
{
	SetSettings(p_settings);
	ResetRing();
	Reseed();
}

void OvCore::Particles::ParticleEmitter::SetSettings(const ParticleSettings& p_settings)
{
	ParticleSettings settings = p_settings;

	settings.maxParticles = std::clamp<uint32_t>(settings.maxParticles, 1, ParticleSettings::kMaxParticles);
	settings.rate = std::max(FiniteOr(settings.rate, 0.0f), 0.0f);
	settings.duration = std::max(FiniteOr(settings.duration, 1.0f), 0.01f);

	for (auto& burst : settings.bursts)
	{
		burst.time = std::max(FiniteOr(burst.time, 0.0f), 0.0f);
		burst.count = std::min(burst.count, settings.maxParticles);
	}

	settings.lifetimeMin = std::max(FiniteOr(settings.lifetimeMin, 1.0f), 0.001f);
	settings.lifetimeMax = std::max(FiniteOr(settings.lifetimeMax, 1.0f), 0.001f);
	OrderPair(settings.lifetimeMin, settings.lifetimeMax);

	settings.speedMin = FiniteOr(settings.speedMin, 0.0f);
	settings.speedMax = FiniteOr(settings.speedMax, 0.0f);
	OrderPair(settings.speedMin, settings.speedMax);

	settings.shapeRadius = std::max(FiniteOr(settings.shapeRadius, 0.0f), 0.0f);
	settings.shapeAngle = std::clamp(FiniteOr(settings.shapeAngle, 0.0f), 0.0f, 180.0f);
	settings.shapeSize = {
		std::max(FiniteOr(settings.shapeSize.x, 0.0f), 0.0f),
		std::max(FiniteOr(settings.shapeSize.y, 0.0f), 0.0f),
		std::max(FiniteOr(settings.shapeSize.z, 0.0f), 0.0f)
	};

	settings.drag = std::max(FiniteOr(settings.drag, 0.0f), 0.0f);
	settings.sizeStart = std::max(FiniteOr(settings.sizeStart, 0.0f), 0.0f);
	settings.sizeEnd = std::max(FiniteOr(settings.sizeEnd, 0.0f), 0.0f);
	settings.fadeIn = Clamp01(FiniteOr(settings.fadeIn, 0.0f));
	settings.fadeOut = Clamp01(FiniteOr(settings.fadeOut, 0.0f));

	settings.spinMin = FiniteOr(settings.spinMin, 0.0f);
	settings.spinMax = FiniteOr(settings.spinMax, 0.0f);
	OrderPair(settings.spinMin, settings.spinMax);

	const bool resize = settings.maxParticles != m_settings.maxParticles || m_records.empty();
	const bool reseed = settings.seed != m_settings.seed;

	m_settings = std::move(settings);
	m_statsDirty = true;

	if (resize)
	{
		ResetRing();
	}

	if (reseed)
	{
		Reseed();
	}
}

const OvCore::Particles::ParticleSettings& OvCore::Particles::ParticleEmitter::GetSettings() const
{
	return m_settings;
}

void OvCore::Particles::ParticleEmitter::Play()
{
	m_playing = true;
	m_cycleTime = 0.0;
	m_rateCarry = 0.0;
	m_previousOrigin.reset();
}

void OvCore::Particles::ParticleEmitter::Stop()
{
	m_playing = false;
}

void OvCore::Particles::ParticleEmitter::Clear()
{
	ResetRing();
	Reseed();
}

bool OvCore::Particles::ParticleEmitter::IsPlaying() const
{
	return m_playing;
}

void OvCore::Particles::ParticleEmitter::Advance(float p_deltaTime, const OvMaths::FMatrix4& p_emitterWorld)
{
	const float deltaTime = std::max(FiniteOr(p_deltaTime, 0.0f), 0.0f);
	const double frameStart = m_time;

	m_currentOrigin = Origin(p_emitterWorld);
	if (!m_previousOrigin)
	{
		m_previousOrigin = m_currentOrigin;
	}

	if (m_playing && deltaTime > 0.0f)
	{
		// The step is cut at each end of cycle, where bursts are armed again (or emission ends)
		double remaining = deltaTime;
		double stepStart = frameStart;
		uint32_t cycles = 0;

		while (remaining > 0.0 && m_playing)
		{
			const double step = std::min(remaining, std::max(static_cast<double>(m_settings.duration) - m_cycleTime, 0.0));

			EmitOverStep(stepStart, step, frameStart, deltaTime, p_emitterWorld);

			m_cycleTime += step;
			stepStart += step;
			remaining -= step;

			if (m_cycleTime >= m_settings.duration)
			{
				if (m_settings.looping && ++cycles < kMaxCyclesPerStep)
				{
					m_cycleTime = 0.0;
				}
				else
				{
					m_playing = m_settings.looping;
					m_cycleTime = m_settings.looping ? 0.0 : m_cycleTime;
					break;
				}
			}
		}
	}

	m_time = frameStart + deltaTime;
	m_previousOrigin = m_currentOrigin;
	m_statsDirty = true;

	if (m_time > kRebaseAfter)
	{
		Rebase();
	}
}

uint32_t OvCore::Particles::ParticleEmitter::Emit(uint32_t p_count, const OvMaths::FMatrix4& p_emitterWorld, const ParticleOverrides& p_overrides)
{
	const uint32_t count = std::min(p_count, m_settings.maxParticles);

	m_currentOrigin = Origin(p_emitterWorld);

	for (uint32_t i = 0; i < count; ++i)
	{
		Spawn(static_cast<float>(m_time), p_emitterWorld, p_overrides);
	}

	m_statsDirty = true;
	return count;
}

uint32_t OvCore::Particles::ParticleEmitter::GetAliveCount() const
{
	return GetStats().alive;
}

float OvCore::Particles::ParticleEmitter::GetTime() const
{
	return static_cast<float>(m_time);
}

std::span<const OvCore::Particles::ParticleRecord> OvCore::Particles::ParticleEmitter::GetRecords() const
{
	return m_records;
}

OvCore::Particles::ParticleState OvCore::Particles::ParticleEmitter::Evaluate(const ParticleRecord& p_record) const
{
	return Evaluate(p_record, m_settings, static_cast<float>(m_time));
}

OvCore::Particles::ParticleState OvCore::Particles::ParticleEmitter::Evaluate(const ParticleRecord& p_record, const ParticleSettings& p_settings, float p_time)
{
	// Common/Particles.ovfxh works these out the same way: keep them in step
	ParticleState state;

	const float age = p_time - p_record.birth;
	state.alive = p_record.lifetime > 0.0f && age >= 0.0f && age < p_record.lifetime;
	state.life = p_record.lifetime > 0.0f ? Clamp01(age / p_record.lifetime) : 1.0f;

	const FVector3& gravity = p_settings.gravity;
	const float drag = p_settings.drag;

	if (drag > kMinimumDrag)
	{
		// dv/dt = g - k v: the velocity eases towards the terminal one, g / k
		const FVector3 terminal{ gravity.x / drag, gravity.y / drag, gravity.z / drag };
		const float decay = std::exp(-drag * age);
		const float reach = (1.0f - decay) / drag;

		state.position = {
			p_record.position.x + (p_record.velocity.x - terminal.x) * reach + terminal.x * age,
			p_record.position.y + (p_record.velocity.y - terminal.y) * reach + terminal.y * age,
			p_record.position.z + (p_record.velocity.z - terminal.z) * reach + terminal.z * age
		};
		state.velocity = {
			(p_record.velocity.x - terminal.x) * decay + terminal.x,
			(p_record.velocity.y - terminal.y) * decay + terminal.y,
			(p_record.velocity.z - terminal.z) * decay + terminal.z
		};
	}
	else
	{
		const float half = 0.5f * age * age;
		state.position = {
			p_record.position.x + p_record.velocity.x * age + gravity.x * half,
			p_record.position.y + p_record.velocity.y * age + gravity.y * half,
			p_record.position.z + p_record.velocity.z * age + gravity.z * half
		};
		state.velocity = {
			p_record.velocity.x + gravity.x * age,
			p_record.velocity.y + gravity.y * age,
			p_record.velocity.z + gravity.z * age
		};
	}

	const float life = state.life;
	state.size = (p_settings.sizeStart + (p_settings.sizeEnd - p_settings.sizeStart) * life) * p_record.size;

	const FVector4& from = p_settings.colorStart;
	const FVector4& to = p_settings.colorEnd;
	state.color = {
		(from.x + (to.x - from.x) * life) * p_record.color.x,
		(from.y + (to.y - from.y) * life) * p_record.color.y,
		(from.z + (to.z - from.z) * life) * p_record.color.z,
		(from.w + (to.w - from.w) * life) * p_record.color.w
	};

	if (p_settings.fadeIn > 0.0f)
	{
		state.color.w *= Clamp01(life / p_settings.fadeIn);
	}

	if (p_settings.fadeOut > 0.0f)
	{
		state.color.w *= Clamp01((1.0f - life) / p_settings.fadeOut);
	}

	state.rotation = p_record.rotation + p_record.spin * age;

	return state;
}

std::optional<OvRendering::Geometry::BoundingSphere> OvCore::Particles::ParticleEmitter::GetBounds() const
{
	return GetStats().bounds;
}

OvCore::Particles::ParticleSlotRange OvCore::Particles::ParticleEmitter::GetDrawRange() const
{
	return GetStats().drawRange;
}

OvCore::Particles::ParticleBufferHeader OvCore::Particles::ParticleEmitter::BuildHeader(const OvMaths::FMatrix4& p_emitterWorld) const
{
	ParticleBufferHeader header{};

	header.localToWorld = FMatrix4::Transpose(m_settings.space == EParticleSpace::LOCAL ? p_emitterWorld : FMatrix4::Identity);
	header.gravity[0] = m_settings.gravity.x;
	header.gravity[1] = m_settings.gravity.y;
	header.gravity[2] = m_settings.gravity.z;
	header.drag = m_settings.drag;

	const FVector4& from = m_settings.colorStart;
	const FVector4& to = m_settings.colorEnd;
	header.colorStart[0] = from.x; header.colorStart[1] = from.y; header.colorStart[2] = from.z; header.colorStart[3] = from.w;
	header.colorEnd[0] = to.x; header.colorEnd[1] = to.y; header.colorEnd[2] = to.z; header.colorEnd[3] = to.w;

	header.sizeStart = m_settings.sizeStart;
	header.sizeEnd = m_settings.sizeEnd;
	header.fadeIn = m_settings.fadeIn;
	header.fadeOut = m_settings.fadeOut;
	header.time = static_cast<float>(m_time);

	const auto range = GetDrawRange();
	header.capacity = static_cast<uint32_t>(m_records.size());
	header.first = range.first;
	header.count = range.count;
	header.space = static_cast<uint32_t>(m_settings.space);

	return header;
}

std::vector<OvCore::Particles::ParticleSlotRange> OvCore::Particles::ParticleEmitter::TakeChangedSlots()
{
	std::vector<ParticleSlotRange> result;
	const auto capacity = static_cast<uint32_t>(m_records.size());

	if (m_allChanged)
	{
		result.push_back({ 0, capacity });
	}
	else if (m_changed.count > 0)
	{
		// The slots written since the last call follow each other around the ring: at most two ranges
		const uint32_t tail = std::min(m_changed.count, capacity - m_changed.first);
		result.push_back({ m_changed.first, tail });

		if (m_changed.count > tail)
		{
			result.push_back({ 0, m_changed.count - tail });
		}
	}

	m_allChanged = false;
	m_changed = {};
	return result;
}

void OvCore::Particles::ParticleEmitter::ResetRing()
{
	m_records.assign(m_settings.maxParticles, ParticleRecord{});
	m_birthOrder.assign(m_settings.maxParticles, 0);
	m_next = 0;
	m_allChanged = true;
	m_changed = {};
	m_statsDirty = true;
}

void OvCore::Particles::ParticleEmitter::Reseed()
{
	uint64_t seed = m_settings.seed != 0 ? m_settings.seed : g_seedCounter.fetch_add(0x9E3779B97F4A7C15ull);

	// splitmix64: spreads a small seed over the whole state
	seed += 0x9E3779B97F4A7C15ull;
	seed = (seed ^ (seed >> 30)) * 0xBF58476D1CE4E5B9ull;
	seed = (seed ^ (seed >> 27)) * 0x94D049BB133111EBull;
	seed ^= seed >> 31;

	m_random = seed != 0 ? seed : 1;
}

float OvCore::Particles::ParticleEmitter::Random()
{
	// xorshift64*: a float in [0, 1) from its 24 high bits
	m_random ^= m_random >> 12;
	m_random ^= m_random << 25;
	m_random ^= m_random >> 27;
	const uint64_t value = m_random * 0x2545F4914F6CDD1Dull;
	return static_cast<float>(value >> 40) / static_cast<float>(1u << 24);
}

float OvCore::Particles::ParticleEmitter::Random(float p_min, float p_max)
{
	return p_min + (p_max - p_min) * Random();
}

OvMaths::FVector3 OvCore::Particles::ParticleEmitter::RandomDirection()
{
	const float y = Random(-1.0f, 1.0f);
	const float angle = Random() * 2.0f * kPi;
	const float ring = std::sqrt(std::max(1.0f - y * y, 0.0f));
	return { ring * std::cos(angle), y, ring * std::sin(angle) };
}

void OvCore::Particles::ParticleEmitter::SampleShape(OvMaths::FVector3& p_position, OvMaths::FVector3& p_direction)
{
	switch (m_settings.shape)
	{
	case EParticleShape::POINT:
		p_position = FVector3::Zero;
		p_direction = RandomDirection();
		break;

	case EParticleShape::SPHERE:
	case EParticleShape::HEMISPHERE:
	{
		p_direction = RandomDirection();
		if (m_settings.shape == EParticleShape::HEMISPHERE)
		{
			p_direction.y = std::abs(p_direction.y);
		}

		// Uniform in the volume: the distance from the centre goes as the cube root
		const float distance = m_settings.shapeRadius * std::cbrt(Random());
		p_position = { p_direction.x * distance, p_direction.y * distance, p_direction.z * distance };
		break;
	}

	case EParticleShape::CONE:
	{
		// A direction uniform over the cap of the cone's angle, around +Y
		const float cosAngle = std::cos(m_settings.shapeAngle * kDegreesToRadians);
		const float y = 1.0f - Random() * (1.0f - cosAngle);
		const float around = Random() * 2.0f * kPi;
		const float ring = std::sqrt(std::max(1.0f - y * y, 0.0f));
		p_direction = { ring * std::cos(around), y, ring * std::sin(around) };

		// A point uniform over the base's disc
		const float distance = m_settings.shapeRadius * std::sqrt(Random());
		const float onDisc = Random() * 2.0f * kPi;
		p_position = { distance * std::cos(onDisc), 0.0f, distance * std::sin(onDisc) };
		break;
	}

	case EParticleShape::BOX:
		p_position = {
			(Random() - 0.5f) * m_settings.shapeSize.x,
			(Random() - 0.5f) * m_settings.shapeSize.y,
			(Random() - 0.5f) * m_settings.shapeSize.z
		};
		p_direction = FVector3::Up;
		break;
	}
}

void OvCore::Particles::ParticleEmitter::Spawn(float p_birth, const OvMaths::FMatrix4& p_emitterWorld, const ParticleOverrides& p_overrides)
{
	const bool local = m_settings.space == EParticleSpace::LOCAL;

	FVector3 position;
	FVector3 direction;
	SampleShape(position, direction);

	// The shape is in the emitter's space: a world system takes it into the world
	if (!local)
	{
		position = TransformPoint(p_emitterWorld, position);
		direction = NormalizeOr(TransformDirection(p_emitterWorld, direction), direction);
	}

	if (p_overrides.position || p_overrides.direction)
	{
		// Overrides are in the world: a local system takes them into the emitter's space
		const FMatrix4 worldToLocal = local ? FMatrix4::Inverse(p_emitterWorld) : FMatrix4::Identity;

		if (p_overrides.position)
		{
			position = local ? TransformPoint(worldToLocal, *p_overrides.position) : *p_overrides.position;
		}

		if (p_overrides.direction)
		{
			const FVector3 overridden = local ? TransformDirection(worldToLocal, *p_overrides.direction) : *p_overrides.direction;
			direction = NormalizeOr(overridden, direction);
		}
	}

	const float speed = p_overrides.speed ? *p_overrides.speed : Random(m_settings.speedMin, m_settings.speedMax);
	const float lifetime = p_overrides.lifetime ? std::max(*p_overrides.lifetime, 0.001f) : Random(m_settings.lifetimeMin, m_settings.lifetimeMax);

	ParticleRecord& record = m_records[m_next];
	record.position = position;
	record.birth = p_birth;
	record.velocity = { direction.x * speed, direction.y * speed, direction.z * speed };
	record.lifetime = lifetime;
	record.color = p_overrides.color ? *p_overrides.color : FVector4{ 1.0f, 1.0f, 1.0f, 1.0f };
	record.size = p_overrides.size ? std::max(*p_overrides.size, 0.0f) : 1.0f;
	record.rotation = m_settings.randomRotation ? Random() * 2.0f * kPi : 0.0f;
	record.spin = Random(m_settings.spinMin, m_settings.spinMax) * kDegreesToRadians;
	record.seed = Random();

	m_birthOrder[m_next] = ++m_emitted;

	const auto capacity = static_cast<uint32_t>(m_records.size());
	if (m_changed.count == 0)
	{
		m_changed.first = m_next;
	}
	m_changed.count = std::min(m_changed.count + 1, capacity);

	m_next = (m_next + 1) % capacity;
}

void OvCore::Particles::ParticleEmitter::EmitOverStep(double p_stepStart, double p_step, double p_frameStart, float p_frameLength, const OvMaths::FMatrix4& p_emitterWorld)
{
	// Where the emitter was at a time of the frame: between its previous origin and its current one
	const auto emitterAt = [&](double p_time) {
		FMatrix4 matrix = p_emitterWorld;

		if (p_frameLength > 0.0f && m_previousOrigin)
		{
			const float alpha = static_cast<float>(std::clamp((p_time - p_frameStart) / p_frameLength, 0.0, 1.0));
			const FVector3 origin = FVector3::Lerp(*m_previousOrigin, m_currentOrigin, alpha);
			matrix.data[3] = origin.x;
			matrix.data[7] = origin.y;
			matrix.data[11] = origin.z;
		}

		return matrix;
	};

	// The rate gives a particle every 1 / rate seconds, each born at its own time within the step
	if (m_settings.rate > 0.0f && p_step > 0.0)
	{
		const double before = m_rateCarry;
		const double total = before + m_settings.rate * p_step;
		const auto births = static_cast<uint64_t>(std::floor(total));

		for (uint64_t i = 1; i <= births; ++i)
		{
			const double birth = p_stepStart + (static_cast<double>(i) - before) / m_settings.rate;
			Spawn(static_cast<float>(birth), emitterAt(birth), {});
		}

		m_rateCarry = total - static_cast<double>(births);
	}

	// A burst is due once in each cycle, when the cycle's time reaches it
	for (const auto& burst : m_settings.bursts)
	{
		if (burst.time >= m_cycleTime && burst.time < m_cycleTime + p_step)
		{
			const double birth = p_stepStart + (burst.time - m_cycleTime);
			const auto matrix = emitterAt(birth);

			for (uint32_t i = 0; i < burst.count; ++i)
			{
				Spawn(static_cast<float>(birth), matrix, {});
			}
		}
	}
}

void OvCore::Particles::ParticleEmitter::Rebase()
{
	// Births and the clock move back together: what is drawn does not change, but the floats stay precise
	const float shift = static_cast<float>(m_time);

	for (auto& record : m_records)
	{
		record.birth -= shift;
	}

	m_time -= shift;
	m_allChanged = true;
	m_statsDirty = true;
}

const OvCore::Particles::ParticleEmitter::Stats& OvCore::Particles::ParticleEmitter::GetStats() const
{
	if (!m_statsDirty)
	{
		return m_stats;
	}

	Stats stats;
	const auto capacity = static_cast<uint32_t>(m_records.size());
	const float time = static_cast<float>(m_time);

	uint64_t oldest = std::numeric_limits<uint64_t>::max();
	FVector3 lower{ std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
	FVector3 upper{ std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest() };
	float largest = 0.0f;

	// Only where the particles are and how large: what Evaluate gives, without the colour, over every slot
	const FVector3& gravity = m_settings.gravity;
	const float drag = m_settings.drag;
	const bool dragged = drag > kMinimumDrag;
	const FVector3 terminal = dragged ? FVector3{ gravity.x / drag, gravity.y / drag, gravity.z / drag } : FVector3::Zero;
	const float largestScale = std::max(m_settings.sizeStart, m_settings.sizeEnd);

	for (uint32_t slot = 0; slot < capacity; ++slot)
	{
		const ParticleRecord& record = m_records[slot];
		const float age = time - record.birth;

		if (!(record.lifetime > 0.0f && age >= 0.0f && age < record.lifetime))
		{
			continue;
		}

		FVector3 position;
		if (dragged)
		{
			const float reach = (1.0f - std::exp(-drag * age)) / drag;
			position = {
				record.position.x + (record.velocity.x - terminal.x) * reach + terminal.x * age,
				record.position.y + (record.velocity.y - terminal.y) * reach + terminal.y * age,
				record.position.z + (record.velocity.z - terminal.z) * reach + terminal.z * age
			};
		}
		else
		{
			const float half = 0.5f * age * age;
			position = {
				record.position.x + record.velocity.x * age + gravity.x * half,
				record.position.y + record.velocity.y * age + gravity.y * half,
				record.position.z + record.velocity.z * age + gravity.z * half
			};
		}

		++stats.alive;
		oldest = std::min(oldest, m_birthOrder[slot]);
		lower = { std::min(lower.x, position.x), std::min(lower.y, position.y), std::min(lower.z, position.z) };
		upper = { std::max(upper.x, position.x), std::max(upper.y, position.y), std::max(upper.z, position.z) };
		largest = std::max(largest, largestScale * record.size);
	}

	if (stats.alive > 0)
	{
		const auto count = static_cast<uint32_t>(std::min<uint64_t>(m_emitted - oldest + 1, capacity));
		stats.drawRange = { (m_next + capacity - count) % capacity, count };

		const FVector3 center{ (lower.x + upper.x) * 0.5f, (lower.y + upper.y) * 0.5f, (lower.z + upper.z) * 0.5f };
		const FVector3 half{ (upper.x - lower.x) * 0.5f, (upper.y - lower.y) * 0.5f, (upper.z - lower.z) * 0.5f };

		// A particle is drawn as wide as its size, possibly turned: half its diagonal reaches past its centre
		stats.bounds = OvRendering::Geometry::BoundingSphere{
			center,
			std::sqrt(half.x * half.x + half.y * half.y + half.z * half.z) + largest * 0.75f
		};
	}

	m_stats = stats;
	m_statsDirty = false;
	return m_stats;
}
