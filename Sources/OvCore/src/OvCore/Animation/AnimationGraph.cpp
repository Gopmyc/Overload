/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <format>
#include <unordered_map>

#include <OvCore/Animation/AnimationGraph.h>
#include <OvTools/Utils/Json.h>

namespace
{
	using namespace OvCore::Animation;
	using OvTools::Utils::Json::Array;
	using OvTools::Utils::Json::Object;
	using OvTools::Utils::Json::Value;

	constexpr float kMinDuration = 0.0001f;
	constexpr float kMinWeight = 0.001f;
	constexpr size_t kMaxFadingStates = 8;

	float Fraction(float p_value)
	{
		return p_value - std::floor(p_value);
	}

	// Reads the members of a JSON file into a definition, stopping at the first mistake and saying where it is
	class Reader
	{
	public:
		explicit Reader(GraphDefinition& p_definition) : m_definition(p_definition) {}

		std::optional<std::string> error;

		void Fail(const std::string& p_path, const std::string& p_message)
		{
			if (!error)
			{
				error = p_path.empty() ? p_message : p_path + ": " + p_message;
			}
		}

		// An object whose members are all among p_allowed
		const Object* ObjectAt(const Value& p_value, const std::string& p_path, std::initializer_list<std::string_view> p_allowed)
		{
			const Object* object = p_value.AsObject();
			if (!object)
			{
				Fail(p_path, "must be an object");
				return nullptr;
			}

			for (const auto& member : *object)
			{
				if (std::find(p_allowed.begin(), p_allowed.end(), member.name) == p_allowed.end())
				{
					std::string allowed;
					for (const auto name : p_allowed)
					{
						allowed += (allowed.empty() ? "" : ", ") + std::string{ name };
					}
					Fail(Join(p_path, member.name), std::format("unknown member (expected {})", allowed));
					return nullptr;
				}
			}

			return object;
		}

		static std::string Join(const std::string& p_path, std::string_view p_member)
		{
			return p_path.empty() ? std::string{ p_member } : p_path + "." + std::string{ p_member };
		}

		static std::string Index(const std::string& p_path, size_t p_index)
		{
			return std::format("{}[{}]", p_path, p_index);
		}

		const Value* Find(const Object& p_object, std::string_view p_name)
		{
			for (const auto& member : p_object)
			{
				if (member.name == p_name)
				{
					return &member.value;
				}
			}
			return nullptr;
		}

		std::optional<std::string> String(const Object& p_object, const std::string& p_path, std::string_view p_name, bool p_required)
		{
			const Value* value = Find(p_object, p_name);
			if (!value)
			{
				if (p_required)
				{
					Fail(Join(p_path, p_name), "is missing");
				}
				return std::nullopt;
			}

			const std::string* text = value->AsString();
			if (!text || text->empty())
			{
				Fail(Join(p_path, p_name), "must be a name");
				return std::nullopt;
			}

			return *text;
		}

		std::optional<float> Number(const Object& p_object, const std::string& p_path, std::string_view p_name, float p_min, float p_max)
		{
			const Value* value = Find(p_object, p_name);
			if (!value)
			{
				return std::nullopt;
			}

			const double* number = value->AsNumber();
			if (!number || *number < p_min || *number > p_max)
			{
				Fail(Join(p_path, p_name), std::format("must be a number from {:g} to {:g}", p_min, p_max));
				return std::nullopt;
			}

			return static_cast<float>(*number);
		}

		std::optional<bool> Bool(const Object& p_object, const std::string& p_path, std::string_view p_name)
		{
			const Value* value = Find(p_object, p_name);
			if (!value)
			{
				return std::nullopt;
			}

			const bool* boolean = value->AsBool();
			if (!boolean)
			{
				Fail(Join(p_path, p_name), "must be true or false");
				return std::nullopt;
			}

			return *boolean;
		}

		const Array* ArrayAt(const Object& p_object, const std::string& p_path, std::string_view p_name, bool p_required)
		{
			const Value* value = Find(p_object, p_name);
			if (!value)
			{
				if (p_required)
				{
					Fail(Join(p_path, p_name), "is missing");
				}
				return nullptr;
			}

			const Array* array = value->AsArray();
			if (!array)
			{
				Fail(Join(p_path, p_name), "must be a list");
				return nullptr;
			}

			if (p_required && array->empty())
			{
				Fail(Join(p_path, p_name), "must not be empty");
				return nullptr;
			}

			return array;
		}

