/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <OvMaths/FMatrix4.h>
#include <OvMaths/FVector3.h>
#include <OvMaths/FVector4.h>
#include <OvRendering/Geometry/BoundingSphere.h>

namespace OvCore::Particles
{
	/**
	* Where particles are born, and the way they leave. The cone and the hemisphere open along the emitter's up axis
	* (local +Y), and a box sends its particles that way
	*/
	enum class EParticleShape : uint8_t
	{
		POINT,
		SPHERE,
		HEMISPHERE,
		CONE,
		BOX
	};

	/**
	* The space particles live in: the world (they stay where they were born) or the emitter's (they follow it)
	*/
	enum class EParticleSpace : uint8_t
	{
		WORLD,
		LOCAL
	};

	/**
	* A number of particles born at once, a time after the start of each cycle
	*/
	struct ParticleBurst
	{
		float time = 0.0f;
		uint32_t count = 0;
	};

	/**
	* What a particle system emits, and how its particles live
	*/
	struct ParticleSettings
	{
		static constexpr uint32_t kMaxParticles = 100000;

		uint32_t maxParticles = 256;		// The particles alive at once: one born past it replaces the oldest
		float rate = 20.0f;					// Particles born per second, while playing
		std::vector<ParticleBurst> bursts;	// Particles born at once, in each cycle
		float duration = 2.0f;				// Seconds of a cycle
		bool looping = true;				// Whether a cycle follows the last one, or the system stops emitting
		bool playOnStart = true;			// Whether the system plays when the scene starts

		float lifetimeMin = 1.0f;			// Seconds a particle lives, drawn between the two
		float lifetimeMax = 1.5f;
		float speedMin = 1.0f;				// Speed at birth, in units per second, drawn between the two
		float speedMax = 2.0f;

		EParticleShape shape = EParticleShape::CONE;
		float shapeRadius = 0.1f;			// Of a sphere, a hemisphere, or a cone's base
		float shapeAngle = 25.0f;			// Of a cone, in degrees from its axis
		OvMaths::FVector3 shapeSize{ 1.0f, 1.0f, 1.0f }; // Of a box

		OvMaths::FVector3 gravity{ 0.0f, 0.0f, 0.0f }; // Acceleration, in units per second squared
		float drag = 0.0f;					// Share of the speed lost per second, exponentially

		float sizeStart = 0.2f;				// Size over a particle's life, in units
		float sizeEnd = 0.05f;
		OvMaths::FVector4 colorStart{ 1.0f, 1.0f, 1.0f, 1.0f }; // Colour over a particle's life
		OvMaths::FVector4 colorEnd{ 1.0f, 1.0f, 1.0f, 0.0f };
		float fadeIn = 0.0f;				// Shares of the life over which the opacity rises from 0, and falls to 0
		float fadeOut = 0.0f;

		float spinMin = 0.0f;				// Turning speed, in degrees per second, drawn between the two
		float spinMax = 0.0f;
		bool randomRotation = false;		// Whether a particle is born at a random angle

		EParticleSpace space = EParticleSpace::WORLD;
		bool unscaled = false;				// Whether the system runs on real time instead of game time
		uint32_t seed = 0;					// The same seed gives the same particles; 0 gives every system its own
	};

	/**
	* What a burst emitted from a script changes, for each of its particles
	*/
	struct ParticleOverrides
	{
		std::optional<OvMaths::FVector3> position;	// Where they are born, in the world, instead of the shape
		std::optional<OvMaths::FVector3> direction;	// Where they go, in the world, instead of the shape's way
		std::optional<OvMaths::FVector4> color;		// Their colour, instead of white, multiplied by the colour over life
		std::optional<float> speed;					// Their speed, instead of one drawn
		std::optional<float> size;					// Their size, times the size over life
		std::optional<float> lifetime;				// Their life, instead of one drawn
	};

	/**
	* A particle as the GPU reads it (std430, 64 bytes): what it was at birth, from which the shader works out the rest
	*/
	struct ParticleRecord
	{
		OvMaths::FVector3 position;	// At birth, in the system's space
		float birth = 0.0f;			// On the system's clock, in seconds
		OvMaths::FVector3 velocity;	// At birth
		float lifetime = 0.0f;		// 0 for a slot never used or cleared
		OvMaths::FVector4 color;	// Multiplied by the colour over life
		float size = 1.0f;			// Multiplied by the size over life
		float rotation = 0.0f;		// At birth, in radians
		float spin = 0.0f;			// In radians per second
		float seed = 0.0f;			// In [0, 1), for a shader's own variety
	};

	static_assert(sizeof(ParticleRecord) == 64, "a particle record must match the shader's layout");

	/**
	* A particle at a given time, as the shader draws it
	*/
	struct ParticleState
	{
		bool alive = false;
		OvMaths::FVector3 position;
		OvMaths::FVector3 velocity;
		float size = 0.0f;
		OvMaths::FVector4 color;
		float rotation = 0.0f;
		float life = 0.0f;			// The share of its life it has lived, in [0, 1)
	};

