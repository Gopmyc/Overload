/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cmath>
#include <string>
#include <vector>

#include <doctest/doctest.h>
#include <OvCore/Scripting/Lua/LuaComponentRef.h>

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvCore/Scripting/ScriptEngine.h>
#include <OvTests/TestUtils.h>
#include <OvTools/Time/Clock.h>

namespace
{
	/**
	* A script engine on a temporary project and the engine's own resources, where the standard library lives. The
	* clock is never updated: real time stands still, game time advances by the steps given to Frame
	*/
	struct StdFixture
	{
		OvTests::TempFolder project;
		OvTools::Time::Clock clock;
		OvCore::Scripting::ScriptEngine engine{ project.GetPath(), OvTests::FindEngineResources() };

		StdFixture()
		{
			OvCore::Global::ServiceLocator::Provide(engine);
			OvCore::Global::ServiceLocator::Provide(clock);
		}

		/**
		* Loads a script of the project and returns the table it returns
		*/
		sol::table Load(const std::string& p_code)
		{
			static int count = 0;
			const std::string path = "Scripts/Test" + std::to_string(++count) + ".lua";
			project.WriteFile(path, p_code);

			const sol::object loaded = engine.GetScript(path);
			REQUIRE(loaded.get_type() == sol::type::table);
			return loaded.as<sol::table>();
		}

		/**
		* Runs a frame's late update, which advances the timers and the tweens
		*/
		static void Frame(float p_step)
		{
			OvCore::SceneSystem::Scene::LateUpdateEvent.Invoke(p_step);
		}
	};

	std::vector<std::string> Strings(const sol::table& p_table)
	{
		std::vector<std::string> result;
		for (size_t i = 1; i <= p_table.size(); ++i)
			result.push_back(p_table.get<std::string>(i));
		return result;
	}

	/**
	* Calls a method of an object a script returned, as object:method()
	*/
	template<typename T>
	T Call(const sol::table& p_table, const char* p_key, const char* p_method)
	{
		const sol::table object = p_table[p_key];
		const sol::protected_function method = object[p_method];
		const sol::protected_function_result result = method(object);
		REQUIRE(result.valid());
		return result.get<T>();
	}

	bool Has(const sol::table& p_table, const char* p_key, const char* p_text)
	{
		const auto value = p_table.get<sol::optional<std::string>>(p_key);
		return value.has_value() && value->find(p_text) != std::string::npos;
	}
}

