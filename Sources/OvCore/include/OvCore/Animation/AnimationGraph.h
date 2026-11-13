/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace OvCore::Animation
{
	/**
	* The kind of a graph parameter
	*/
	enum class EGraphParameterType : uint8_t
	{
		FLOAT,
		BOOL,
		TRIGGER
	};

	/**
	* A value a script sets and transitions test: a number, a boolean, or a trigger (set, then used up by the first
	* transition that tests it)
	*/
	struct GraphParameter
	{
		std::string name;
		EGraphParameterType type = EGraphParameterType::FLOAT;
		float defaultValue = 0.0f; // 0 or 1 for a boolean
	};

	/**
	* How a condition tests its parameter
	*/
	enum class EGraphConditionOperator : uint8_t
	{
		GREATER,
		GREATER_EQUAL,
		LESS,
		LESS_EQUAL,
		EQUAL,
		NOT_EQUAL,
		TRIGGERED
	};

	/**
	* A test on a parameter
	*/
	struct GraphCondition
	{
		size_t parameter = 0;
		EGraphConditionOperator op = EGraphConditionOperator::TRIGGERED;
		float value = 0.0f;
	};

	/**
	* A clip a state plays, and where it stands on the state's blend parameter
	*/
	struct GraphMotion
	{
		size_t clip = 0;
		float position = 0.0f;
	};

	/**
	* What a layer plays: one clip, or clips blended along a parameter (idle, walk and run along the speed), their
	* times kept in step, or nothing (an empty state, through which the layers before show)
	*/
	struct GraphState
	{
		std::string name;
		std::vector<GraphMotion> motions;			// Sorted by position
		std::optional<size_t> blendParameter;		// Set for a blend
		float speed = 1.0f;
		std::optional<size_t> speedParameter;		// Multiplies the speed when set
		bool loop = true;
	};

	/**
	* A move from a state, or from any state, to another, when every condition holds (and, with an exit time, once
	* the state has played that far), over a fade
	*/
	struct GraphTransition
	{
		std::optional<size_t> from;					// Nothing: from any state
		size_t to = 0;
		std::vector<GraphCondition> conditions;
		float duration = 0.25f;						// Seconds
		std::optional<float> exitTime;				// In the state's normalized time
	};

	/**
	* States and transitions played over the layers below, on the bones under its mask
	*/
	struct GraphLayer
	{
		std::string name;
		float weight = 1.0f;
		std::string mask;							// A bone: only it and the bones under it; empty: every bone
		std::vector<GraphState> states;
		std::vector<GraphTransition> transitions;
		size_t defaultState = 0;
	};

	/**
	* A graph as a file describes it
	*/
	struct GraphDefinition
	{
		std::vector<GraphParameter> parameters;
		std::vector<std::string> clips;				// Every clip named, once
		std::vector<std::string> clipPaths;			// Where each clip is first named in the file, for errors
		std::vector<GraphLayer> layers;

		/**
		* Reads a graph from its JSON text, returning why it cannot (with the JSON path of the mistake), or nothing
		* @param p_text
		* @param p_definition
		*/
		static std::optional<std::string> Parse(std::string_view p_text, GraphDefinition& p_definition);

		/**
		* Returns the index of a parameter by name
		* @param p_name
		*/
		std::optional<size_t> FindParameter(std::string_view p_name) const;
	};

	/**
	* Plays a graph: its parameters, which state each layer is in, the fades between states, and, every update, the
	* clips to sample with their times and weights. Knows nothing of models: it is given each clip's duration.
	*/
	class AnimationGraph
	{
	public:
		/**
		* A clip to sample: its layer, its time, and its weight within the layer (a layer's weights sum to 1, or less
		* while an empty state takes part)
		*/
		struct Sample
		{
			size_t layer = 0;
			size_t clip = 0;
			float time = 0.0f;						// Seconds
			float weight = 1.0f;
			bool loop = true;
		};

		/**
		* Constructor
		* @param p_definition
		* @param p_clipDurations (seconds, one per clip of the definition)
		*/
		AnimationGraph(GraphDefinition p_definition, std::vector<float> p_clipDurations);

		/**
		* Returns the definition played
		*/
		const GraphDefinition& GetDefinition() const;

		/**
		* Sets a number or a boolean (0 or 1), returning false for an unknown parameter or a trigger
		* @param p_parameter
		* @param p_value
		*/
		bool SetValue(size_t p_parameter, float p_value);

		/**
		* Returns a parameter's value (a trigger: 1 while set)
		* @param p_parameter
		*/
		float GetValue(size_t p_parameter) const;

		/**
		* Sets or clears a trigger, returning false for an unknown parameter or one that is no trigger
		* @param p_parameter
		* @param p_set
		*/
		bool SetTrigger(size_t p_parameter, bool p_set = true);

		/**
		* Moves time on by p_deltaTime, takes the transitions whose conditions hold, and works the samples out
		* @param p_deltaTime
		*/
		void Update(float p_deltaTime);

		/**
		* Moves a layer to a state at once, over a fade, whatever the transitions say. Returns false for an unknown
		* layer or state
		* @param p_layer
		* @param p_state
		* @param p_fade (seconds)
		*/
		bool Play(size_t p_layer, size_t p_state, float p_fade);

		/**
		* Returns the state a layer is in (the one it goes to while it fades)
		* @param p_layer
		*/
		size_t GetState(size_t p_layer) const;

		/**
		* Returns how far the layer's state has played, in its normalized time (1 per loop)
		* @param p_layer
		*/
		float GetStateTime(size_t p_layer) const;

		/**
		* Returns true while a layer fades from a state to another
		* @param p_layer
		*/
		bool IsInTransition(size_t p_layer) const;

		/**
		* Returns the samples the last update worked out
		*/
		const std::vector<Sample>& GetSamples() const;

		/**
		* Puts every parameter back to its default and every layer in its default state
		*/
		void Reset();

		/**
		* Returns the weights of a 1D blend's motions for a parameter value
		* @param p_state
		* @param p_value
		* @param p_weights
		*/
		static void BlendWeights(const GraphState& p_state, float p_value, std::vector<float>& p_weights);

	private:
		struct StateInstance
		{
			size_t state = 0;
			float time = 0.0f;						// Normalized: 1 per loop
			float weight = 1.0f;					// Among the states fading out, their shares at the fade's start
		};

		struct LayerRuntime
		{
			StateInstance current;
			std::vector<StateInstance> fadingOut;
			float fadeElapsed = 0.0f;
			float fadeDuration = 0.0f;
		};

		float StateDuration(const GraphState& p_state) const;
		void Advance(StateInstance& p_instance, size_t p_layer, float p_deltaTime) const;
		bool ConditionsHold(const GraphTransition& p_transition) const;
		bool ExitTimeReached(const GraphTransition& p_transition, size_t p_layer, float p_previousTime) const;
		void StartTransition(size_t p_layer, size_t p_state, float p_duration);
		void AppendSamples(size_t p_layer, const StateInstance& p_instance, float p_weight);

	private:
		GraphDefinition m_definition;
		std::vector<float> m_clipDurations;
		std::vector<float> m_values;
		std::vector<LayerRuntime> m_layers;
		std::vector<Sample> m_samples;
		std::vector<float> m_blendWeights;
	};
}
