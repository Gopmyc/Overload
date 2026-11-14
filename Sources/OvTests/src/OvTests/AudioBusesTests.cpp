/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cmath>
#include <vector>

#include <doctest/doctest.h>
#include <soloud.h>
#include <tinyxml2.h>

#include <OvAudio/Core/AudioEngine.h>
#include <OvAudio/Data/SoundInstance.h>
#include <OvAudio/Entities/AudioSource.h>
#include <OvAudio/Settings/AttenuationSettings.h>
#include <OvCore/ECS/Components/CAudioSource.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Helpers/ProjectSettings.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvMaths/FTransform.h>
#include <OvTests/AudioTestUtils.h>
#include <OvTests/TestUtils.h>
#include <OvTools/Filesystem/IniFile.h>

using namespace OvTests::Audio;
using OvAudio::Core::OneShotSettings;
using OvAudio::Entities::AudioSource;
using OvAudio::Settings::AttenuationSettings;
using OvAudio::Settings::EAttenuationModel;

namespace
{
	/**
	* An engine on the null driver with the default buses of a project
	*/
	struct BusEngine : NullEngine
	{
		BusEngine()
		{
			engine.SetBuses({ "Music", "Effects", "Interface", "Ambience" });
		}
	};

	OneShotSettings Flat(float p_volume = 1.0f)
	{
		OneShotSettings settings;
		settings.volume = p_volume;
		return settings;
	}

	/**
	* The loudness of a looping tone in front of a listener with no turn, at a distance, once it settled
	*/
	float LoudnessAt(AudioEngine& p_engine, OvMaths::FTransform& p_transform, float p_distance)
	{
		p_transform.SetLocalPosition({ 0.0f, 0.0f, p_distance });
		Settle(p_engine);
		return Loudness(MixFor(p_engine, 0.1f));
	}

	float SpeedOf(AudioEngine& p_engine, const OvAudio::Data::SoundInstance& p_instance)
	{
		return p_engine.GetBackend().getRelativePlaySpeed(p_instance.GetHandle());
	}
}

