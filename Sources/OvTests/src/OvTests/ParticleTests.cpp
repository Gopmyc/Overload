/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <doctest/doctest.h>
#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CParticleSystem.h>
#include <OvCore/Particles/ParticleEmitter.h>
#include <OvCore/SceneSystem/Scene.h>

using OvCore::Particles::EParticleShape;
using OvCore::Particles::EParticleSpace;
using OvCore::Particles::ParticleEmitter;
using OvCore::Particles::ParticleOverrides;
using OvCore::Particles::ParticleRecord;
using OvCore::Particles::ParticleSettings;
using OvCore::Particles::ParticleState;
using OvMaths::FMatrix4;
using OvMaths::FVector3;
using OvMaths::FVector4;

namespace
{
	constexpr float kPi = 3.14159265358979f;

	/**
	* Settings that emit nothing by themselves, and whose particles live long
	*/
	ParticleSettings Quiet()
	{
		ParticleSettings settings;
		settings.rate = 0.0f;
		settings.lifetimeMin = settings.lifetimeMax = 100.0f;
		settings.seed = 7;
		return settings;
	}

	float Length(const FVector3& p_vector)
	{
		return std::sqrt(p_vector.x * p_vector.x + p_vector.y * p_vector.y + p_vector.z * p_vector.z);
	}

	std::vector<const ParticleRecord*> Born(const ParticleEmitter& p_emitter)
	{
		std::vector<const ParticleRecord*> result;
		for (const auto& record : p_emitter.GetRecords())
		{
			if (record.lifetime > 0.0f)
			{
				result.push_back(&record);
			}
		}
		return result;
	}
}

