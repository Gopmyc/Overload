/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <string>

#include <doctest/doctest.h>
#include <OvCore/Scripting/Lua/LuaComponentRef.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvCore/Scripting/Lua/LuaApi.h>
#include <OvMaths/FVector3.h>

namespace
{
	// With its safety checks on (Debug builds), sol refuses most of these values itself, in its own words, before
	// the engine's check sees them: the engine's words are checked where sol leaves them to it (Release builds)
	constexpr bool kEngineWords = !sol::detail::default_safe_function_calls;

	/**
	* A Lua state with the engine's bindings, and a scene to take components from
	*/
	struct MathFixture
	{
		sol::state lua;
		OvCore::SceneSystem::Scene scene{ "", "" };

		MathFixture()
		{
			OvCore::Scripting::LuaApi::OpenLibraries(lua);
			OvCore::Scripting::LuaApi::Bind(lua);

			// A table dressed as a vector: a copy of a vector's metatable, its marks included. Not the metatable itself,
			// whose __gc would be called on the table when it is collected, read as a vector
			lua.script(R"(
				function FakeVector()
					local metatable = {}
					for key, value in pairs(getmetatable(Vector3.new())) do metatable[key] = value end
					metatable.__gc = nil
					return setmetatable({}, metatable)
				end
			)");
		}

		/**
		* Runs a chunk and returns what it returns, as a string, or the error it raised
		*/
		std::string Run(const std::string& p_chunk)
		{
			const auto result = lua.safe_script(p_chunk, sol::script_pass_on_error);
			if (!result.valid())
			{
				const sol::error error = result;
				return std::string("error: ") + error.what();
			}

			const sol::object value = result;
			return value.is<std::string>() ? value.as<std::string>() : value.is<bool>() ? (value.as<bool>() ? "true" : "false") : "?";
		}

		/**
		* Runs an expression under pcall, and returns "accepted", or the message it raised. The expression is not
		* returned straight away: a tail call would hide the name of the function from the error
		*/
		std::string Call(const std::string& p_expression)
		{
			return Run("local ok, message = pcall(function() local result = " + p_expression + "; return result end) "
				"if ok then return 'accepted' end return tostring(message)");
		}

		/**
		* Checks that an expression raises, with this message where the engine words it
		*/
		void Refused(const std::string& p_expression, const std::string& p_message)
		{
			CAPTURE(p_expression);
			const std::string message = Call(p_expression);
			CHECK(message != "accepted");

			if constexpr (kEngineWords)
			{
				CHECK(message == p_message);
			}
		}

		/**
		* Checks that an expression raises, with a message that holds this part where the engine words it
		*/
		void RefusedWith(const std::string& p_expression, const std::string& p_part)
		{
			CAPTURE(p_expression);
			const std::string message = Call(p_expression);
			CHECK(message != "accepted");

			if constexpr (kEngineWords)
			{
				CAPTURE(message);
				CHECK(message.find(p_part) != std::string::npos);
			}
		}
	};
}