	/**
	* The head of a particle system's GPU buffer (std430, 160 bytes), followed by its records
	*/
	struct ParticleBufferHeader
	{
		OvMaths::FMatrix4 localToWorld;	// Column-major, as GLSL reads it: the emitter's for local space, else identity
		float gravity[3];
		float drag;
		float colorStart[4];
		float colorEnd[4];
		float sizeStart;
		float sizeEnd;
		float fadeIn;
		float fadeOut;
		float time;						// The system's clock, as the records' births count it
		float padding[3];
		uint32_t capacity;
		uint32_t first;					// The ring slot of the first instance drawn
		uint32_t count;					// The instances drawn
		uint32_t space;
	};

	static_assert(sizeof(ParticleBufferHeader) == 160, "the particle buffer's head must match the shader's layout");

	/**
	* A range of ring slots
	*/
	struct ParticleSlotRange
	{
		uint32_t first = 0;
		uint32_t count = 0;
	};

	/**
	* Emits particles into a ring of records, on its own clock, without any GPU: what a particle system simulates,
	* and what the engine's tests check. A particle is never moved once born: its record holds what it was at birth,
	* and Evaluate (or the shader) works out where it is at any time
	*/
	class ParticleEmitter
	{
	public:
		/**
		* Creates an emitter, stopped
		* @param p_settings
		*/
		explicit ParticleEmitter(const ParticleSettings& p_settings = {});

		/**
		* Changes the settings, bringing each into its range. A new number of particles clears the ring
		* @param p_settings
		*/
		void SetSettings(const ParticleSettings& p_settings);

		/**
		* Returns the settings
		*/
		const ParticleSettings& GetSettings() const;

		/**
		* Starts emitting from the start of a cycle
		*/
		void Play();

		/**
		* Stops emitting: the particles alive live on
		*/
		void Stop();

		/**
		* Removes every particle, and draws the seed again
		*/
		void Clear();

		/**
		* Returns true while the emitter emits
		*/
		bool IsPlaying() const;

		/**
		* Moves the clock on, emitting what the rate and the bursts give over that time, from an emitter moving from
		* where it was at the previous call to where it is now
		* @param p_deltaTime
		* @param p_emitterWorld (the emitter's world matrix)
		*/
		void Advance(float p_deltaTime, const OvMaths::FMatrix4& p_emitterWorld);

		/**
		* Emits particles now, playing or not, and returns how many were born
		* @param p_count
		* @param p_emitterWorld
		* @param p_overrides
		*/
		uint32_t Emit(uint32_t p_count, const OvMaths::FMatrix4& p_emitterWorld, const ParticleOverrides& p_overrides = {});

		/**
		* Returns the particles alive
		*/
		uint32_t GetAliveCount() const;

		/**
		* Returns the clock, in seconds since the emitter was made (or its clock was last moved back)
		*/
		float GetTime() const;

		/**
		* Returns the ring of records
		*/
		std::span<const ParticleRecord> GetRecords() const;

		/**
		* Returns a particle at the emitter's time
		* @param p_record
		*/
		ParticleState Evaluate(const ParticleRecord& p_record) const;

		/**
		* Returns a particle at a time, by the formulas of the shader
		* @param p_record
		* @param p_settings
		* @param p_time
		*/
		static ParticleState Evaluate(const ParticleRecord& p_record, const ParticleSettings& p_settings, float p_time);

		/**
		* Returns a sphere holding every particle alive, in the particles' space, or nothing when none is
		*/
		std::optional<OvRendering::Geometry::BoundingSphere> GetBounds() const;

		/**
		* Returns the ring slots to draw: from the oldest particle alive to the newest
		*/
		ParticleSlotRange GetDrawRange() const;

		/**
		* Returns the head of the GPU buffer
		* @param p_emitterWorld
		*/
		ParticleBufferHeader BuildHeader(const OvMaths::FMatrix4& p_emitterWorld) const;

		/**
		* Returns the slots written since the last call (all of them after a clear), and forgets them
		*/
		std::vector<ParticleSlotRange> TakeChangedSlots();

	private:
		struct Stats
		{
			uint32_t alive = 0;
			ParticleSlotRange drawRange;
			std::optional<OvRendering::Geometry::BoundingSphere> bounds;
		};

		void ResetRing();
		void Reseed();
		float Random();
		float Random(float p_min, float p_max);
		OvMaths::FVector3 RandomDirection();
		void SampleShape(OvMaths::FVector3& p_position, OvMaths::FVector3& p_direction);
		void Spawn(float p_birth, const OvMaths::FMatrix4& p_emitterWorld, const ParticleOverrides& p_overrides);
		void EmitOverStep(double p_stepStart, double p_step, double p_frameStart, float p_frameLength, const OvMaths::FMatrix4& p_emitterWorld);
		void Rebase();
		const Stats& GetStats() const;

	private:
		ParticleSettings m_settings;
		std::vector<ParticleRecord> m_records;
		std::vector<uint64_t> m_birthOrder;	// The emission number of each slot's particle, 0 for none

		double m_time = 0.0;				// Since the clock's origin, which moves forward now and then (see Rebase)
		double m_cycleTime = 0.0;
		double m_rateCarry = 0.0;
		bool m_playing = false;

		uint32_t m_next = 0;
		uint64_t m_emitted = 0;
		uint64_t m_random = 0;

		std::optional<OvMaths::FVector3> m_previousOrigin;
		OvMaths::FVector3 m_currentOrigin;

		ParticleSlotRange m_changed;
		bool m_allChanged = true;

		mutable Stats m_stats;
		mutable bool m_statsDirty = true;
	};
}