		size_t Clip(const std::string& p_name, const std::string& p_path)
		{
			const auto found = std::find(m_definition.clips.begin(), m_definition.clips.end(), p_name);
			if (found != m_definition.clips.end())
			{
				return static_cast<size_t>(std::distance(m_definition.clips.begin(), found));
			}

			m_definition.clips.push_back(p_name);
			m_definition.clipPaths.push_back(p_path);
			return m_definition.clips.size() - 1;
		}

		std::optional<size_t> Parameter(const std::string& p_name, const std::string& p_path, std::optional<EGraphParameterType> p_type)
		{
			const auto index = m_definition.FindParameter(p_name);
			if (!index)
			{
				Fail(p_path, std::format("unknown parameter '{}'", p_name));
				return std::nullopt;
			}

			if (p_type && m_definition.parameters[*index].type != *p_type)
			{
				Fail(p_path, std::format("parameter '{}' must be a number", p_name));
				return std::nullopt;
			}

			return index;
		}

		void Parameters(const Array& p_parameters, const std::string& p_path)
		{
			for (size_t i = 0; i < p_parameters.size() && !error; ++i)
			{
				const std::string path = Index(p_path, i);
				const Object* object = ObjectAt(p_parameters[i], path, { "name", "type", "default" });
				if (!object)
				{
					return;
				}

				GraphParameter parameter;
				const auto name = String(*object, path, "name", true);
				const auto type = String(*object, path, "type", true);
				if (!name || !type)
				{
					return;
				}

				if (m_definition.FindParameter(*name))
				{
					Fail(Join(path, "name"), std::format("a parameter '{}' is already declared", *name));
					return;
				}

				parameter.name = *name;

				const Value* defaultValue = Find(*object, "default");

				if (*type == "float")
				{
					parameter.type = EGraphParameterType::FLOAT;
					if (defaultValue)
					{
						const double* number = defaultValue->AsNumber();
						if (!number)
						{
							Fail(Join(path, "default"), "must be a number");
							return;
						}
						parameter.defaultValue = static_cast<float>(*number);
					}
				}
				else if (*type == "bool")
				{
					parameter.type = EGraphParameterType::BOOL;
					if (defaultValue)
					{
						const bool* boolean = defaultValue->AsBool();
						if (!boolean)
						{
							Fail(Join(path, "default"), "must be true or false");
							return;
						}
						parameter.defaultValue = *boolean ? 1.0f : 0.0f;
					}
				}
				else if (*type == "trigger")
				{
					parameter.type = EGraphParameterType::TRIGGER;
					if (defaultValue)
					{
						Fail(Join(path, "default"), "a trigger has no default");
						return;
					}
				}
				else
				{
					Fail(Join(path, "type"), "must be \"float\", \"bool\" or \"trigger\"");
					return;
				}

				m_definition.parameters.push_back(std::move(parameter));
			}
		}

