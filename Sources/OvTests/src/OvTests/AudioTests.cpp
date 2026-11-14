/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cmath>
#include <cstdint>
#include <fstream>
#include <memory>
#include <numbers>
#include <vector>

#include <doctest/doctest.h>
#include <soloud.h>

#include <OvAudio/Core/AudioEngine.h>
#include <OvAudio/Data/SoundInstance.h>
#include <OvAudio/Entities/AudioListener.h>
#include <OvAudio/Entities/AudioSource.h>
#include <OvAudio/Resources/Loaders/SoundLoader.h>
#include <OvAudio/Resources/Sound.h>
#include <OvMaths/FQuaternion.h>
#include <OvMaths/FTransform.h>
#include <OvTests/TestUtils.h>
#include <OvTools/Utils/PathParser.h>

using OvAudio::Core::AudioEngine;
using OvAudio::Core::EDriver;
using OvAudio::Entities::AudioListener;
using OvAudio::Entities::AudioSource;
using OvAudio::Resources::Sound;
using OvAudio::Resources::Loaders::SoundLoader;

namespace
{
	constexpr uint32_t kRate = AudioEngine::kSampleRate;

	/**
	* A mono WAV of 16-bit samples
	*/
	void WriteWav(const std::filesystem::path& p_path, const std::vector<float>& p_samples)
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

	std::vector<float> Sine(float p_frequency, float p_seconds, float p_amplitude = 0.5f)
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

	Mixed MixFor(AudioEngine& p_engine, float p_seconds)
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

	float Loudness(const Mixed& p_mixed)
	{
		return std::sqrt(p_mixed.left * p_mixed.left + p_mixed.right * p_mixed.right);
	}

	/**
	* A value within a share of another. doctest's Approx alone also allows its epsilon as an absolute error, which
	* lets small loudnesses through whatever they are
	*/
	doctest::Approx Near(double p_value, double p_share)
	{
		return doctest::Approx(p_value).epsilon(p_share).scale(0.0);
	}