TEST_SUITE("Particles")
{
	TEST_CASE("the rate gives its particles over many frames without drift, each born at its own time")
	{
		auto settings = Quiet();
		settings.rate = 50.0f;
		settings.maxParticles = 1000;
		ParticleEmitter emitter(settings);
		emitter.Play();

		// A little over 2 s, in uneven frames
		const float frames[] = { 0.016f, 0.033f, 0.007f, 0.1f, 0.044f };
		double elapsed = 0.0;
		for (int i = 0; elapsed < 2.009; ++i)
		{
			const float step = static_cast<float>(std::min<double>(frames[i % 5], 2.01 - elapsed));
			emitter.Advance(step, FMatrix4::Identity);
			elapsed += step;
		}

		CHECK(emitter.GetAliveCount() == 100);

		// Born every 1 / rate seconds, whatever the frames
		auto born = Born(emitter);
		std::sort(born.begin(), born.end(), [](auto* a, auto* b) { return a->birth < b->birth; });
		REQUIRE(born.size() == 100);
		for (size_t i = 0; i < born.size(); ++i)
		{
			CHECK(born[i]->birth == doctest::Approx(0.02 * (i + 1)).epsilon(1e-4));
		}

		// Stopped, it emits no more, and what lives lives on
		emitter.Stop();
		emitter.Advance(1.0f, FMatrix4::Identity);
		CHECK(emitter.GetAliveCount() == 100);
		CHECK_FALSE(emitter.IsPlaying());
	}

	TEST_CASE("bursts come once a cycle, and a system that does not loop stops after one")
	{
		auto settings = Quiet();
		settings.maxParticles = 1000;
		settings.duration = 1.0f;
		settings.bursts = { { 0.0f, 10 }, { 0.5f, 5 } };
		ParticleEmitter looping(settings);
		looping.Play();

		for (int i = 0; i < 10; ++i)
		{
			looping.Advance(0.25f, FMatrix4::Identity);
		}

		// 10 at 0, 1 and 2 s, 5 at 0.5 and 1.5 s: the one at 2.5 s is not due yet
		CHECK(looping.GetAliveCount() == 40);
		CHECK(looping.IsPlaying());

		settings.looping = false;
		ParticleEmitter once(settings);
		once.Play();

		for (int i = 0; i < 10; ++i)
		{
			once.Advance(0.25f, FMatrix4::Identity);
		}

		CHECK(once.GetAliveCount() == 15);
		CHECK_FALSE(once.IsPlaying());

		// A frame longer than a cycle fires each cycle's bursts
		settings.looping = true;
		ParticleEmitter late(settings);
		late.Play();
		late.Advance(3.0f, FMatrix4::Identity);
		CHECK(late.GetAliveCount() == 45);
	}

	TEST_CASE("a full ring gives the oldest particle's slot to a new one")
	{
		auto settings = Quiet();
		settings.maxParticles = 8;
		ParticleEmitter emitter(settings);

		CHECK(emitter.Emit(8, FMatrix4::Identity) == 8);
		emitter.Advance(1.0f, FMatrix4::Identity);
		CHECK(emitter.Emit(3, FMatrix4::Identity) == 3);

		CHECK(emitter.GetAliveCount() == 8);

		// The three first slots hold the newest, born a second later
		const auto records = emitter.GetRecords();
		for (size_t slot = 0; slot < 8; ++slot)
		{
			CAPTURE(slot);
			CHECK(records[slot].birth == doctest::Approx(slot < 3 ? 1.0 : 0.0));
		}

		// Drawn from the oldest to the newest: the whole ring, from the slot after the newest
		CHECK(emitter.GetDrawRange().first == 3);
		CHECK(emitter.GetDrawRange().count == 8);

		// No more are born than the ring holds
		CHECK(emitter.Emit(20, FMatrix4::Identity) == 8);
	}

	TEST_CASE("the same seed gives the same particles, and seed 0 gives each system its own")
	{
		auto settings = Quiet();
		settings.shape = EParticleShape::SPHERE;
		settings.shapeRadius = 2.0f;
		settings.randomRotation = true;
		settings.seed = 42;

		ParticleEmitter first(settings);
		ParticleEmitter second(settings);
		first.Emit(64, FMatrix4::Identity);
		second.Emit(64, FMatrix4::Identity);
		CHECK(std::memcmp(first.GetRecords().data(), second.GetRecords().data(), 64 * sizeof(ParticleRecord)) == 0);

		settings.seed = 43;
		ParticleEmitter other(settings);
		other.Emit(64, FMatrix4::Identity);
		CHECK(std::memcmp(first.GetRecords().data(), other.GetRecords().data(), 64 * sizeof(ParticleRecord)) != 0);

		settings.seed = 0;
		ParticleEmitter anyFirst(settings);
		ParticleEmitter anySecond(settings);
		anyFirst.Emit(64, FMatrix4::Identity);
		anySecond.Emit(64, FMatrix4::Identity);
		CHECK(std::memcmp(anyFirst.GetRecords().data(), anySecond.GetRecords().data(), 64 * sizeof(ParticleRecord)) != 0);

		// Clear draws the seed again: the same particles as at the start
		first.Clear();
		first.Emit(64, FMatrix4::Identity);
		CHECK(std::memcmp(first.GetRecords().data(), second.GetRecords().data(), 64 * sizeof(ParticleRecord)) == 0);
	}

	TEST_CASE("every particle is born within its shape, and leaves its way")
	{
		auto settings = Quiet();
		settings.maxParticles = 500;
		settings.speedMin = settings.speedMax = 1.0f;
		settings.shapeRadius = 2.0f;
		settings.shapeAngle = 30.0f;
		settings.shapeSize = { 4.0f, 2.0f, 6.0f };

		for (const auto shape : { EParticleShape::POINT, EParticleShape::SPHERE, EParticleShape::HEMISPHERE, EParticleShape::CONE, EParticleShape::BOX })
		{
			CAPTURE(static_cast<int>(shape));
			settings.shape = shape;
			ParticleEmitter emitter(settings);
			emitter.Emit(500, FMatrix4::Identity);

			bool inside = true;
			bool way = true;
			float farthest = 0.0f;

			for (const auto& record : emitter.GetRecords())
			{
				const auto& p = record.position;
				const auto& v = record.velocity;
				way = way && std::abs(Length(v) - 1.0f) < 1e-4f;
				farthest = std::max(farthest, Length(p));

				switch (shape)
				{
				case EParticleShape::POINT:
					inside = inside && Length(p) == 0.0f;
					break;
				case EParticleShape::SPHERE:
					inside = inside && Length(p) <= 2.0f + 1e-4f;
					break;
				case EParticleShape::HEMISPHERE:
					inside = inside && Length(p) <= 2.0f + 1e-4f && p.y >= 0.0f && v.y >= 0.0f;
					break;
				case EParticleShape::CONE:
					inside = inside && p.y == 0.0f && std::sqrt(p.x * p.x + p.z * p.z) <= 2.0f + 1e-4f;
					way = way && v.y >= std::cos(30.0f * kPi / 180.0f) - 1e-4f;
					break;
				case EParticleShape::BOX:
					inside = inside && std::abs(p.x) <= 2.0f && std::abs(p.y) <= 1.0f && std::abs(p.z) <= 3.0f;
					way = way && v.y == doctest::Approx(1.0f);
					break;
				}
			}

			CHECK(inside);
			CHECK(way);

			// The shapes with a size fill it, not only its centre
			if (shape != EParticleShape::POINT)
			{
				CHECK(farthest > 1.5f);
			}
		}
	}

	TEST_CASE("a particle moves as gravity and drag move it: the formula meets a fine integration")
	{
		for (const float drag : { 0.0f, 0.7f, 3.0f })
		{
			CAPTURE(drag);
			ParticleSettings settings = Quiet();
			settings.gravity = { 0.5f, -9.81f, 0.0f };
			settings.drag = drag;

			ParticleRecord record;
			record.position = { 1.0f, 2.0f, 3.0f };
			record.velocity = { 3.0f, 5.0f, -1.0f };
			record.lifetime = 10.0f;

			// dv/dt = g - k v, integrated in doubles by the midpoint method, in steps of 10 µs
			double position[3] = { record.position.x, record.position.y, record.position.z };
			double velocity[3] = { record.velocity.x, record.velocity.y, record.velocity.z };
			const double gravity[3] = { settings.gravity.x, settings.gravity.y, settings.gravity.z };
			const double step = 1e-5;
			for (int i = 0; i < 150000; ++i)
			{
				for (int axis = 0; axis < 3; ++axis)
				{
					const double half = velocity[axis] + (gravity[axis] - drag * velocity[axis]) * step * 0.5;
					position[axis] += half * step;
					velocity[axis] += (gravity[axis] - drag * half) * step;
				}
			}

			const ParticleState state = ParticleEmitter::Evaluate(record, settings, 1.5f);
			CHECK(state.alive);
			CHECK(state.position.x == doctest::Approx(position[0]).epsilon(1e-4));
			CHECK(state.position.y == doctest::Approx(position[1]).epsilon(1e-4));
			CHECK(state.position.z == doctest::Approx(position[2]).epsilon(1e-4));
			CHECK(state.velocity.x == doctest::Approx(velocity[0]).epsilon(1e-4));
			CHECK(state.velocity.y == doctest::Approx(velocity[1]).epsilon(1e-4));
		}
	}

	TEST_CASE("a particle's size, colour, fades and rotation follow its life, and it dies at its end")
	{
		ParticleSettings settings = Quiet();
		settings.sizeStart = 2.0f;
		settings.sizeEnd = 0.0f;
		settings.colorStart = { 1.0f, 0.0f, 0.0f, 1.0f };
		settings.colorEnd = { 0.0f, 0.0f, 1.0f, 0.5f };
		settings.fadeIn = 0.25f;
		settings.fadeOut = 0.25f;

		ParticleRecord record;
		record.birth = 10.0f;
		record.lifetime = 4.0f;
		record.color = { 1.0f, 1.0f, 1.0f, 0.5f };
		record.size = 3.0f;
		record.rotation = 1.0f;
		record.spin = 0.5f;

		CHECK_FALSE(ParticleEmitter::Evaluate(record, settings, 9.99f).alive);

		const auto born = ParticleEmitter::Evaluate(record, settings, 10.0f);
		CHECK(born.alive);
		CHECK(born.color.w == doctest::Approx(0.0f));

		const auto fadingIn = ParticleEmitter::Evaluate(record, settings, 10.5f);
		CHECK(fadingIn.life == doctest::Approx(0.125f));
		CHECK(fadingIn.color.w == doctest::Approx((1.0f - 0.125f * 0.5f) * 0.5f * 0.5f));

		const auto middle = ParticleEmitter::Evaluate(record, settings, 12.0f);
		CHECK(middle.size == doctest::Approx(1.0f * 3.0f));
		CHECK(middle.color.x == doctest::Approx(0.5f));
		CHECK(middle.color.z == doctest::Approx(0.5f));
		CHECK(middle.color.w == doctest::Approx(0.75f * 0.5f));
		CHECK(middle.rotation == doctest::Approx(1.0f + 0.5f * 2.0f));

		const auto fadingOut = ParticleEmitter::Evaluate(record, settings, 13.5f);
		CHECK(fadingOut.color.w == doctest::Approx((1.0f - 0.875f * 0.5f) * 0.5f * 0.5f));

		CHECK(ParticleEmitter::Evaluate(record, settings, 13.99f).alive);
		CHECK_FALSE(ParticleEmitter::Evaluate(record, settings, 14.0f).alive);

		// A slot never used is never alive
		CHECK_FALSE(ParticleEmitter::Evaluate(ParticleRecord{}, settings, 0.0f).alive);
	}

	TEST_CASE("the bounds hold every particle alive, and there are none without one")
	{
		auto settings = Quiet();
		settings.maxParticles = 300;
		settings.rate = 100.0f;
		settings.lifetimeMin = 0.5f;
		settings.lifetimeMax = 2.0f;
		settings.shape = EParticleShape::SPHERE;
		settings.shapeRadius = 1.0f;
		settings.speedMin = 1.0f;
		settings.speedMax = 6.0f;
		settings.gravity = { 0.0f, -9.81f, 0.0f };
		settings.drag = 0.4f;
		settings.sizeStart = 0.5f;

		ParticleEmitter emitter(settings);
		CHECK_FALSE(emitter.GetBounds().has_value());
		CHECK(emitter.GetDrawRange().count == 0);

		emitter.Play();
		for (int frame = 0; frame < 120; ++frame)
		{
			emitter.Advance(1.0f / 60.0f, FMatrix4::Translation({ 0.05f * frame, 0.0f, 0.0f }));

			const auto bounds = emitter.GetBounds();
			REQUIRE(bounds.has_value());

			bool held = true;
			for (const auto& record : emitter.GetRecords())
			{
				const auto state = emitter.Evaluate(record);
				if (state.alive)
				{
					const FVector3 offset{ state.position.x - bounds->position.x, state.position.y - bounds->position.y, state.position.z - bounds->position.z };
					held = held && Length(offset) + state.size * 0.5f <= bounds->radius + 1e-3f;
				}
			}
			CHECK(held);
		}

		emitter.Clear();
		CHECK(emitter.GetAliveCount() == 0);
		CHECK_FALSE(emitter.GetBounds().has_value());
	}

	TEST_CASE("a world system is born along the emitter's way through the frame; a local one in its own space")
	{
		auto settings = Quiet();
		settings.maxParticles = 1000;
		settings.rate = 100.0f;
		settings.shape = EParticleShape::POINT;

		ParticleEmitter world(settings);
		world.Play();
		world.Advance(0.0f, FMatrix4::Translation({ 0.0f, 0.0f, 0.0f }));
		world.Advance(1.0f, FMatrix4::Translation({ 10.0f, 0.0f, 0.0f }));

		float lowest = 100.0f;
		float highest = -100.0f;
		for (const auto* record : Born(world))
		{
			lowest = std::min(lowest, record->position.x);
			highest = std::max(highest, record->position.x);
		}
		CHECK(Born(world).size() == 100);
		CHECK(lowest < 0.2f);
		CHECK(highest == doctest::Approx(10.0f));

		settings.space = EParticleSpace::LOCAL;
		ParticleEmitter local(settings);
		local.Play();
		local.Advance(1.0f, FMatrix4::Translation({ 10.0f, 0.0f, 0.0f }));

		bool atOrigin = true;
		for (const auto* record : Born(local))
		{
			atOrigin = atOrigin && Length(record->position) == 0.0f;
		}
		CHECK(atOrigin);

		// Its header carries the emitter's matrix, by which the shader takes the particles into the world
		const auto header = local.BuildHeader(FMatrix4::Translation({ 10.0f, 0.0f, 0.0f }));
		CHECK(header.localToWorld.data[12] == doctest::Approx(10.0f));
		CHECK(world.BuildHeader(FMatrix4::Translation({ 10.0f, 0.0f, 0.0f })).localToWorld.data[12] == doctest::Approx(0.0f));
	}

	TEST_CASE("a script's burst overrides where its particles are born, their way, colour, speed, size and life")
	{
		auto settings = Quiet();
		settings.space = EParticleSpace::LOCAL;
		ParticleEmitter emitter(settings);

		ParticleOverrides overrides;
		overrides.position = FVector3{ 5.0f, 1.0f, 0.0f };
		overrides.direction = FVector3{ 0.0f, 0.0f, 2.0f };
		overrides.color = FVector4{ 0.2f, 0.4f, 0.6f, 0.8f };
		overrides.speed = 3.0f;
		overrides.size = 2.0f;
		overrides.lifetime = 0.5f;

		// In the world, while the system's space is the emitter's: the emitter stands at x = 4
		CHECK(emitter.Emit(4, FMatrix4::Translation({ 4.0f, 0.0f, 0.0f }), overrides) == 4);

		for (const auto* record : Born(emitter))
		{
			CHECK(record->position.x == doctest::Approx(1.0f));
			CHECK(record->position.y == doctest::Approx(1.0f));
			CHECK(record->velocity.z == doctest::Approx(3.0f));
			CHECK(record->color.y == doctest::Approx(0.4f));
			CHECK(record->size == doctest::Approx(2.0f));
			CHECK(record->lifetime == doctest::Approx(0.5f));
		}

		emitter.Advance(0.6f, FMatrix4::Identity);
		CHECK(emitter.GetAliveCount() == 0);
	}

	TEST_CASE("only what changed goes to the GPU, and the clock moves back without moving a particle")
	{
		auto settings = Quiet();
		settings.maxParticles = 10;
		settings.lifetimeMin = settings.lifetimeMax = 1000.0f;
		settings.gravity = { 0.0f, -1.0f, 0.0f };
		settings.speedMin = settings.speedMax = 1.0f;
		ParticleEmitter emitter(settings);

		// Everything once, then the slots written: around the ring's end, in two ranges
		CHECK(emitter.TakeChangedSlots().front().count == 10);
		emitter.Emit(8, FMatrix4::Identity);
		emitter.TakeChangedSlots();
		emitter.Emit(4, FMatrix4::Identity);
		const auto changed = emitter.TakeChangedSlots();
		REQUIRE(changed.size() == 2);
		CHECK(changed[0].first == 8);
		CHECK(changed[0].count == 2);
		CHECK(changed[1].first == 0);
		CHECK(changed[1].count == 2);
		CHECK(emitter.TakeChangedSlots().empty());

		// Ten minutes on, the clock goes back to near 0 and the births with it
		const auto& record = emitter.GetRecords()[3];
		for (int i = 0; i < 599; ++i)
		{
			emitter.Advance(1.0f, FMatrix4::Identity);
		}
		const auto before = emitter.Evaluate(record);
		emitter.Advance(2.0f, FMatrix4::Identity);
		const auto after = emitter.Evaluate(record);

		CHECK(emitter.GetTime() < 10.0f);
		CHECK(after.alive);
		CHECK(after.position.y == doctest::Approx(before.position.y + 2.0f * record.velocity.y - 0.5f * (601.0f * 601.0f - 599.0f * 599.0f)).epsilon(1e-4));
		CHECK(emitter.TakeChangedSlots().front().count == 10);
	}

	TEST_CASE("the component plays on start, simulates with its scene, and is saved and read back")
	{
		OvCore::SceneSystem::Scene scene("", "");
		auto& actor = scene.CreateActor("Sparks");
		actor.transform.SetLocalPosition({ 3.0f, 0.0f, 0.0f });

		auto& system = actor.AddComponent<OvCore::ECS::Components::CParticleSystem>();
		REQUIRE(scene.GetFastAccessComponents().particleSystems.size() == 1);

		auto settings = system.GetSettings();
		settings.rate = 10.0f;
		settings.maxParticles = 64;
		settings.bursts = { { 0.0f, 5 }, { 0.75f, 2 } };
		settings.shape = EParticleShape::BOX;
		settings.space = EParticleSpace::LOCAL;
		settings.colorEnd = { 0.1f, 0.2f, 0.3f, 0.4f };
		settings.gravity = { 0.0f, -2.0f, 0.0f };
		settings.seed = 9;
		system.SetSettings(settings);

		scene.Play();
		CHECK(system.IsPlaying());
		scene.LateUpdate(0.5f);
		CHECK(system.GetAliveCount() == 10);

		// No GPU is needed to emit and count: the buffer is made on the first draw
		CHECK(system.Emit(3) == 3);
		CHECK(system.GetAliveCount() == 13);
		CHECK(system.GetWorldBounds().has_value());

		tinyxml2::XMLDocument document;
		auto* node = document.NewElement("data");
		document.InsertFirstChild(node);
		system.OnSerialize(document, node);

		auto& other = scene.CreateActor("Copy").AddComponent<OvCore::ECS::Components::CParticleSystem>();
		other.OnDeserialize(document, node);
		const auto& read = other.GetSettings();
		CHECK(read.rate == doctest::Approx(10.0f));
		CHECK(read.maxParticles == 64);
		REQUIRE(read.bursts.size() == 2);
		CHECK(read.bursts[1].time == doctest::Approx(0.75f));
		CHECK(read.bursts[1].count == 2);
		CHECK(read.shape == EParticleShape::BOX);
		CHECK(read.space == EParticleSpace::LOCAL);
		CHECK(read.colorEnd.w == doctest::Approx(0.4f));
		CHECK(read.gravity.y == doctest::Approx(-2.0f));
		CHECK(read.seed == 9);
		CHECK(other.GetModel() == nullptr);
		CHECK(other.GetMaterial() == nullptr);

		// An inactive actor's system waits
		actor.SetActive(false);
		const auto alive = system.GetAliveCount();
		scene.LateUpdate(5.0f);
		CHECK(system.GetAliveCount() == alive);

		actor.RemoveComponent<OvCore::ECS::Components::CParticleSystem>();
		CHECK(scene.GetFastAccessComponents().particleSystems.size() == 1);
	}
}