		void State(const Value& p_value, const std::string& p_path, GraphLayer& p_layer)
		{
			const Object* object = ObjectAt(p_value, p_path, { "name", "clip", "blend", "speed", "speedParameter", "loop" });
			if (!object)
			{
				return;
			}

			GraphState state;
			const auto name = String(*object, p_path, "name", true);
			if (!name)
			{
				return;
			}

			if (std::any_of(p_layer.states.begin(), p_layer.states.end(), [&](const GraphState& p_state) { return p_state.name == *name; }))
			{
				Fail(Join(p_path, "name"), std::format("a state '{}' is already in this layer", *name));
				return;
			}

			state.name = *name;

			const Value* clip = Find(*object, "clip");
			const Value* blend = Find(*object, "blend");

			if (clip && blend)
			{
				Fail(p_path, "a state plays a clip or a blend, not both");
				return;
			}

			if (clip)
			{
				const auto clipName = String(*object, p_path, "clip", true);
				if (!clipName)
				{
					return;
				}
				state.motions.push_back({ Clip(*clipName, Join(p_path, "clip")), 0.0f });
			}
			else if (blend)
			{
				const std::string blendPath = Join(p_path, "blend");
				const Object* blendObject = ObjectAt(*blend, blendPath, { "parameter", "motions" });
				if (!blendObject)
				{
					return;
				}

				const auto parameterName = String(*blendObject, blendPath, "parameter", true);
				if (!parameterName)
				{
					return;
				}

				state.blendParameter = Parameter(*parameterName, Join(blendPath, "parameter"), EGraphParameterType::FLOAT);
				if (!state.blendParameter)
				{
					return;
				}

				const Array* motions = ArrayAt(*blendObject, blendPath, "motions", true);
				if (!motions)
				{
					return;
				}

				for (size_t i = 0; i < motions->size(); ++i)
				{
					const std::string motionPath = Index(Join(blendPath, "motions"), i);
					const Object* motion = ObjectAt((*motions)[i], motionPath, { "clip", "position" });
					if (!motion)
					{
						return;
					}

					const auto clipName = String(*motion, motionPath, "clip", true);
					const Value* position = Find(*motion, "position");
					if (!clipName)
					{
						return;
					}

					if (!position || !position->AsNumber())
					{
						Fail(Join(motionPath, "position"), position ? "must be a number" : "is missing");
						return;
					}

					state.motions.push_back({ Clip(*clipName, Join(motionPath, "clip")), static_cast<float>(*position->AsNumber()) });
				}

				std::stable_sort(state.motions.begin(), state.motions.end(), [](const GraphMotion& p_a, const GraphMotion& p_b) { return p_a.position < p_b.position; });
			}
			// Else, an empty state: it plays nothing, and the layers before it show through

			if (const auto speed = Number(*object, p_path, "speed", -100.0f, 100.0f))
			{
				state.speed = *speed;
			}

			if (const auto speedParameter = String(*object, p_path, "speedParameter", false))
			{
				state.speedParameter = Parameter(*speedParameter, Join(p_path, "speedParameter"), EGraphParameterType::FLOAT);
			}

			if (const auto loop = Bool(*object, p_path, "loop"))
			{
				state.loop = *loop;
			}

			if (!error)
			{
				p_layer.states.push_back(std::move(state));
			}
		}

		std::optional<size_t> StateIndex(const GraphLayer& p_layer, const std::string& p_name, const std::string& p_path)
		{
			for (size_t i = 0; i < p_layer.states.size(); ++i)
			{
				if (p_layer.states[i].name == p_name)
				{
					return i;
				}
			}

			Fail(p_path, std::format("no state '{}' in this layer", p_name));
			return std::nullopt;
		}

		void Condition(const Value& p_value, const std::string& p_path, GraphTransition& p_transition)
		{
			const Object* object = ObjectAt(p_value, p_path, { "parameter", "op", "value" });
			if (!object)
			{
				return;
			}

			const auto name = String(*object, p_path, "parameter", true);
			if (!name)
			{
				return;
			}

			const auto index = Parameter(*name, Join(p_path, "parameter"), std::nullopt);
			if (!index)
			{
				return;
			}

			GraphCondition condition;
			condition.parameter = *index;

			const Value* op = Find(*object, "op");
			const Value* value = Find(*object, "value");
			const std::string* opText = op ? op->AsString() : nullptr;

			if (op && !opText)
			{
				Fail(Join(p_path, "op"), "must be a comparison");
				return;
			}

			switch (m_definition.parameters[*index].type)
			{
			case EGraphParameterType::TRIGGER:
				if (op || value)
				{
					Fail(p_path, std::format("'{}' is a trigger: a condition on it takes no \"op\" nor \"value\"", *name));
					return;
				}
				condition.op = EGraphConditionOperator::TRIGGERED;
				break;

			case EGraphParameterType::BOOL:
				if (opText && *opText != "==" && *opText != "!=")
				{
					Fail(Join(p_path, "op"), "a boolean is compared with \"==\" or \"!=\"");
					return;
				}
				if (value && !value->AsBool())
				{
					Fail(Join(p_path, "value"), "must be true or false");
					return;
				}
				condition.op = opText && *opText == "!=" ? EGraphConditionOperator::NOT_EQUAL : EGraphConditionOperator::EQUAL;
				condition.value = !value || *value->AsBool() ? 1.0f : 0.0f;
				break;

			case EGraphParameterType::FLOAT:
				if (!opText)
				{
					Fail(Join(p_path, "op"), "is missing");
					return;
				}
				if (*opText == ">") condition.op = EGraphConditionOperator::GREATER;
				else if (*opText == ">=") condition.op = EGraphConditionOperator::GREATER_EQUAL;
				else if (*opText == "<") condition.op = EGraphConditionOperator::LESS;
				else if (*opText == "<=") condition.op = EGraphConditionOperator::LESS_EQUAL;
				else if (*opText == "==") condition.op = EGraphConditionOperator::EQUAL;
				else if (*opText == "!=") condition.op = EGraphConditionOperator::NOT_EQUAL;
				else
				{
					Fail(Join(p_path, "op"), "must be \">\", \">=\", \"<\", \"<=\", \"==\" or \"!=\"");
					return;
				}
				if (!value || !value->AsNumber())
				{
					Fail(Join(p_path, "value"), value ? "must be a number" : "is missing");
					return;
				}
				condition.value = static_cast<float>(*value->AsNumber());
				break;
			}

			p_transition.conditions.push_back(condition);
		}

