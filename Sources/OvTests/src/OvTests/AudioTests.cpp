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
#include <OvTests/AudioTestUtils.h>
#include <OvTests/TestUtils.h>
#include <OvTools/Utils/PathParser.h>

using OvAudio::Core::AudioEngine;
using OvAudio::Core::EDriver;
using OvAudio::Entities::AudioListener;
using OvAudio::Entities::AudioSource;
using OvAudio::Resources::Sound;
using OvAudio::Resources::Loaders::SoundLoader;

using namespace OvTests::Audio;

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
