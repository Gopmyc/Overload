/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <vector>

#include <doctest/doctest.h>

#include <OvAudio/Core/AudioEngine.h>
#include <OvAudio/Data/SoundInstance.h>
#include <OvAudio/Resources/Loaders/SoundLoader.h>
#include <OvAudio/Resources/Sound.h>
#include <OvTests/TestUtils.h>

/**
* What the audio tests share: sounds written by the test, and an engine on the null driver mixed by the test, whose
* signal they measure
*/
namespace OvTests::Audio
{
	using OvAudio::Core::AudioEngine;
	using OvAudio::Core::EDriver;
	using OvAudio::Resources::Sound;
	using OvAudio::Resources::Loaders::SoundLoader;

	constexpr uint32_t kRate = AudioEngine::kSampleRate;

	/**
	* A mono WAV of 16-bit samples
	*/
	inline void WriteWav(const std::filesystem::path& p_path, const std::vector<float>& p_samples)
	{
		const auto put32 = [](std::ofstream& p_stream, uint32_t p_value) { p_stream.write(reinterpret_cast<const char*>(&p_value), 4); };
		const auto put16 = [](std::ofstream& p_stream, uint16_t p_value) { p_stream.write(reinterpret_cast<const char*>(&p_value), 2); };

		std::ofstream stream(p_path, std::ios::binary);
		const auto dataSize = static_cast<uint32_t>(p_samples.size() * 2);
		stream.write("RIFF", 4);
		put32(stream, 36 + dataSize);
		stream.write("WAVEfmt ", 8);
		put32(stream, 16);
		put16(stream, 1); // PCM
		put16(stream, 1); // Mono
		put32(stream, kRate);
		put32(stream, kRate * 2);
		put16(stream, 2);
		put16(stream, 16);
		stream.write("data", 4);
		put32(stream, dataSize);

		for (const float sample : p_samples)
		{
			put16(stream, static_cast<uint16_t>(static_cast<int16_t>(std::lround(std::clamp(sample, -1.0f, 1.0f) * 32767.0f))));
		}
	}

	inline std::vector<float> Sine(float p_frequency, float p_seconds, float p_amplitude = 0.5f)
	{
		std::vector<float> samples(static_cast<size_t>(p_seconds * kRate));
		for (size_t index = 0; index < samples.size(); ++index)
		{
			samples[index] = p_amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * p_frequency * static_cast<double>(index) / kRate));
		}

		return samples;
	}

	/**
	* A sound written to a temporary file and read back as the engine reads one
	*/
	struct TestSound
	{
		OvTests::TempFolder folder;
		Sound* sound = nullptr;

		explicit TestSound(const std::vector<float>& p_samples)
		{
			const auto path = folder.GetPath() / "Test.wav";
			WriteWav(path, p_samples);
			sound = SoundLoader::Create(path.string());
			REQUIRE(sound != nullptr);
		}

		~TestSound()
		{
			SoundLoader::Destroy(sound);
		}
	};

	/**
	* What a stretch of mixing gave: each channel's loudness, and its frequency counted by crossings of zero
	*/
	struct Mixed
	{
		float left = 0.0f;
		float right = 0.0f;
		float frequency = 0.0f;
	};

	inline Mixed MixFor(AudioEngine& p_engine, float p_seconds)
	{
		const auto frames = static_cast<uint32_t>(p_seconds * kRate);
		std::vector<float> buffer(static_cast<size_t>(frames) * AudioEngine::kChannels);
		REQUIRE(p_engine.Mix(buffer.data(), frames));

		double left = 0.0;
		double right = 0.0;
		uint32_t crossings = 0;

		for (uint32_t frame = 0; frame < frames; ++frame)
		{
			const float l = buffer[frame * 2];
			const float r = buffer[frame * 2 + 1];
			left += static_cast<double>(l) * l;
			right += static_cast<double>(r) * r;

			const float mono = l + r;
			const float previous = frame > 0 ? buffer[(frame - 1) * 2] + buffer[(frame - 1) * 2 + 1] : 0.0f;
			if (frame > 0 && (previous < 0.0f) != (mono < 0.0f))
			{
				++crossings;
			}
		}

		return {
			static_cast<float>(std::sqrt(left / frames)),
			static_cast<float>(std::sqrt(right / frames)),
			static_cast<float>(crossings) / 2.0f / p_seconds
		};
	}

	inline float Loudness(const Mixed& p_mixed)
	{
		return std::sqrt(p_mixed.left * p_mixed.left + p_mixed.right * p_mixed.right);
	}

	/**
	* A value within a share of another. doctest's Approx alone also allows its epsilon as an absolute error, which
	* lets small loudnesses through whatever they are
	*/
	inline doctest::Approx Near(double p_value, double p_share)
	{
		return doctest::Approx(p_value).epsilon(p_share).scale(0.0);
	}

	/**
	* SoLoud glides a change of volume, pan or position across one mix of up to 2048 frames: once the sources and the
	* listener are updated, this lets the change land before a test listens
	*/
	inline void Settle(AudioEngine& p_engine)
	{
		p_engine.Update(0.0f);
		MixFor(p_engine, 0.06f);
	}

	/**
	* Returns the speed SoLoud plays a sound at: its pitch, with the variation drawn for it. It is defined in
	* AudioTestUtils.cpp, which includes <soloud.h> so that the tests need not (see there)
	*/
	float SpeedOf(AudioEngine& p_engine, const OvAudio::Data::SoundInstance& p_instance);

	/**
	* An engine on the null driver, whose time only moves when a test mixes
	*/
	struct NullEngine
	{
		AudioEngine engine{ EDriver::NULL_DRIVER };

		NullEngine()
		{
			REQUIRE(engine.IsNullDriver());
			engine.SetManualMixing(true);
		}
	};
}