		void Transition(const Value& p_value, const std::string& p_path, GraphLayer& p_layer)
		{
			const Object* object = ObjectAt(p_value, p_path, { "from", "to", "conditions", "duration", "exitTime" });
			if (!object)
			{
				return;
			}

			GraphTransition transition;

			const auto from = String(*object, p_path, "from", true);
			const auto to = String(*object, p_path, "to", true);
			if (!from || !to)
			{
				return;
			}

			if (*from != "*")
			{
				transition.from = StateIndex(p_layer, *from, Join(p_path, "from"));
				if (!transition.from)
				{
					return;
				}
			}

			const auto target = StateIndex(p_layer, *to, Join(p_path, "to"));
			if (!target)
			{
				return;
			}
			transition.to = *target;

			if (const Array* conditions = ArrayAt(*object, p_path, "conditions", false))
			{
				for (size_t i = 0; i < conditions->size() && !error; ++i)
				{
					Condition((*conditions)[i], Index(Join(p_path, "conditions"), i), transition);
				}
			}

			if (const auto duration = Number(*object, p_path, "duration", 0.0f, 60.0f))
			{
				transition.duration = *duration;
			}

			transition.exitTime = Number(*object, p_path, "exitTime", 0.0f, 1000.0f);

			if (!transition.exitTime && transition.conditions.empty() && !error)
			{
				Fail(p_path, "a transition needs conditions or an exit time, or it would be taken at once");
				return;
			}

			if (!error)
			{
				p_layer.transitions.push_back(std::move(transition));
			}
		}

		void Layer(const Value& p_value, const std::string& p_path)
		{
			const Object* object = ObjectAt(p_value, p_path, { "name", "weight", "mask", "default", "states", "transitions" });
			if (!object)
			{
				return;
			}

			GraphLayer layer;

			const auto name = String(*object, p_path, "name", true);
			if (!name)
			{
				return;
			}

			if (std::any_of(m_definition.layers.begin(), m_definition.layers.end(), [&](const GraphLayer& p_layer) { return p_layer.name == *name; }))
			{
				Fail(Join(p_path, "name"), std::format("a layer '{}' is already declared", *name));
				return;
			}

			layer.name = *name;

			if (const auto weight = Number(*object, p_path, "weight", 0.0f, 1.0f))
			{
				layer.weight = *weight;
			}

			if (const auto mask = String(*object, p_path, "mask", false))
			{
				layer.mask = *mask;
			}

			const Array* states = ArrayAt(*object, p_path, "states", true);
			if (!states)
			{
				return;
			}

			for (size_t i = 0; i < states->size() && !error; ++i)
			{
				State((*states)[i], Index(Join(p_path, "states"), i), layer);
			}

			if (error)
			{
				return;
			}

			if (const auto defaultState = String(*object, p_path, "default", false))
			{
				const auto index = StateIndex(layer, *defaultState, Join(p_path, "default"));
				if (!index)
				{
					return;
				}
				layer.defaultState = *index;
			}

			if (const Array* transitions = ArrayAt(*object, p_path, "transitions", false))
			{
				for (size_t i = 0; i < transitions->size() && !error; ++i)
				{
					Transition((*transitions)[i], Index(Join(p_path, "transitions"), i), layer);
				}
			}

			if (!error)
			{
				m_definition.layers.push_back(std::move(layer));
			}
		}

	private:
		GraphDefinition& m_definition;
	};
}