TEST_SUITE("Audio buses")
{
	TEST_CASE("a bus and the Master set the volume of what plays on them")
	{
		BusEngine null;
		auto& engine = null.engine;

		CHECK(engine.GetBusNames() == std::vector<std::string>{ "Music", "Effects", "Interface", "Ambience" });
		CHECK(engine.HasBus("Master"));
		CHECK(engine.HasBus("Effects"));
		CHECK_FALSE(engine.HasBus("Nope"));
		CHECK(engine.GetVoiceCount() == 0); // The buses play as voices, uncounted

		TestSound tone(Sine(440.0f, 0.25f, 0.25f));
		AudioSource source(engine);
		source.SetLooped(true);
		source.SetBus("Effects");
		source.Play(*tone.sound);
		CHECK(engine.GetVoiceCount() == 1);
		Settle(engine);
		const float full = Loudness(MixFor(engine, 0.1f));
		CHECK(full > 0.05f);

		CHECK(engine.SetVolume("Effects", 0.5f));
		CHECK(*engine.GetVolume("Effects") == doctest::Approx(0.5f));
		Settle(engine);
		CHECK(Loudness(MixFor(engine, 0.1f)) == Near(full * 0.5f, 0.03));

		// The Master scales everything, the buses too; another bus changes nothing here
		CHECK(engine.SetVolume("Master", 0.5f));
		CHECK(engine.SetVolume("Music", 0.0f));
		Settle(engine);
		CHECK(Loudness(MixFor(engine, 0.1f)) == Near(full * 0.25f, 0.03));
		engine.SetVolume("Master", 1.0f);

		CHECK_FALSE(engine.SetVolume("Nope", 0.5f));
		CHECK_FALSE(engine.GetVolume("Nope").has_value());
		CHECK(engine.SetVolume("Effects", -2.0f));
		CHECK(*engine.GetVolume("Effects") == 0.0f);

		// Back to 1, all of them, the Master too
		engine.SetVolume("Master", 0.2f);
		engine.ResetVolumes();
		CHECK(*engine.GetVolume("Effects") == 1.0f);
		CHECK(*engine.GetVolume("Music") == 1.0f);
		CHECK(*engine.GetVolume("Master") == 1.0f);
		engine.SetVolume("Effects", 0.5f);
		engine.SetVolume("Music", 0.0f);

		// A sound playing moves to another bus where it was, and back to the Master
		source.SetBus("Music");
		CHECK(source.IsPlaying());
		Settle(engine);
		CHECK(Loudness(MixFor(engine, 0.1f)) < 1e-4f);
		source.SetBus("Master");
		CHECK(source.GetBus().empty());
		Settle(engine);
		CHECK(Loudness(MixFor(engine, 0.1f)) == Near(full, 0.03));
		CHECK(engine.GetVoiceCount() == 1);
	}

	TEST_CASE("a sound on a silent bus still ends on time")
	{
		BusEngine null;
		auto& engine = null.engine;
		engine.SetVolume("Effects", 0.0f);
		Settle(engine);

		TestSound tone(Sine(440.0f, 0.2f, 0.25f));
		AudioSource source(engine);
		source.SetBus("Effects");
		source.Play(*tone.sound);

		CHECK(Loudness(MixFor(engine, 0.1f)) < 1e-4f);
		CHECK(source.IsPlaying());
		MixFor(engine, 0.15f);
		CHECK_FALSE(source.HasSound());
		CHECK(engine.GetVoiceCount() == 0);
	}

	TEST_CASE("the same buses keep their volumes, other ones start at 1, and a bus that goes plays on the Master")
	{
		BusEngine null;
		auto& engine = null.engine;

		engine.SetVolume("Music", 0.3f);
		engine.SetBuses({ "Music", "Effects", "Interface", "Ambience" });
		CHECK(*engine.GetVolume("Music") == doctest::Approx(0.3f));

		engine.SetBuses({ "Voices", "Music" });
		CHECK(engine.GetBusNames() == std::vector<std::string>{ "Voices", "Music" });
		CHECK(*engine.GetVolume("Music") == doctest::Approx(1.0f));
		CHECK_FALSE(engine.HasBus("Effects"));
		CHECK(engine.GetVoiceCount() == 0);

		// At most kMaxBuses
		engine.SetBuses({ "A", "B", "C", "D", "E", "F", "G", "H", "I", "J" });
		CHECK(engine.GetBusNames().size() == AudioEngine::kMaxBuses);
		CHECK(engine.GetVoiceCount() == 0);

		OvTests::LogCapture logs;
		TestSound tone(Sine(440.0f, 0.25f, 0.25f));
		AudioSource source(engine);
		source.SetLooped(true);
		source.SetBus("Effects");
		source.Play(*tone.sound);
		Settle(engine);
		CHECK(Loudness(MixFor(engine, 0.1f)) > 0.05f);
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_WARNING, "No audio bus \"Effects\""));
	}

	TEST_CASE("sounds played once overlap, end by themselves, and stop with their source")
	{
		BusEngine null;
		auto& engine = null.engine;
		TestSound tone(Sine(440.0f, 0.2f, 0.2f));

		// Measured once under way: a bus starts its sounds a little later than the Master
		const auto first = engine.PlayOneShot(*tone.sound, Flat());
		REQUIRE(first);
		MixFor(engine, 0.02f);
		const float one = Loudness(MixFor(engine, 0.05f));
		CHECK(one > 0.03f);
		MixFor(engine, 0.2f);
		engine.Update(0.0f);
		CHECK(engine.GetVoiceCount() == 0);

		// Two at once, started together: twice as loud
		engine.PlayOneShot(*tone.sound, Flat());
		engine.PlayOneShot(*tone.sound, Flat());
		CHECK(engine.GetVoiceCount() == 2);
		MixFor(engine, 0.02f);
		CHECK(Loudness(MixFor(engine, 0.05f)) == Near(one * 2.0f, 0.05));
		MixFor(engine, 0.2f);
		CHECK(engine.GetVoiceCount() == 0);

		// On a bus, at its volume
		engine.SetVolume("Interface", 0.5f);
		Settle(engine);
		auto onBus = Flat();
		onBus.bus = "Interface";
		engine.PlayOneShot(*tone.sound, onBus);
		MixFor(engine, 0.02f);
		CHECK(Loudness(MixFor(engine, 0.05f)) == Near(one * 0.5f, 0.05));
		MixFor(engine, 0.2f);

		// A source's, beside its own sound; pausing and stopping the source reach them
		TestSound loop(Sine(220.0f, 0.25f, 0.2f));
		AudioSource source(engine);
		source.SetLooped(true);
		source.Play(*loop.sound);
		source.PlayOneShot(*tone.sound);
		source.PlayOneShot(*tone.sound, 0.5f);
		CHECK(engine.GetVoiceCount() == 3);
		source.Pause();
		CHECK(Loudness(MixFor(engine, 0.1f)) < 1e-4f);
		CHECK(engine.GetVoiceCount() == 3);
		source.Resume();
		CHECK(Loudness(MixFor(engine, 0.05f)) > 0.03f);
		source.Stop();
		CHECK(engine.GetVoiceCount() == 0);

		// A new sound of the source leaves the ones played once alone
		source.PlayOneShot(*tone.sound);
		source.Play(*loop.sound);
		CHECK(engine.GetVoiceCount() == 2);
		source.Stop();

		// The ones played without a source stop all together
		engine.PlayOneShot(*tone.sound, Flat());
		engine.PlayOneShot(*tone.sound, Flat());
		engine.StopOneShots();
		CHECK(engine.GetVoiceCount() == 0);
	}

	TEST_CASE("a suspension pauses the sounds played once, and resumes them")
	{
		BusEngine null;
		auto& engine = null.engine;
		TestSound tone(Sine(440.0f, 0.3f, 0.2f));

		engine.PlayOneShot(*tone.sound, Flat());
		AudioSource source(engine);
		source.PlayOneShot(*tone.sound);

		engine.Suspend();
		CHECK(Loudness(MixFor(engine, 0.5f)) < 1e-4f);
		CHECK(engine.GetVoiceCount() == 2);
		engine.Unsuspend();
		CHECK(Loudness(MixFor(engine, 0.05f)) > 0.03f);
		MixFor(engine, 0.3f);
		CHECK(engine.GetVoiceCount() == 0);
	}

	TEST_CASE("a sound played once in the world: from its place, following its source, ended if too far to be heard")
	{
		BusEngine null;
		auto& engine = null.engine;
		TestSound tone(Sine(440.0f, 0.5f, 0.2f));

		// No listener: at the origin, looking along +Z, so +X is on the left
		OneShotSettings left;
		left.position = OvMaths::FVector3{ 5.0f, 0.0f, 0.0f };
		REQUIRE(engine.PlayOneShot(*tone.sound, left));
		const Mixed heard = MixFor(engine, 0.05f);
		CHECK(heard.left > heard.right * 2.0f);
		engine.StopOneShots();

		// Too far to be heard (past the maximum distance of a linear fade): it ends at once
		OneShotSettings tooFar;
		tooFar.position = OvMaths::FVector3{ 0.0f, 0.0f, 200.0f };
		tooFar.attenuation.model = EAttenuationModel::LINEAR_DISTANCE;
		tooFar.attenuation.maxDistance = 100.0f;
		const auto instance = engine.PlayOneShot(*tone.sound, tooFar);
		CHECK((!instance || !instance->IsValid()));
		MixFor(engine, 0.05f);
		CHECK(engine.GetVoiceCount() == 0);

		// A source's follows it
		OvMaths::FTransform transform;
		transform.SetLocalPosition({ 5.0f, 0.0f, 0.0f });
		AudioSource source(engine, transform);
		source.SetSpatial(true);
		source.PlayOneShot(*tone.sound);
		Settle(engine);
		const Mixed before = MixFor(engine, 0.05f);
		CHECK(before.left > before.right * 2.0f);
		transform.SetLocalPosition({ -5.0f, 0.0f, 0.0f });
		Settle(engine);
		const Mixed after = MixFor(engine, 0.05f);
		CHECK(after.right > after.left * 2.0f);
	}

	TEST_CASE("the pitch varies within its bounds, the same way for the same seed")
	{
		BusEngine null;
		auto& engine = null.engine;

		engine.SetRandomSeed(7);
		std::vector<float> draws;
		for (int draw = 0; draw < 200; ++draw)
		{
			draws.push_back(engine.VaryPitch(1.0f, 0.2f));
		}
		CHECK(std::ranges::all_of(draws, [](float p_pitch) { return p_pitch >= 0.8f && p_pitch <= 1.2f; }));
		CHECK(std::ranges::min(draws) < 0.85f);
		CHECK(std::ranges::max(draws) > 1.15f);

		engine.SetRandomSeed(7);
		for (const float pitch : draws)
		{
			CHECK(engine.VaryPitch(1.0f, 0.2f) == pitch);
		}

		CHECK(engine.VaryPitch(1.5f, 0.0f) == 1.5f);
		CHECK(engine.VaryPitch(0.05f, 1.0f) >= AudioEngine::kMinPitch);
		CHECK(engine.VaryPitch(1.0f, -0.2f) >= 0.8f); // A negative variation counts as its size

		// Each sound a source plays, and each one played once, draws its own
		TestSound tone(Sine(440.0f, 0.25f, 0.2f));
		AudioSource source(engine);
		source.SetPitch(1.0f);
		source.SetPitchVariation(0.3f);
		CHECK(source.GetPitchVariation() == doctest::Approx(0.3f));

		std::vector<float> speeds;
		for (int play = 0; play < 20; ++play)
		{
			source.Play(*tone.sound);
			speeds.push_back(SpeedOf(engine, *source.GetSoundInstance().lock()));
		}
		CHECK(std::ranges::all_of(speeds, [](float p_speed) { return p_speed >= 0.7f && p_speed <= 1.3f; }));
		CHECK(std::ranges::min(speeds) != std::ranges::max(speeds));

		auto varied = Flat();
		varied.pitch = 2.0f;
		varied.pitchVariation = 0.1f;
		for (int play = 0; play < 10; ++play)
		{
			const auto instance = engine.PlayOneShot(*tone.sound, varied);
			const float speed = SpeedOf(engine, *instance);
			CHECK(speed >= 1.9f);
			CHECK(speed <= 2.1f);
		}
	}

	TEST_CASE("each attenuation model follows its formula")
	{
		const auto settings = [](EAttenuationModel p_model, float p_rolloff)
		{
			AttenuationSettings result;
			result.model = p_model;
			result.minDistance = 2.0f;
			result.maxDistance = 40.0f;
			result.rolloff = p_rolloff;
			return result;
		};

		for (const float distance : { 0.5f, 2.0f, 5.0f, 10.0f, 39.0f, 40.0f, 400.0f })
		{
			const float d = std::clamp(distance, 2.0f, 40.0f);
			CHECK(settings(EAttenuationModel::NONE, 1.0f).Evaluate(distance) == 1.0f);
			CHECK(settings(EAttenuationModel::INVERSE_DISTANCE, 1.5f).Evaluate(distance) == doctest::Approx(2.0f / (2.0f + 1.5f * (d - 2.0f))));
			CHECK(settings(EAttenuationModel::LINEAR_DISTANCE, 1.0f).Evaluate(distance) == doctest::Approx(1.0f - (d - 2.0f) / 38.0f));
			CHECK(settings(EAttenuationModel::EXPONENTIAL_DISTANCE, 2.0f).Evaluate(distance) == doctest::Approx(std::pow(d / 2.0f, -2.0f)));
		}

		// Beyond its maximum distance, the linear model is silent; the others hold the volume they reached
		CHECK(settings(EAttenuationModel::LINEAR_DISTANCE, 1.0f).Evaluate(100.0f) == doctest::Approx(0.0f));
		CHECK(settings(EAttenuationModel::EXPONENTIAL_DISTANCE, 1.0f).Evaluate(100.0f) == doctest::Approx(0.05f));

		// Brought into the range the audio engine takes
		AttenuationSettings odd;
		odd.model = EAttenuationModel::LINEAR_DISTANCE;
		odd.minDistance = 0.0f;
		odd.maxDistance = -5.0f;
		odd.rolloff = 3.0f;
		const auto sane = odd.Sanitized();
		CHECK(sane.minDistance == AttenuationSettings::kSmallestDistance);
		CHECK(sane.maxDistance > sane.minDistance);
		CHECK(sane.rolloff == 1.0f); // The linear model would turn negative past its silence
		odd.model = EAttenuationModel::INVERSE_DISTANCE;
		odd.rolloff = -1.0f;
		CHECK(odd.Sanitized().rolloff == 0.0f);
		odd.minDistance = std::nanf("");
		CHECK(odd.Sanitized().minDistance == 1.0f);
	}

	TEST_CASE("a sound in the world fades as its source's model says")
	{
		NullEngine null;
		auto& engine = null.engine;
		TestSound tone(Sine(440.0f, 0.25f, 0.25f));

		OvMaths::FTransform transform;
		AudioSource source(engine, transform);
		source.SetSpatial(true);
		source.SetLooped(true);
		source.SetAttenuationThreshold(2.0f);
		source.SetMaxDistance(30.0f);
		transform.SetLocalPosition({ 0.0f, 0.0f, 2.0f });
		source.Play(*tone.sound);

		const float full = LoudnessAt(engine, transform, 2.0f);
		CHECK(full > 0.05f);

		for (const auto [model, rolloff] : { std::pair{ EAttenuationModel::INVERSE_DISTANCE, 1.0f }, std::pair{ EAttenuationModel::LINEAR_DISTANCE, 1.0f }, std::pair{ EAttenuationModel::EXPONENTIAL_DISTANCE, 2.0f } })
		{
			CAPTURE(static_cast<int>(model));
			source.SetAttenuationModel(model);
			source.SetRolloff(rolloff);
			CHECK(source.GetAttenuationModel() == model);

			for (const float distance : { 4.0f, 8.0f, 12.0f })
			{
				CAPTURE(distance);
				const float expected = source.GetAttenuation().Evaluate(distance);
				CHECK(LoudnessAt(engine, transform, distance) == Near(full * expected, 0.03));
			}
		}

		// The linear model is silent at its maximum distance and beyond
		source.SetAttenuationModel(EAttenuationModel::LINEAR_DISTANCE);
		source.SetRolloff(1.0f);
		CHECK(LoudnessAt(engine, transform, 30.0f) < 1e-4f);
		CHECK(LoudnessAt(engine, transform, 60.0f) < 1e-4f);

		// Without a model, it does not fade
		source.SetAttenuationModel(EAttenuationModel::NONE);
		CHECK(LoudnessAt(engine, transform, 25.0f) == Near(full, 0.03));
	}

	TEST_CASE("an audio source's bus, pitch variation and attenuation go through a file and back")
	{
		NullEngine null;
		null.engine.SetBuses({ "Music", "Effects" });
		OvCore::Global::ServiceLocator::Provide(null.engine);

		OvCore::SceneSystem::Scene scene("", "");
		auto& source = scene.CreateActor("Source").AddComponent<OvCore::ECS::Components::CAudioSource>();
		auto& other = scene.CreateActor("Other").AddComponent<OvCore::ECS::Components::CAudioSource>();

		// The defaults: no bus, no variation, exponential from 1 to 500 metres
		CHECK(source.GetBus().empty());
		CHECK(source.GetPitchVariation() == 0.0f);
		CHECK(source.GetAttenuationModel() == EAttenuationModel::EXPONENTIAL_DISTANCE);
		CHECK(source.GetAttenuationThreshold() == 1.0f);
		CHECK(source.GetMaxDistance() == 500.0f);
		CHECK(source.GetRolloff() == 1.0f);

		source.SetBus("Effects");
		source.SetPitchVariation(0.15f);
		source.SetAttenuationModel(EAttenuationModel::LINEAR_DISTANCE);
		source.SetAttenuationThreshold(3.0f);
		source.SetMaxDistance(80.0f);
		source.SetRolloff(0.5f);

		tinyxml2::XMLDocument document;
		auto* node = document.NewElement("data");
		document.InsertFirstChild(node);
		source.OnSerialize(document, node);
		other.OnDeserialize(document, node);

		CHECK(other.GetBus() == "Effects");
		CHECK(other.GetPitchVariation() == doctest::Approx(0.15f));
		CHECK(other.GetAttenuationModel() == EAttenuationModel::LINEAR_DISTANCE);
		CHECK(other.GetAttenuationThreshold() == doctest::Approx(3.0f));
		CHECK(other.GetMaxDistance() == doctest::Approx(80.0f));
		CHECK(other.GetRolloff() == doctest::Approx(0.5f));

		// A file made before them keeps the defaults
		tinyxml2::XMLDocument older;
		auto* olderNode = older.NewElement("data");
		older.InsertFirstChild(olderNode);
		for (const char* key : { "bus", "pitch_variation", "attenuation_model", "rolloff", "max_distance" })
		{
			node->DeleteChild(node->FirstChildElement(key));
		}
		auto& third = scene.CreateActor("Third").AddComponent<OvCore::ECS::Components::CAudioSource>();
		third.OnDeserialize(document, node);
		CHECK(third.GetBus().empty());
		CHECK(third.GetAttenuationModel() == EAttenuationModel::EXPONENTIAL_DISTANCE);
		CHECK(third.GetMaxDistance() == 500.0f);
		CHECK(third.GetRolloff() == 1.0f);
	}

	TEST_CASE("a project's buses are read from its file")
	{
		const auto read = [](std::string_view p_lines)
		{
			OvTests::TempFolder folder;
			OvTools::Filesystem::IniFile ini(folder.WriteFile("Test.ovproject", p_lines).string());
			return OvCore::Helpers::ProjectSettings::ReadAudioBuses(ini);
		};

		CHECK(read("executable_name=Game\n") == std::vector<std::string>{ "Music", "Effects", "Interface", "Ambience" });
		CHECK(read("audio_buses=\n").empty());
		CHECK(read("audio_buses= Voices , Music,,Sfx \n") == std::vector<std::string>{ "Voices", "Music", "Sfx" });

		OvTests::LogCapture logs;
		CHECK(read("audio_buses=Music,Master,Music,Sfx\n") == std::vector<std::string>{ "Music", "Sfx" });
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_WARNING, "\"Master\" is the volume of every sound"));
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_WARNING, "\"Music\" is named twice"));

		CHECK(read("audio_buses=A,B,C,D,E,F,G,H,I\n").size() == AudioEngine::kMaxBuses);
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_WARNING, "only the first 8 buses are used"));
	}
}