TEST_SUITE("Maths arguments")
{
	TEST_CASE_FIXTURE(MathFixture, "maths values are taken as before: built in Lua, held by C++, optional or left out")
	{
		CHECK(Run("return tostring(Vector3.Distance(Vector3.new(0, 0, 0), Vector3.new(3, 4, 0)))") == "5.0");
		CHECK(Run("return tostring((Vector3.new(1, 2, 3) + Vector3.new(1, 1, 1)).z)") == "4.0");
		CHECK(Run("return tostring((Vector3.new(1, 2, 3) * 2).y)") == "4.0");
		CHECK(Run("return tostring(Vector3.new(1, 2, 3):Dot(Vector3.new(1, 0, 0)))") == "1.0");
		CHECK(Run("return tostring(Vector2.Dot(Vector2.new(1, 2), Vector2.new(3, 4)))") == "11.0");
		CHECK(Run("return tostring(Vector4.Length(Vector4.new(0, 0, 3, 4)))") == "5.0");
		CHECK(Run("return tostring(Quaternion.RotatePoint(Vector3.new(1, 0, 0), Quaternion.new()).x)") == "1.0");
		CHECK(Run("return tostring(Matrix3.Determinant(Matrix3.new()))") == "1.0");
		CHECK(Run("return tostring((Matrix4.new() * Vector4.new(1, 2, 3, 1)).y)") == "2.0");

		// A vector C++ holds, which Lua reaches through a pointer
		OvMaths::FVector3 held{ 3.0f, 4.0f, 0.0f };
		lua["held"] = &held;
		CHECK(Run("return tostring(Vector3.Distance(held, Vector3.new(0, 0, 0)))") == "5.0");
		CHECK(Run("return tostring((held + Vector3.new(0, 0, 1)).z)") == "1.0");
		lua["fixed"] = static_cast<const OvMaths::FVector3*>(&held);
		CHECK(Run("return tostring(Vector3.Distance(fixed, Vector3.new(0, 0, 0)))") == "5.0");

		// An engine function that takes a vector, and one whose vector argument is optional
		auto& actor = scene.CreateActor("Box");
		lua["transform"] = &actor.transform;
		CHECK(Run("transform:SetLocalPosition(Vector3.new(1, 2, 3)) return tostring(transform:GetLocalPosition().y)") == "2.0");
		CHECK(Run("return tostring(Quaternion.new(Vector3.new(0, 90, 0)).w ~= nil)") == "true");
	}

	TEST_CASE_FIXTURE(MathFixture, "a value that is no maths value raises an error that says what was expected and what came")
	{
		// Every maths type, and every kind of value that is not one: nil, a table, a number, a string, a boolean
		Refused("Vector2.Dot(nil, Vector2.new())", "bad argument #1 to 'Dot' (Vector2 expected, got nil)");
		Refused("Vector3.Distance(Vector3.new(), { x = 0, y = 0, z = 0 })", "bad argument #2 to 'Distance' (Vector3 expected, got table)");
		Refused("Vector4.Length(4)", "bad argument #1 to 'Length' (Vector4 expected, got number)");
		Refused("Quaternion.Length('a')", "bad argument #1 to 'Length' (Quaternion expected, got string)");
		Refused("Matrix3.Determinant(true)", "bad argument #1 to 'Determinant' (Matrix3 expected, got boolean)");
		Refused("Matrix4.Transpose(nil)", "bad argument #1 to 'Transpose' (Matrix4 expected, got nil)");
		Refused("Vector3.Distance(Vector3.new())", "bad argument #2 to 'Distance' (Vector3 expected, got no value)");

		// Called as a method, an argument is counted after self, as Lua counts it, and self is named as such
		Refused("Vector3.new():Dot(nil)", "bad argument #1 to 'Dot' (Vector3 expected, got nil)");
		Refused("Vector3:Length()", "calling 'Length' on bad self (Vector3 expected, got table)");

		// The right operand of an operator
		RefusedWith("Vector3.new() + nil", "(Vector3 expected, got nil)");
		RefusedWith("Vector2.new() - {}", "(Vector2 expected, got table)");
	}

	TEST_CASE_FIXTURE(MathFixture, "a maths value of another type is refused, and named")
	{
		Refused("Vector3.Distance(Quaternion.new(), Vector3.new())", "bad argument #1 to 'Distance' (Vector3 expected, got Quaternion)");
		Refused("Matrix4.Transpose(Matrix3.new())", "bad argument #1 to 'Transpose' (Matrix4 expected, got Matrix3)");
		Refused("Vector2.Dot(Vector2.new(), Vector3.new())", "bad argument #2 to 'Dot' (Vector2 expected, got Vector3)");

		// Another userdata, and a table dressed as a vector
		auto& actor = scene.CreateActor("Box");
		lua["transform"] = &actor.transform;
		Refused("Vector3.Length(transform)", "bad argument #1 to 'Length' (Vector3 expected, got CTransform)");
		Refused("Vector3.Length(FakeVector())", "bad argument #1 to 'Length' (Vector3 expected, got table)");
		RefusedWith("FakeVector() + Vector3.new()", "(Vector3 expected, got table)");
	}

	TEST_CASE_FIXTURE(MathFixture, "the self of an operator or a field that is not even a userdata is refused")
	{
		// Lua gives an operator its operands in order: here the vector is the second, and self is something else. The
		// self is only checked to be a userdata: Lua hands an operator or a field the value it found it on
		RefusedWith("nil + Vector3.new()", "(Vector3 expected, got nil)");
		RefusedWith("{} - Vector2.new()", "(Vector2 expected, got table)");
		RefusedWith("getmetatable(Quaternion.new()).__add(5, Quaternion.new())", "(Quaternion expected, got number)");

		// A field of a table dressed as a vector
		RefusedWith("FakeVector().x", "(Vector3 expected, got table)");
		RefusedWith("(function() local fake = FakeVector(); fake.y = 2 end)()", "(Vector3 expected, got table)");

		// A static function reached through a vector, given nothing
		RefusedWith("Vector3.new().Dot()", "(Vector3 expected, got no value)");
	}

	TEST_CASE_FIXTURE(MathFixture, "an engine function given no vector raises instead of reading one")
	{
		auto& actor = scene.CreateActor("Box");
		lua["transform"] = &actor.transform;

		Refused("transform:SetLocalPosition(nil)", "bad argument #1 to 'SetLocalPosition' (Vector3 expected, got nil)");
		Refused("transform:SetLocalPosition({ x = 1, y = 2, z = 3 })", "bad argument #1 to 'SetLocalPosition' (Vector3 expected, got table)");
		Refused("transform:SetLocalRotation(Vector3.new())", "bad argument #1 to 'SetLocalRotation' (Quaternion expected, got Vector3)");

		// Nothing was written
		CHECK(actor.transform.GetLocalPosition().x == doctest::Approx(0.0f));
	}
}