std::optional<std::string> OvCore::Animation::GraphDefinition::Parse(std::string_view p_text, GraphDefinition& p_definition)
{
	p_definition = {};

	std::string parseError;
	const auto root = OvTools::Utils::Json::Parse(p_text, parseError, 32);
	if (!root)
	{
		return parseError;
	}

	Reader reader(p_definition);
	const Object* object = reader.ObjectAt(*root, "", { "parameters", "layers" });

	if (object)
	{
		if (const Array* parameters = reader.ArrayAt(*object, "", "parameters", false))
		{
			reader.Parameters(*parameters, "parameters");
		}
	}

	if (object && !reader.error)
	{
		if (const Array* layers = reader.ArrayAt(*object, "", "layers", true))
		{
			for (size_t i = 0; i < layers->size() && !reader.error; ++i)
			{
				reader.Layer((*layers)[i], std::format("layers[{}]", i));
			}
		}
	}

	if (reader.error)
	{
		p_definition = {};
	}

	return reader.error;
}

std::optional<size_t> OvCore::Animation::GraphDefinition::FindParameter(std::string_view p_name) const
{
	for (size_t i = 0; i < parameters.size(); ++i)
	{
		if (parameters[i].name == p_name)
		{
			return i;
		}
	}
	return std::nullopt;
}

OvCore::Animation::AnimationGraph::AnimationGraph(GraphDefinition p_definition, std::vector<float> p_clipDurations) :
	m_definition(std::move(p_definition)),
	m_clipDurations(std::move(p_clipDurations))
{
	m_clipDurations.resize(m_definition.clips.size(), 0.0f);
	Reset();
}

const OvCore::Animation::GraphDefinition& OvCore::Animation::AnimationGraph::GetDefinition() const
{
	return m_definition;
}

bool OvCore::Animation::AnimationGraph::SetValue(size_t p_parameter, float p_value)
{
	if (p_parameter >= m_values.size() || m_definition.parameters[p_parameter].type == EGraphParameterType::TRIGGER)
	{
		return false;
	}

	m_values[p_parameter] = m_definition.parameters[p_parameter].type == EGraphParameterType::BOOL ?
		(p_value != 0.0f ? 1.0f : 0.0f) :
		p_value;
	return true;
}

float OvCore::Animation::AnimationGraph::GetValue(size_t p_parameter) const
{
	return p_parameter < m_values.size() ? m_values[p_parameter] : 0.0f;
}

bool OvCore::Animation::AnimationGraph::SetTrigger(size_t p_parameter, bool p_set)
{
	if (p_parameter >= m_values.size() || m_definition.parameters[p_parameter].type != EGraphParameterType::TRIGGER)
	{
		return false;
	}

	m_values[p_parameter] = p_set ? 1.0f : 0.0f;
	return true;
}

float OvCore::Animation::AnimationGraph::StateDuration(const GraphState& p_state) const
{
	// An empty state counts its time in seconds
	if (p_state.motions.empty())
	{
		return 1.0f;
	}

	if (p_state.motions.size() == 1)
	{
		return std::max(m_clipDurations[p_state.motions.front().clip], kMinDuration);
	}

	// A blend lasts as its clips do, each as much as it weighs: walking and running in step
	std::vector<float> weights;
	BlendWeights(p_state, p_state.blendParameter ? m_values[*p_state.blendParameter] : 0.0f, weights);

	float duration = 0.0f;
	for (size_t i = 0; i < p_state.motions.size(); ++i)
	{
		duration += weights[i] * m_clipDurations[p_state.motions[i].clip];
	}

	return std::max(duration, kMinDuration);
}

void OvCore::Animation::AnimationGraph::Advance(StateInstance& p_instance, size_t p_layer, float p_deltaTime) const
{
	const auto& state = m_definition.layers[p_layer].states[p_instance.state];
	const float speed = state.speed * (state.speedParameter ? m_values[*state.speedParameter] : 1.0f);

	p_instance.time += p_deltaTime * speed / StateDuration(state);

	if (!state.loop)
	{
		p_instance.time = std::clamp(p_instance.time, 0.0f, 1.0f);
	}
}

bool OvCore::Animation::AnimationGraph::ConditionsHold(const GraphTransition& p_transition) const
{
	for (const auto& condition : p_transition.conditions)
	{
		const float value = m_values[condition.parameter];
		bool holds = false;

		switch (condition.op)
		{
		case EGraphConditionOperator::GREATER: holds = value > condition.value; break;
		case EGraphConditionOperator::GREATER_EQUAL: holds = value >= condition.value; break;
		case EGraphConditionOperator::LESS: holds = value < condition.value; break;
		case EGraphConditionOperator::LESS_EQUAL: holds = value <= condition.value; break;
		case EGraphConditionOperator::EQUAL: holds = value == condition.value; break;
		case EGraphConditionOperator::NOT_EQUAL: holds = value != condition.value; break;
		case EGraphConditionOperator::TRIGGERED: holds = value != 0.0f; break;
		}

		if (!holds)
		{
			return false;
		}
	}

	return true;
}