	/**
	* SoLoud glides a change of volume, pan or position across one mix of up to 2048 frames: once the sources and the
	* listener are updated, this lets the change land before a test listens
	*/
	void Settle(AudioEngine& p_engine)
	{
		p_engine.Update(0.0f);
		MixFor(p_engine, 0.06f);
	}

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

TEST_SUITE("Audio")
{
	TEST_CASE("the null driver plays a sound as a device would, and says so")
	{
		NullEngine null;
		CHECK(null.engine.GetDriverName() == "null");
		CHECK(null.engine.GetVoiceCount() == 0);

		TestSound tone(Sine(440.0f, 0.5f));
		AudioSource source(null.engine);
		source.Play(*tone.sound);
		CHECK(source.IsPlaying());
		CHECK(null.engine.GetVoiceCount() == 1);

		const Mixed mixed = MixFor(null.engine, 0.2f);
		CHECK(Loudness(mixed) > 0.1f);
		CHECK(mixed.frequency == Near(440.0f, 0.02));

		// Played to its end, the sound stops and leaves its voice
		MixFor(null.engine, 0.4f);
		CHECK_FALSE(source.HasSound());
		CHECK(null.engine.GetVoiceCount() == 0);
		CHECK(Loudness(MixFor(null.engine, 0.1f)) < 1e-4f);
	}

	TEST_CASE("on the null driver, each update mixes its step, so that sounds keep step with the game")
	{
		AudioEngine engine{ EDriver::NULL_DRIVER };
		REQUIRE(engine.IsNullDriver());

		TestSound tone(Sine(440.0f, 0.5f));
		AudioSource source(engine);

		// Steps of 1/144 s are no whole number of samples: they add up without drifting
		source.Play(*tone.sound);
		for (int frame = 0; frame < 66; ++frame) // 0.458 s
		{
			engine.Update(1.0f / 144.0f);
		}
		CHECK(source.IsPlaying());
		for (int frame = 0; frame < 12; ++frame) // 0.542 s
		{
			engine.Update(1.0f / 144.0f);
		}
		CHECK_FALSE(source.HasSound());

		// A time no update counts (a pause, an editor not playing) moves no sound, and a long step mixes a quarter of a
		// second at most
		source.Play(*tone.sound);
		engine.Update(0.0f);
		engine.Update(5.0f);
		CHECK(source.IsPlaying());
		engine.Update(0.2f);
		CHECK(source.IsPlaying());
		engine.Update(0.1f);
		CHECK_FALSE(source.HasSound());
	}

	TEST_CASE("volume, pan, pitch and looping shape what is heard")
	{
		NullEngine null;
		TestSound tone(Sine(440.0f, 0.25f));
		AudioSource source(null.engine);

		source.SetLooped(true);
		source.Play(*tone.sound);
		const float full = Loudness(MixFor(null.engine, 0.1f));

		source.SetVolume(0.5f);
		Settle(null.engine);
		CHECK(Loudness(MixFor(null.engine, 0.1f)) == Near(full * 0.5f, 0.03));

		// Pan: one side only
		source.SetVolume(1.0f);
		source.SetPan(-1.0f);
		Settle(null.engine);
		const Mixed left = MixFor(null.engine, 0.1f);
		CHECK(left.left > 0.1f);
		CHECK(left.right < left.left * 0.01f);
		source.SetPan(1.0f);
		Settle(null.engine);
		const Mixed right = MixFor(null.engine, 0.1f);
		CHECK(right.right > 0.1f);
		CHECK(right.left < right.right * 0.01f);
		source.SetPan(0.0f);
		Settle(null.engine);

		// Pitch: twice as fast, an octave up
		source.SetPitch(2.0f);
		CHECK(MixFor(null.engine, 0.2f).frequency == Near(880.0f, 0.02));
		source.SetPitch(1.0f);

		// Looped, it plays past its length
		MixFor(null.engine, 0.6f);
		CHECK(source.IsPlaying());
		CHECK(Loudness(MixFor(null.engine, 0.1f)) == Near(full, 0.03));

		source.Stop();
		CHECK_FALSE(source.HasSound());
		CHECK(Loudness(MixFor(null.engine, 0.1f)) < 1e-4f);
	}

	TEST_CASE("a sound in the world is heard from the side the listener's camera would show it on, and fades with distance")
	{
		NullEngine null;
		TestSound tone(Sine(440.0f, 0.25f));

		// A listener as a camera with no turn: it looks along +Z, and shows +X on the left of its picture
		OvMaths::FTransform listenerTransform;
		AudioListener listener(listenerTransform);

		OvMaths::FTransform sourceTransform;
		AudioSource source(null.engine, sourceTransform);
		source.SetSpatial(true);
		source.SetLooped(true);

		// Played before any update, it begins on its side: the engine's listener holds from the start
		sourceTransform.SetLocalPosition({ 5.0f, 0.0f, 0.0f });
		source.Play(*tone.sound);
		const Mixed atStart = MixFor(null.engine, 0.02f);
		CHECK(atStart.left > atStart.right * 2.0f);

		Settle(null.engine);
		const Mixed onLeft = MixFor(null.engine, 0.1f);
		CHECK(onLeft.left > onLeft.right * 2.0f);

		sourceTransform.SetLocalPosition({ -5.0f, 0.0f, 0.0f });
		Settle(null.engine);
		const Mixed onRight = MixFor(null.engine, 0.1f);
		CHECK(onRight.right > onRight.left * 2.0f);

		// Turned about, the listener hears the same source on its other side
		listenerTransform.SetLocalRotation(OvMaths::FQuaternion({ 0.0f, 180.0f, 0.0f }));
		Settle(null.engine);
		const Mixed turned = MixFor(null.engine, 0.1f);
		CHECK(turned.left > turned.right * 2.0f);
		listenerTransform.SetLocalRotation(OvMaths::FQuaternion::Identity);

		// In front, four times as far: a quarter as loud (exponential, from a threshold of 1 metre)
		sourceTransform.SetLocalPosition({ 0.0f, 0.0f, 2.0f });
		Settle(null.engine);
		const float closer = Loudness(MixFor(null.engine, 0.1f));
		sourceTransform.SetLocalPosition({ 0.0f, 0.0f, 8.0f });
		Settle(null.engine);
		const float further = Loudness(MixFor(null.engine, 0.1f));
		CHECK(further == Near(closer * 0.25f, 0.03));

		// Within the threshold, no fading
		source.SetAttenuationThreshold(10.0f);
		Settle(null.engine);
		const float within = Loudness(MixFor(null.engine, 0.1f));
		sourceTransform.SetLocalPosition({ 0.0f, 0.0f, 2.0f });
		Settle(null.engine);
		CHECK(Loudness(MixFor(null.engine, 0.1f)) == Near(within, 0.03));
	}

	TEST_CASE("a sound playing keeps playing, where it was, when it becomes spatial or flat")
	{
		NullEngine null;
		TestSound tone(Sine(440.0f, 1.0f));
		AudioSource source(null.engine);
		source.Play(*tone.sound);
		MixFor(null.engine, 0.3f);

		source.SetSpatial(true);
		REQUIRE(source.HasSound());
		CHECK(source.GetSoundInstance().lock()->IsSpatial());
		CHECK(source.IsPlaying());
		CHECK(null.engine.GetBackend().getStreamPosition(source.GetSoundInstance().lock()->GetHandle()) == Near(0.3, 0.02));

		// Paused, it stays paused
		source.Pause();
		source.SetSpatial(false);
		REQUIRE(source.HasSound());
		CHECK_FALSE(source.GetSoundInstance().lock()->IsSpatial());
		CHECK(source.IsPaused());
		CHECK(null.engine.GetVoiceCount() == 1);
	}

	TEST_CASE("resuming after a suspension leaves paused what the game paused")
	{
		NullEngine null;
		TestSound tone(Sine(440.0f, 0.25f));
		AudioSource paused(null.engine);
		AudioSource playing(null.engine);

		for (AudioSource* source : { &paused, &playing })
		{
			source->SetLooped(true);
			source->Play(*tone.sound);
		}

		paused.Pause();
		null.engine.Suspend();
		CHECK(null.engine.IsSuspended());
		CHECK(playing.IsPaused());

		null.engine.Unsuspend();
		CHECK_FALSE(null.engine.IsSuspended());
		CHECK(playing.IsPlaying());
		CHECK(paused.IsPaused());
	}

	TEST_CASE("a file that cannot be read gives no sound, and FLAC files are sounds")
	{
		OvTests::TempFolder folder;
		OvTests::LogCapture logs;

		CHECK(SoundLoader::Create((folder.GetPath() / "Missing.wav").string()) == nullptr);
		const auto garbage = folder.WriteFile("Garbage.ogg", "not a sound");
		CHECK(SoundLoader::Create(garbage.string()) == nullptr);
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_WARNING, "Cannot read the sound"));

		using OvTools::Utils::PathParser;
		CHECK(PathParser::GetFileType("Music.flac") == PathParser::EFileType::SOUND);
		CHECK(PathParser::GetFileType("Hit.wav") == PathParser::EFileType::SOUND);
	}
}