TEST_SUITE("Standard library")
{
	TEST_CASE_FIXTURE(StdFixture, "Class: instances read their class and its bases, and final names stay final")
	{
		const sol::table r = Load(R"(
			local Class = require(":Libraries/Std/Init").Class
			local r = {}

			local Base = Class.New()
			function Base:Name() return "base" end
			function Base:Hello() return "hello " .. self:Name() end
			local Derived = Class.New(Base)
			function Derived:Name() return "derived" end

			local instance = Class.Make(Derived, { x = 1 })
			r.hello = instance:Hello()
			r.field = instance.x
			r.isBase = Class.IsA(instance, Base)
			r.isDerived = Class.IsA(instance, Derived)
			r.baseIsDerived = Class.IsA(Class.Make(Base), Derived)
			r.number = Class.IsA(42, Base)
			r.base = Derived.Base == Base

			Class.Final(Base, { "Hello" })
			local Late = Class.New(Base)
			r.final = select(2, pcall(function() function Late:Hello() end end))
			r.other = pcall(function() function Late:Other() end end)
			local Grand = Class.New(Late)
			r.grand = pcall(function() Grand.Hello = 1 end)
			r.instanceSets = pcall(function() Class.Make(Late).Hello = 1 end)
			r.notClass = select(2, pcall(Class.Make, {}))
			return r
		)");

		CHECK(r.get<std::string>("hello") == "hello derived");
		CHECK(r.get<int>("field") == 1);
		CHECK(r.get<bool>("isBase"));
		CHECK(r.get<bool>("isDerived"));
		CHECK_FALSE(r.get<bool>("baseIsDerived"));
		CHECK_FALSE(r.get<bool>("number"));
		CHECK(r.get<bool>("base"));

		// A subclass made after Final can't set a final name, nor its own subclasses; an instance still can
		CHECK(Has(r, "final", "'Hello' is final"));
		CHECK(r.get<bool>("other"));
		CHECK_FALSE(r.get<bool>("grand"));
		CHECK(r.get<bool>("instanceSets"));
		CHECK(Has(r, "notClass", "Make takes a class"));
	}

	TEST_CASE_FIXTURE(StdFixture, "Signal: listeners in order, a change during an Emit, a failing listener dropped alone")
	{
		OvTests::LogCapture logs;

		const sol::table r = Load(R"(
			local Signal = require(":Libraries/Std/Signal")
			local r = { log = {} }
			local function add(entry) r.log[#r.log + 1] = entry end

			local signal = Signal.New()
			local first = signal:Connect(function(value) add("A" .. value) end)
			local second
			second = signal:Connect(function(value)
				add("B" .. value)
				signal:Disconnect(second)
				signal:Connect(function(other) add("N" .. other) end)
			end)
			signal:Connect(function() error("boom") end)
			signal:Connect(function(value, extra) add("C" .. value .. tostring(extra)) end)

			signal:Emit(1, "x")
			signal:Emit(2)

			r.count = signal:GetCount()
			r.disconnected = signal:Disconnect(first)
			r.again = signal:Disconnect(first)
			signal:Clear()
			r.cleared = signal:GetCount()
			signal:Emit(3)

			r.badCallback = select(2, pcall(signal.Connect, signal, 42))
			r.badOwner = select(2, pcall(signal.Connect, signal, function() end, 42))
			return r
		)");

		// B disconnects itself and connects N, which is first called at the next Emit
		CHECK(Strings(r["log"]) == std::vector<std::string>{ "A1", "B1", "C1x", "A2", "C2nil", "N2" });
		CHECK(r.get<int>("count") == 3);
		CHECK(r.get<bool>("disconnected"));
		CHECK_FALSE(r.get<bool>("again"));
		CHECK(r.get<int>("cleared") == 0);
		CHECK(logs.Count(OvDebug::ELogLevel::LOG_ERROR) == 1);
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_ERROR, "Std.Signal: a listener failed and is disconnected"));
		CHECK(Has(r, "badCallback", "Connect takes a function"));
		CHECK(Has(r, "badOwner", "an owner is an actor"));
	}

	TEST_CASE_FIXTURE(StdFixture, "Pool: free first, then new, then the oldest taken back; spent once make gives nothing")
	{
		const sol::table r = Load(R"(
			local Pool = require(":Libraries/Std/Pool")
			local r = {}
			local made, evicted = 0, {}

			local pool = Pool.New({
				max = 2,
				make = function() made = made + 1 return { id = made, busy = false } end,
				isFree = function(item) return not item.busy end,
				evict = function(item) evicted[#evicted + 1] = item.id end
			})

			local a = pool:Take() a.busy = true
			local b = pool:Take() b.busy = true
			r.made = made
			local c = pool:Take()
			r.evicted = evicted[1]
			r.oldestBack = c == a
			r.takes = pool:GetTakeCount()
			r.indexA = pool:GetTakeIndex(a)
			r.indexB = pool:GetTakeIndex(b)
			b.busy = false
			r.freeFirst = pool:Take() == b
			r.count = pool:GetCount()

			local kept = Pool.New({
				max = 1,
				make = function() return {} end,
				isFree = function() return false end,
				canEvict = function() return false end
			})
			kept:Take()
			r.none = kept:Take() == nil

			local calls = 0
			local spent = Pool.New({ max = 5, make = function() calls = calls + 1 return nil end, isFree = function() return true end })
			spent:Take() spent:Take()
			r.makeCalls = calls

			r.bad = select(2, pcall(Pool.New, { max = 1 }))
			return r
		)");

		CHECK(r.get<int>("made") == 2);
		CHECK(r.get<int>("evicted") == 1);
		CHECK(r.get<bool>("oldestBack"));
		CHECK(r.get<int>("takes") == 3);
		CHECK(r.get<int>("indexA") == 3);
		CHECK(r.get<int>("indexB") == 2);
		CHECK(r.get<bool>("freeFirst"));
		CHECK(r.get<int>("count") == 2);
		CHECK(r.get<bool>("none"));
		CHECK(r.get<int>("makeCalls") == 1);
		CHECK(Has(r, "bad", "'make' is a function"));
	}

	TEST_CASE_FIXTURE(StdFixture, "StateMachine: the first state that holds, with exit and enter as it changes")
	{
		const sol::table r = Load(R"(
			local StateMachine = require(":Libraries/Std/StateMachine")
			local r = { log = {} }
			local function add(entry) r.log[#r.log + 1] = entry end
			local mind = { health = 100 }

			local machine = StateMachine.New(mind, {
				{ name = "Flee", condition = function(owner) return owner.health < 10 end,
				  enter = function(owner, previous) add("enter Flee from " .. tostring(previous)) end },
				{ name = "Fight", condition = function(owner) return owner.target ~= nil end,
				  enter = function() add("enter Fight") end,
				  exit = function(owner, next) add("exit Fight to " .. tostring(next)) end,
				  update = function(owner, dt) add("fight " .. dt) end },
				{ name = "Idle", update = function(owner, dt) add("idle " .. dt) end }
			})

			r.before = tostring(machine:GetState())
			r.first = machine:Update(1)
			mind.target = "orc"
			r.second = machine:Update(2)
			mind.health = 5
			r.third = machine:Update(3)
			r.forced = machine:SetState("Idle")
			r.state = machine:GetState()
			r.unknown = machine:SetState("Nope")
			r.duplicate = select(2, pcall(StateMachine.New, mind, { { name = "A" }, { name = "A" } }))
			return r
		)");

		CHECK(r.get<std::string>("before") == "nil");
		CHECK(r.get<std::string>("first") == "Idle");
		CHECK(r.get<std::string>("second") == "Fight");
		CHECK(r.get<std::string>("third") == "Flee");
		CHECK(Strings(r["log"]) == std::vector<std::string>{ "idle 1", "enter Fight", "fight 2", "exit Fight to Flee", "enter Flee from Fight" });
		CHECK(r.get<bool>("forced"));
		CHECK(r.get<std::string>("state") == "Idle");
		CHECK_FALSE(r.get<bool>("unknown"));
		CHECK(Has(r, "duplicate", "two states are named 'A'"));
	}

	TEST_CASE_FIXTURE(StdFixture, "Ease: every curve from 0 to 1 with a clamped input, and an approach that ignores the frame rate")
	{
		const sol::table r = Load(R"(
			local Ease = require(":Libraries/Std/Ease")
			local r = {}
			local wrong = {}
			for _, name in ipairs({ "Linear", "InQuad", "OutQuad", "InOutQuad", "InCubic", "OutCubic", "InOutCubic",
				"InBack", "OutBack", "SmoothStep", "SmootherStep" }) do
				local curve = Ease[name]
				if math.abs(curve(0)) > 1e-9 or math.abs(curve(1) - 1) > 1e-9 or curve(-1) ~= curve(0) or curve(2) ~= curve(1) then
					wrong[#wrong + 1] = name
				end
			end
			r.wrong = table.concat(wrong, ",")
			r.half = Ease.InOutCubic(0.5)
			r.smooth = Ease.SmootherStep(0.5)
			local highest = 0
			for i = 0, 100 do highest = math.max(highest, Ease.OutBack(i / 100)) end
			r.overshoot = highest
			r.approach = Ease.Approach(0, 10, 2, 0.5)
			r.twice = Ease.Approach(Ease.Approach(0, 10, 2, 0.25), 10, 2, 0.25)
			r.vector = Ease.Approach(Vector3.new(0, 0, 0), Vector3.new(10, 0, 0), 2, 0.5).x
			return r
		)");

		CHECK(r.get<std::string>("wrong").empty());
		CHECK(r.get<double>("half") == doctest::Approx(0.5));
		CHECK(r.get<double>("smooth") == doctest::Approx(0.5));
		CHECK(r.get<double>("overshoot") > 1.0);
		CHECK(r.get<double>("approach") == doctest::Approx(10.0 * (1.0 - std::exp(-1.0))));
		CHECK(r.get<double>("twice") == doctest::Approx(r.get<double>("approach")));
		CHECK(r.get<double>("vector") == doctest::Approx(r.get<double>("approach")));
	}

	TEST_CASE_FIXTURE(StdFixture, "Timers: after and every on game time, by themselves, a failure dropped alone")
	{
		OvTests::LogCapture logs;

		const sol::table r = Load(R"(
			local Timers = require(":Libraries/Std/Timers")
			local r = { log = {} }
			local function add(entry) r.log[#r.log + 1] = entry end

			Timers.After(1, function() add("after") end)
			local count = 0
			Timers.Every(0.5, function()
				count = count + 1
				add("every" .. count)
				if count == 3 then return false end
			end)
			r.cancelled = Timers.After(0.2, function() add("never") end)
			r.cancelled:Cancel()
			Timers.After(0.25, function() error("boom") end)
			Timers.After(0.25, function()
				add("nested")
				Timers.After(0, function() add("next") end)
			end)
			r.unscaled = Timers.After(0.1, function() add("unscaled") end, { unscaled = true })
			r.long = Timers.After(10, function() end)

			r.badDelay = select(2, pcall(Timers.After, "1", function() end))
			r.badCallback = select(2, pcall(Timers.Every, 1, 42))
			r.badOwner = select(2, pcall(Timers.After, 1, function() end, { owner = 42 }))
			return r
		)");

		for (int frame = 0; frame < 8; ++frame)
			Frame(0.25f);

		// The timer made while the timers advanced first counts at the next frame; Every stops when it returns false
		CHECK(Strings(r["log"]) == std::vector<std::string>{ "nested", "every1", "next", "after", "every2", "every3" });
		CHECK_FALSE(Call<bool>(r, "cancelled", "IsPending"));
		CHECK(Call<bool>(r, "long", "IsPending"));

		// Real time stands still here: the unscaled timer waits
		CHECK(Call<bool>(r, "unscaled", "IsPending"));

		CHECK(logs.Count(OvDebug::ELogLevel::LOG_ERROR) == 1);
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_ERROR, "Std.Timers: a callback failed and its timer is cancelled"));
		CHECK(Has(r, "badDelay", "After takes a number of seconds"));
		CHECK(Has(r, "badCallback", "Every takes a function"));
		CHECK(Has(r, "badOwner", "an owner is an actor"));
	}

	TEST_CASE_FIXTURE(StdFixture, "Timers: a repeat fires once in a long frame, without catching up")
	{
		const sol::table r = Load(R"(
			local Timers = require(":Libraries/Std/Timers")
			local r = { count = 0 }
			Timers.Every(0.5, function() r.count = r.count + 1 end)
			return r
		)");

		Frame(2.0f);
		CHECK(r.get<int>("count") == 1);
		Frame(0.25f);
		CHECK(r.get<int>("count") == 1);
		Frame(0.25f);
		CHECK(r.get<int>("count") == 2);
	}

	TEST_CASE_FIXTURE(StdFixture, "Tween: values over time, eased, delayed, looped, chained, cancelled and finished")
	{
		OvTests::LogCapture logs;

		const sol::table r = Load(R"(
			local Std = require(":Libraries/Std/Init")
			local Tween, Ease = Std.Tween, Std.Ease
			local r = { values = {}, chain = {}, done = 0 }
			local function chain(entry) r.chain[#r.chain + 1] = entry end

			r.main = Tween.Run({ duration = 1, from = 10, to = 20,
				onUpdate = function(value) r.values[#r.values + 1] = value end,
				onDone = function() r.done = r.done + 1 end })
			Tween.Run({ duration = 0.5, from = Vector3.new(0, 0, 0), to = Vector3.new(0, 4, 0),
				onUpdate = function(value) r.vector = value.y end })
			Tween.Run({ duration = 1, delay = 0.5, onUpdate = function(value) r.delayed = value end })
			r.yoyo = Tween.Run({ duration = 0.5, loops = 2, yoyo = true, onUpdate = function(value) r.yoyoValue = value end })
			Tween.Run({ duration = 1, ease = Ease.InQuad, onUpdate = function(value) r.eased = value end })

			local first = Tween.Run({ duration = 0.5, onUpdate = function() chain("a") end, onDone = function() chain("a done") end })
			first:Then({ duration = 0.5, onUpdate = function() chain("b") end, onDone = function() chain("b done") end })

			r.cancelled = Tween.Run({ duration = 1, onDone = function() r.cancelledDone = true end })
			r.cancelled:Cancel()
			r.finished = Tween.Run({ duration = 10, from = 0, to = 5,
				onUpdate = function(value) r.finishedValue = value end, onDone = function() r.finishedDone = true end })
			r.finished:Finish()

			Tween.Run({ duration = 1, onUpdate = function() error("boom") end })
			r.bad = select(2, pcall(Tween.Run, 42))
			r.badEase = select(2, pcall(Tween.Run, { ease = 1 }))
			return r
		)");

		auto isRunning = [&](const char* p_key) { return Call<bool>(r, p_key, "IsRunning"); };

		// Run applies the start at once; Finish the end, then onDone; Cancel neither
		CHECK(r.get<sol::table>("values").get<double>(1) == doctest::Approx(10.0));
		CHECK(r.get<double>("finishedValue") == doctest::Approx(5.0));
		CHECK(r.get<bool>("finishedDone"));
		CHECK_FALSE(isRunning("finished"));
		CHECK_FALSE(isRunning("cancelled"));

		Frame(0.25f);
		Frame(0.25f);
		CHECK(Call<double>(r, "main", "GetProgress") == doctest::Approx(0.5));
		CHECK(r.get<double>("vector") == doctest::Approx(4.0));
		CHECK(r.get<double>("delayed") == doctest::Approx(0.0));
		CHECK(r.get<double>("eased") == doctest::Approx(0.25));
		CHECK(r.get<double>("yoyoValue") == doctest::Approx(1.0));

		Frame(0.25f);
		Frame(0.25f);
		std::vector<double> values;
		const sol::table valueTable = r["values"];
		for (size_t i = 1; i <= valueTable.size(); ++i)
			values.push_back(valueTable.get<double>(i));
		CHECK(values == std::vector<double>{ 10.0, 12.5, 15.0, 17.5, 20.0 });
		CHECK(r.get<int>("done") == 1);
		CHECK_FALSE(isRunning("main"));
		CHECK(r.get<double>("delayed") == doctest::Approx(0.5));

		// Two yoyo runs end where they began
		CHECK(r.get<double>("yoyoValue") == doctest::Approx(0.0));
		CHECK_FALSE(isRunning("yoyo"));

		// The chained tween starts as the first ends, and first moves at the next frame
		CHECK(Strings(r["chain"]) == std::vector<std::string>{ "a", "a", "a", "a done", "b", "b", "b", "b done" });
		CHECK_FALSE(r.get<sol::optional<bool>>("cancelledDone").has_value());

		CHECK(logs.Count(OvDebug::ELogLevel::LOG_ERROR) == 1);
		CHECK(logs.Contains(OvDebug::ELogLevel::LOG_ERROR, "Std.Tween: a function failed and its tween is stopped"));
		CHECK(Has(r, "bad", "Run takes a table"));
		CHECK(Has(r, "badEase", "'ease' is a function"));
	}
}
