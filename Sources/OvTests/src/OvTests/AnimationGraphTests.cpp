/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cmath>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <OvCore/Animation/AnimationGraph.h>

using OvCore::Animation::AnimationGraph;
using OvCore::Animation::EGraphParameterType;
using OvCore::Animation::GraphDefinition;

namespace
{
	// A locomotion layer (a blend along the speed, a fall and a landing), and an upper body layer swung by a trigger
	constexpr const char* kGraph = R"({
		"parameters": [
			{ "name": "Speed", "type": "float", "default": 0 },
			{ "name": "Grounded", "type": "bool", "default": true },
			{ "name": "Attack", "type": "trigger" }
		],
		"layers": [
			{
				"name": "Base",
				"states": [
					{ "name": "Move", "blend": { "parameter": "Speed", "motions": [
						{ "clip": "Run", "position": 6 },
						{ "clip": "Idle", "position": 0 },
						{ "clip": "Walk", "position": 2 }
					] } },
					{ "name": "Fall", "clip": "Fall" },
					{ "name": "Land", "clip": "Land", "loop": false }
				],
				"transitions": [
					{ "from": "Move", "to": "Fall", "conditions": [ { "parameter": "Grounded", "value": false } ], "duration": 0.2 },
					{ "from": "Fall", "to": "Land", "conditions": [ { "parameter": "Grounded" } ], "duration": 0.1 },
					{ "from": "Land", "to": "Move", "exitTime": 0.9, "duration": 0.2 }
				]
			},
			{
				"name": "Upper",
				"weight": 0.8,
				"mask": "spine",
				"states": [
					{ "name": "Empty", "clip": "Idle" },
					{ "name": "Swing", "clip": "Swing", "loop": false }
				],
				"transitions": [
					{ "from": "*", "to": "Swing", "conditions": [ { "parameter": "Attack" } ], "duration": 0.1 },
					{ "from": "Swing", "to": "Empty", "exitTime": 1, "duration": 0.1 }
				]
			}
		]
	})";

	// Seconds, in the order the file names the clips: Run, Idle, Walk, Fall, Land, Swing
	const std::vector<float> kDurations = { 0.5f, 2.0f, 1.0f, 1.0f, 0.5f, 0.8f };

	GraphDefinition Definition(const char* p_text = kGraph)
	{
		GraphDefinition definition;
		const auto error = GraphDefinition::Parse(p_text, definition);
		REQUIRE_MESSAGE(!error, error.value_or(""));
		return definition;
	}

	AnimationGraph Graph()
	{
		return AnimationGraph(Definition(), kDurations);
	}

	size_t Clip(const AnimationGraph& p_graph, const std::string& p_name)
	{
		const auto& clips = p_graph.GetDefinition().clips;
		for (size_t i = 0; i < clips.size(); ++i)
		{
			if (clips[i] == p_name)
			{
				return i;
			}
		}
		FAIL("no clip " << p_name);
		return 0;
	}

	float Weight(const AnimationGraph& p_graph, size_t p_layer, const std::string& p_clip)
	{
		float weight = 0.0f;
		for (const auto& sample : p_graph.GetSamples())
		{
			if (sample.layer == p_layer && sample.clip == Clip(p_graph, p_clip))
			{
				weight += sample.weight;
			}
		}
		return weight;
	}

	float LayerTotal(const AnimationGraph& p_graph, size_t p_layer)
	{
		float total = 0.0f;
		for (const auto& sample : p_graph.GetSamples())
		{
			if (sample.layer == p_layer)
			{
				total += sample.weight;
			}
		}
		return total;
	}

	std::string StateName(const AnimationGraph& p_graph, size_t p_layer)
	{
		return p_graph.GetDefinition().layers[p_layer].states[p_graph.GetState(p_layer)].name;
	}

	size_t Parameter(const AnimationGraph& p_graph, const std::string& p_name)
	{
		return p_graph.GetDefinition().FindParameter(p_name).value();
	}

	void Run(AnimationGraph& p_graph, float p_seconds, float p_step = 1.0f / 60.0f)
	{
		for (float t = 0.0f; t < p_seconds - 1e-6f; t += p_step)
		{
			p_graph.Update(p_step);
		}
	}

	std::string Refusal(const std::string& p_text)
	{
		GraphDefinition definition;
		const auto error = GraphDefinition::Parse(p_text, definition);
		CHECK(definition.layers.empty());
		return error.value_or("(accepted)");
	}

	// A one-layer graph around the given states and transitions, with a float F, a bool B and a trigger T
	std::string Layer(const std::string& p_states, const std::string& p_transitions = "[]", const std::string& p_extra = "")
	{
		return R"({ "parameters": [ { "name": "F", "type": "float" }, { "name": "B", "type": "bool" }, { "name": "T", "type": "trigger" } ],
			"layers": [ { "name": "L", )" + p_extra + R"("states": )" + p_states + R"(, "transitions": )" + p_transitions + " } ] }";
	}
}