bool OvCore::Animation::AnimationGraph::ExitTimeReached(const GraphTransition& p_transition, size_t p_layer, float p_previousTime) const
{
	const auto& current = m_layers[p_layer].current;
	const auto& state = m_definition.layers[p_layer].states[current.state];
	const float exitTime = *p_transition.exitTime;

	// Within a loop, the exit time comes back every loop: the transition is taken as the state passes it
	if (state.loop && exitTime < 1.0f)
	{
		return std::floor(current.time - exitTime) > std::floor(p_previousTime - exitTime);
	}

	return current.time >= exitTime;
}

void OvCore::Animation::AnimationGraph::StartTransition(size_t p_layer, size_t p_state, float p_duration)
{
	auto& layer = m_layers[p_layer];

	if (p_duration <= 0.0f)
	{
		layer.current = { p_state, 0.0f, 1.0f };
		layer.fadingOut.clear();
		layer.fadeElapsed = 0.0f;
		layer.fadeDuration = 0.0f;
		return;
	}

	// The blend as it stands fades out as one: a fade cut short starts from the pose it had reached
	const float progress = layer.fadeDuration > 0.0f ? std::clamp(layer.fadeElapsed / layer.fadeDuration, 0.0f, 1.0f) : 1.0f;

	std::vector<StateInstance> fadingOut;
	for (const auto& instance : layer.fadingOut)
	{
		fadingOut.push_back({ instance.state, instance.time, instance.weight * (1.0f - progress) });
	}
	fadingOut.push_back({ layer.current.state, layer.current.time, progress });

	std::erase_if(fadingOut, [](const StateInstance& p_instance) { return p_instance.weight < kMinWeight; });
	std::stable_sort(fadingOut.begin(), fadingOut.end(), [](const StateInstance& p_a, const StateInstance& p_b) { return p_a.weight > p_b.weight; });
	if (fadingOut.size() > kMaxFadingStates)
	{
		fadingOut.resize(kMaxFadingStates);
	}

	float total = 0.0f;
	for (const auto& instance : fadingOut)
	{
		total += instance.weight;
	}
	for (auto& instance : fadingOut)
	{
		instance.weight = total > 0.0f ? instance.weight / total : 0.0f;
	}

	layer.fadingOut = std::move(fadingOut);
	layer.current = { p_state, 0.0f, 1.0f };
	layer.fadeElapsed = 0.0f;
	layer.fadeDuration = layer.fadingOut.empty() ? 0.0f : p_duration;
}

void OvCore::Animation::AnimationGraph::Update(float p_deltaTime)
{
	for (size_t l = 0; l < m_layers.size(); ++l)
	{
		auto& layer = m_layers[l];
		const auto& definition = m_definition.layers[l];

		const float previousTime = layer.current.time;
		Advance(layer.current, l, p_deltaTime);
		for (auto& instance : layer.fadingOut)
		{
			Advance(instance, l, p_deltaTime);
		}

		if (layer.fadeDuration > 0.0f)
		{
			layer.fadeElapsed += std::max(p_deltaTime, 0.0f);
			if (layer.fadeElapsed >= layer.fadeDuration)
			{
				layer.fadingOut.clear();
				layer.fadeElapsed = 0.0f;
				layer.fadeDuration = 0.0f;
			}
		}

		// Transitions from any state first, then the state's own, each in the file's order: the first that holds is
		// taken, and uses up the triggers it tested
		const GraphTransition* taken = nullptr;
		for (int pass = 0; pass < 2 && !taken; ++pass)
		{
			for (const auto& transition : definition.transitions)
			{
				const bool fromAny = !transition.from.has_value();
				if (fromAny != (pass == 0) || (!fromAny && *transition.from != layer.current.state))
				{
					continue;
				}

				if (fromAny && transition.to == layer.current.state)
				{
					continue;
				}

				if (ConditionsHold(transition) && (!transition.exitTime || ExitTimeReached(transition, l, previousTime)))
				{
					taken = &transition;
					break;
				}
			}
		}

		if (taken)
		{
			for (const auto& condition : taken->conditions)
			{
				if (condition.op == EGraphConditionOperator::TRIGGERED)
				{
					m_values[condition.parameter] = 0.0f;
				}
			}

			StartTransition(l, taken->to, taken->duration);
		}
	}

	m_samples.clear();
	for (size_t l = 0; l < m_layers.size(); ++l)
	{
		const auto& layer = m_layers[l];
		const float progress = layer.fadeDuration > 0.0f ? std::clamp(layer.fadeElapsed / layer.fadeDuration, 0.0f, 1.0f) : 1.0f;

		AppendSamples(l, layer.current, progress);
		for (const auto& instance : layer.fadingOut)
		{
			AppendSamples(l, instance, (1.0f - progress) * instance.weight);
		}
	}
}