TEST_SUITE("Animation graph")
{
	TEST_CASE("a graph reads, each clip named once, a blend's clips in order along its parameter")
	{
		const auto definition = Definition();

		REQUIRE(definition.parameters.size() == 3);
		CHECK(definition.parameters[1].type == EGraphParameterType::BOOL);
		CHECK(definition.parameters[1].defaultValue == 1.0f);
		CHECK(definition.parameters[2].type == EGraphParameterType::TRIGGER);

		CHECK(definition.clips == std::vector<std::string>{ "Run", "Idle", "Walk", "Fall", "Land", "Swing" });
		CHECK(definition.clipPaths[0] == "layers[0].states[0].blend.motions[0].clip");

		REQUIRE(definition.layers.size() == 2);
		const auto& move = definition.layers[0].states[0];
		REQUIRE(move.motions.size() == 3);
		CHECK(move.motions[0].position == 0.0f);
		CHECK(move.motions[2].position == 6.0f);
		CHECK(definition.layers[0].states[2].loop == false);
		CHECK(definition.layers[1].weight == doctest::Approx(0.8f));
		CHECK(definition.layers[1].mask == "spine");
		CHECK_FALSE(definition.layers[1].transitions[0].from.has_value());
	}

	TEST_CASE("a mistake is refused, saying where it is")
	{
		CHECK(Refusal(R"({ "layers": [] })") == "layers: must not be empty");
		CHECK(Refusal(R"({ "layers": [ { "name": "L", "states": [ { "name": "S", "clip": "C" } ] } ], "extra": 1 })").starts_with("extra: unknown member"));
		CHECK(Refusal(R"({ "parameters": [ { "name": "P", "type": "integer" } ], "layers": [] })") == R"(parameters[0].type: must be "float", "bool" or "trigger")");
		CHECK(Refusal(Layer(R"([ { "name": "S", "clip": "C" } ])", R"([ { "from": "S", "to": "S", "conditions": [ { "parameter": "Nope" } ] } ])"))
			== "layers[0].transitions[0].conditions[0].parameter: unknown parameter 'Nope'");
		CHECK(Refusal(Layer(R"([ { "name": "S", "clip": "C" } ])", R"([ { "from": "S", "to": "Nowhere", "exitTime": 1 } ])"))
			== "layers[0].transitions[0].to: no state 'Nowhere' in this layer");
		CHECK(Refusal(Layer(R"([ { "name": "S", "clip": "C", "blend": { "parameter": "F", "motions": [] } } ])"))
			== "layers[0].states[0]: a state plays a clip or a blend, not both");
		CHECK(Refusal(Layer(R"([ { "name": "S", "clip": "C" } ])", R"([ { "from": "*", "to": "S", "conditions": [ { "parameter": "T", "op": "==" } ] } ])")).starts_with("layers[0].transitions[0].conditions[0]: 'T' is a trigger"));
		CHECK(Refusal(Layer(R"([ { "name": "S", "clip": "C" } ])", R"([ { "from": "*", "to": "S", "conditions": [ { "parameter": "F", "value": 1 } ] } ])"))
			== "layers[0].transitions[0].conditions[0].op: is missing");
		CHECK(Refusal(Layer(R"([ { "name": "S", "clip": "C" } ])", R"([ { "from": "*", "to": "S", "conditions": [ { "parameter": "B", "op": ">" } ] } ])"))
			== R"(layers[0].transitions[0].conditions[0].op: a boolean is compared with "==" or "!=")");
		CHECK(Refusal(Layer(R"([ { "name": "S", "clip": "C" }, { "name": "S", "clip": "D" } ])"))
			== "layers[0].states[1].name: a state 'S' is already in this layer");
		CHECK(Refusal(Layer(R"([ { "name": "S", "clip": "C" } ])", R"([ { "from": "S", "to": "S" } ])")).starts_with("layers[0].transitions[0]: a transition needs conditions or an exit time"));
		CHECK(Refusal(Layer(R"([ { "name": "S", "blend": { "parameter": "B", "motions": [ { "clip": "C", "position": 0 } ] } } ])"))
			== "layers[0].states[0].blend.parameter: parameter 'B' must be a number");
		CHECK(Refusal(Layer(R"([ { "name": "S", "clip": "C" } ])", "[]", R"("default": "Elsewhere", )")) == "layers[0].default: no state 'Elsewhere' in this layer");
		CHECK(Refusal(Layer(R"([ { "name": "S", "clip": "C", "speed": "fast" } ])")) == "layers[0].states[0].speed: must be a number from -100 to 100");

		// Not even JSON: the reader's own words, with the line
		const auto error = Refusal("{ \"layers\": [ \n oops ] }");
		CHECK(error.find("line 2") != std::string::npos);
	}

	TEST_CASE("transitions follow the parameters, and an exit time waits for the state to play that far")
	{
		auto graph = Graph();
		const size_t grounded = Parameter(graph, "Grounded");
		CHECK(StateName(graph, 0) == "Move");

		Run(graph, 0.5f);
		CHECK(StateName(graph, 0) == "Move");

		graph.SetValue(grounded, 0.0f);
		graph.Update(1.0f / 60.0f);
		CHECK(StateName(graph, 0) == "Fall");
		CHECK(graph.IsInTransition(0));

		Run(graph, 0.5f);
		CHECK(StateName(graph, 0) == "Fall");
		CHECK_FALSE(graph.IsInTransition(0));

		graph.SetValue(grounded, 1.0f);
		graph.Update(1.0f / 60.0f);
		CHECK(StateName(graph, 0) == "Land");

		// The landing lasts 0.5 s and goes back once 90% through: still landing at 0.4 s, moving again by 0.5 s
		Run(graph, 0.4f);
		CHECK(StateName(graph, 0) == "Land");
		Run(graph, 0.1f);
		CHECK(StateName(graph, 0) == "Move");
	}

	TEST_CASE("a trigger is used up by the transition it starts; any state goes to its target, not to itself")
	{
		auto graph = Graph();
		const size_t attack = Parameter(graph, "Attack");

		CHECK(graph.SetTrigger(attack));
		graph.Update(1.0f / 60.0f);
		CHECK(StateName(graph, 1) == "Swing");
		CHECK(graph.GetValue(attack) == 0.0f);

		// Set again mid swing: the swing goes on, the trigger waits
		Run(graph, 0.2f);
		const float time = graph.GetStateTime(1);
		graph.SetTrigger(attack);
		graph.Update(1.0f / 60.0f);
		CHECK(StateName(graph, 1) == "Swing");
		CHECK(graph.GetStateTime(1) > time);
		CHECK(graph.GetValue(attack) == 1.0f);

		// Back to rest once the swing is over, then at once into a new swing for the trigger still set
		graph.SetTrigger(attack, false);
		Run(graph, 0.7f);
		CHECK(StateName(graph, 1) == "Empty");
		graph.SetTrigger(attack);
		graph.Update(1.0f / 60.0f);
		CHECK(StateName(graph, 1) == "Swing");
		CHECK(graph.GetStateTime(1) == doctest::Approx(0.0f));

		// A number or a boolean is no trigger, and a trigger takes no value
		CHECK_FALSE(graph.SetTrigger(Parameter(graph, "Speed")));
		CHECK_FALSE(graph.SetValue(attack, 1.0f));
	}

	TEST_CASE("a fade's weights add up to one, the target's growing as it goes")
	{
		auto graph = Graph();
		graph.SetValue(Parameter(graph, "Grounded"), 0.0f);

		// The transition is taken on the first update, and its 0.2 s fade runs from the next: twelve frames at 60 Hz
		float previous = 0.0f;
		bool sums = true;
		bool grows = true;
		for (int frame = 0; frame <= 12; ++frame)
		{
			graph.Update(1.0f / 60.0f);
			sums = sums && std::abs(LayerTotal(graph, 0) - 1.0f) < 1e-5f && std::abs(LayerTotal(graph, 1) - 1.0f) < 1e-5f;
			const float fall = Weight(graph, 0, "Fall");
			grows = grows && (frame == 0 ? fall == 0.0f : fall > previous);
			if (frame == 6)
			{
				CHECK(fall == doctest::Approx(0.5f).epsilon(1e-3));
			}
			previous = fall;
		}

		CHECK(sums);
		CHECK(grows);
		CHECK(previous == doctest::Approx(1.0f));
		CHECK_FALSE(graph.IsInTransition(0));
	}

	TEST_CASE("a fade cut short starts from the blend it had reached")
	{
		auto graph = Graph();
		graph.SetValue(Parameter(graph, "Grounded"), 0.0f);
		graph.Update(1.0f / 60.0f);
		Run(graph, 0.08f);

		const float fall = Weight(graph, 0, "Fall");
		const float idle = Weight(graph, 0, "Idle");
		REQUIRE(fall > 0.2f);
		REQUIRE(fall < 0.8f);

		// Into the landing while halfway into the fall
		CHECK(graph.Play(0, 2, 0.2f));
		graph.Update(0.0001f);

		CHECK(Weight(graph, 0, "Fall") == doctest::Approx(fall).epsilon(0.01));
		CHECK(Weight(graph, 0, "Idle") == doctest::Approx(idle).epsilon(0.01));
		CHECK(Weight(graph, 0, "Land") < 0.01f);
		CHECK(LayerTotal(graph, 0) == doctest::Approx(1.0f));

		Run(graph, 0.25f);
		CHECK(Weight(graph, 0, "Land") == doctest::Approx(1.0f));
	}

	TEST_CASE("a blend at its edges and in between, its clips kept in step")
	{
		auto graph = Graph();
		const size_t speed = Parameter(graph, "Speed");

		graph.Update(0.0f);
		CHECK(Weight(graph, 0, "Idle") == doctest::Approx(1.0f));

		graph.SetValue(speed, -3.0f);
		graph.Update(0.0f);
		CHECK(Weight(graph, 0, "Idle") == doctest::Approx(1.0f));

		graph.SetValue(speed, 10.0f);
		graph.Update(0.0f);
		CHECK(Weight(graph, 0, "Run") == doctest::Approx(1.0f));

		graph.SetValue(speed, 4.0f);
		graph.Update(0.0f);
		CHECK(Weight(graph, 0, "Walk") == doctest::Approx(0.5f));
		CHECK(Weight(graph, 0, "Run") == doctest::Approx(0.5f));
		CHECK(Weight(graph, 0, "Idle") == doctest::Approx(0.0f));

		// Halfway between a 1 s walk and a 0.5 s run, a cycle lasts 0.75 s: a quarter of a second is a third of it,
		// for both clips
		graph.Update(0.25f);
		float walk = -1.0f;
		float run = -1.0f;
		for (const auto& sample : graph.GetSamples())
		{
			if (sample.layer == 0 && sample.clip == Clip(graph, "Walk")) walk = sample.time / 1.0f;
			if (sample.layer == 0 && sample.clip == Clip(graph, "Run")) run = sample.time / 0.5f;
		}
		CHECK(walk == doctest::Approx(1.0f / 3.0f).epsilon(1e-4));
		CHECK(run == doctest::Approx(1.0f / 3.0f).epsilon(1e-4));
	}

	TEST_CASE("an empty state plays nothing, and a fade to it leaves its layer as it goes")
	{
		GraphDefinition definition;
		const auto error = GraphDefinition::Parse(Layer(R"([ { "name": "Rest" }, { "name": "Wave", "clip": "Wave", "loop": false } ])",
			R"([ { "from": "*", "to": "Wave", "conditions": [ { "parameter": "T" } ] }, { "from": "Wave", "to": "Rest", "exitTime": 1, "duration": 0.5 } ])"), definition);
		REQUIRE_MESSAGE(!error, error.value_or(""));

		AnimationGraph graph(std::move(definition), { 1.0f });
		graph.Update(0.1f);
		CHECK(graph.GetSamples().empty());

		graph.SetTrigger(2);
		graph.Update(0.0f);
		Run(graph, 0.25f);
		CHECK(LayerTotal(graph, 0) == doctest::Approx(1.0f));

		// The wave over, its weight goes down to nothing over half a second
		Run(graph, 1.0f);
		CHECK(graph.IsInTransition(0));
		CHECK(LayerTotal(graph, 0) > 0.0f);
		CHECK(LayerTotal(graph, 0) < 1.0f);
		Run(graph, 0.5f);
		CHECK(graph.GetSamples().empty());
	}

	TEST_CASE("each layer samples on its own, and Play and Reset move the states at once")
	{
		auto graph = Graph();

		graph.Update(1.0f / 60.0f);
		for (const auto& sample : graph.GetSamples())
		{
			CHECK((sample.layer == 0 || sample.layer == 1));
		}
		CHECK(Weight(graph, 1, "Idle") == doctest::Approx(1.0f));

		CHECK(graph.Play(1, 1, 0.0f));
		CHECK(StateName(graph, 1) == "Swing");
		CHECK_FALSE(graph.IsInTransition(1));
		CHECK_FALSE(graph.Play(1, 9, 0.0f));
		CHECK_FALSE(graph.Play(5, 0, 0.0f));

		graph.SetValue(Parameter(graph, "Speed"), 3.0f);
		graph.SetValue(Parameter(graph, "Grounded"), 0.0f);
		graph.Reset();
		CHECK(StateName(graph, 1) == "Empty");
		CHECK(graph.GetValue(Parameter(graph, "Speed")) == 0.0f);
		CHECK(graph.GetValue(Parameter(graph, "Grounded")) == 1.0f);
		CHECK(Weight(graph, 0, "Idle") == doctest::Approx(1.0f));
	}
}