bool OvCore::Animation::AnimationGraph::Play(size_t p_layer, size_t p_state, float p_fade)
{
	if (p_layer >= m_layers.size() || p_state >= m_definition.layers[p_layer].states.size())
	{
		return false;
	}

	StartTransition(p_layer, p_state, p_fade);
	return true;
}

size_t OvCore::Animation::AnimationGraph::GetState(size_t p_layer) const
{
	return p_layer < m_layers.size() ? m_layers[p_layer].current.state : 0;
}

float OvCore::Animation::AnimationGraph::GetStateTime(size_t p_layer) const
{
	return p_layer < m_layers.size() ? m_layers[p_layer].current.time : 0.0f;
}

bool OvCore::Animation::AnimationGraph::IsInTransition(size_t p_layer) const
{
	return p_layer < m_layers.size() && m_layers[p_layer].fadeDuration > 0.0f;
}

const std::vector<OvCore::Animation::AnimationGraph::Sample>& OvCore::Animation::AnimationGraph::GetSamples() const
{
	return m_samples;
}

void OvCore::Animation::AnimationGraph::Reset()
{
	m_values.clear();
	for (const auto& parameter : m_definition.parameters)
	{
		m_values.push_back(parameter.type == EGraphParameterType::TRIGGER ? 0.0f : parameter.defaultValue);
	}

	m_layers.assign(m_definition.layers.size(), {});
	for (size_t l = 0; l < m_layers.size(); ++l)
	{
		m_layers[l].current = { m_definition.layers[l].defaultState, 0.0f, 1.0f };
	}

	// The samples of the start, with no time gone by and no transition taken
	m_samples.clear();
	for (size_t l = 0; l < m_layers.size(); ++l)
	{
		AppendSamples(l, m_layers[l].current, 1.0f);
	}
}

void OvCore::Animation::AnimationGraph::BlendWeights(const GraphState& p_state, float p_value, std::vector<float>& p_weights)
{
	const auto& motions = p_state.motions;
	p_weights.assign(motions.size(), 0.0f);

	if (motions.empty())
	{
		return;
	}

	if (motions.size() == 1 || p_value <= motions.front().position)
	{
		p_weights.front() = 1.0f;
		return;
	}

	if (p_value >= motions.back().position)
	{
		p_weights.back() = 1.0f;
		return;
	}

	for (size_t i = 0; i + 1 < motions.size(); ++i)
	{
		const float from = motions[i].position;
		const float to = motions[i + 1].position;

		if (p_value >= from && p_value <= to)
		{
			const float alpha = to - from > kMinDuration ? (p_value - from) / (to - from) : 0.0f;
			p_weights[i] = 1.0f - alpha;
			p_weights[i + 1] = alpha;
			return;
		}
	}
}

void OvCore::Animation::AnimationGraph::AppendSamples(size_t p_layer, const StateInstance& p_instance, float p_weight)
{
	if (p_weight < kMinWeight * kMinWeight)
	{
		return;
	}

	const auto& state = m_definition.layers[p_layer].states[p_instance.state];
	BlendWeights(state, state.blendParameter ? m_values[*state.blendParameter] : 0.0f, m_blendWeights);

	// Every clip of a blend at the same point of its own length
	const float phase = state.loop ? Fraction(p_instance.time) : std::clamp(p_instance.time, 0.0f, 1.0f);

	for (size_t i = 0; i < state.motions.size(); ++i)
	{
		if (m_blendWeights[i] <= 0.0f)
		{
			continue;
		}

		const size_t clip = state.motions[i].clip;
		m_samples.push_back({ p_layer, clip, phase * m_clipDurations[clip], p_weight * m_blendWeights[i], state.loop });
	}
}
