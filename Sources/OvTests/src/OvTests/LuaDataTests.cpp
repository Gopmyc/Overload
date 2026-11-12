/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <string>

#include <doctest/doctest.h>
#include <OvCore/Scripting/Lua/LuaComponentRef.h>

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Scripting/JsonLimits.h>
#include <OvTests/TestUtils.h>
#include <OvTools/Filesystem/SaveStorage.h>

// The engine's binding functions, defined in OvCore
void BindLuaJson(sol::state& p_state);
void BindLuaStorage(sol::state& p_state);

namespace
{
	/**
	* A Lua state with the standard libraries a script has, and the bindings under test
	*/
	struct LuaFixture
	{
		sol::state lua;

		LuaFixture()
		{
			lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);
			BindLuaJson(lua);
			BindLuaStorage(lua);
		}

		/**
		* Runs a chunk that returns a string (or raises), and returns that string
		*/
		std::string Run(const std::string& p_code)
		{
			const auto result = lua.safe_script(p_code, sol::script_pass_on_error);
			if (!result.valid())
			{
				const sol::error error = result;
				return std::string("lua error: ") + error.what();
			}
			return result.get<std::string>();
		}
	};

	/**
	* Restores the default Json limits when a test changed them
	*/
	struct JsonLimitsGuard
	{
		~JsonLimitsGuard() { OvCore::Scripting::SetJsonLimits({}); }
	};
}

TEST_SUITE("Lua Json")
{
	TEST_CASE_FIXTURE(LuaFixture, "values make the round trip")
	{
		CHECK(Run(R"(
			local text = assert(Json.Encode({ name = "Elf", level = 3, ratio = 0.1, items = { 1, 2, 3 }, alive = true }))
			local value = assert(Json.Decode(text))
			assert(value.name == "Elf" and value.level == 3 and value.ratio == 0.1 and value.alive == true)
			assert(math.type(value.level) == "integer" and #value.items == 3 and value.items[3] == 3)
			return text
		)") == R"({"alive":true,"items":[1,2,3],"level":3,"name":"Elf","ratio":0.1})");

		CHECK(Run(R"(return Json.Encode({}))") == "[]");
		CHECK(Run(R"(local v = Json.Decode('{"1": 2, "skip": null}') return tostring(v["1"]) .. " " .. tostring(v.skip))") == "2 nil");
	}

	TEST_CASE_FIXTURE(LuaFixture, "what JSON cannot hold is refused with a message")
	{
		const auto encodeError = [this](const std::string& p_value)
		{
			return Run("local text, error = Json.Encode(" + p_value + ") assert(text == nil) return error");
		};

		CHECK(encodeError("{ 1, 2, nil, 4 }").starts_with("json: "));
		CHECK(encodeError("{ 1, x = 2 }").starts_with("json: "));
		CHECK(encodeError("{ f = print }").starts_with("json: "));
		CHECK(encodeError("1 / 0").starts_with("json: "));
		CHECK(encodeError("'\\xFF'").starts_with("json: "));
		CHECK(encodeError("(function() local t = {} t[1] = t return t end)()").starts_with("json: "));

		const auto decodeError = [this](const std::string& p_text)
		{
			return Run("local value, error = Json.Decode(" + p_text + ") assert(value == nil) return error");
		};

		CHECK(decodeError("'[1, 2'").starts_with("json: "));
		CHECK(decodeError("'[1] 2'").starts_with("json: "));
		CHECK(decodeError("'[null]'").starts_with("json: "));
		CHECK(decodeError("'{\"a\": 1, \"a\": 2}'").starts_with("json: "));
		CHECK(decodeError("'\\xEF\\xBB\\xBF[]'").starts_with("json: "));
		CHECK(decodeError("'{print}'").starts_with("json: "));
	}

	TEST_CASE_FIXTURE(LuaFixture, "the limits set from the project hold")
	{
		JsonLimitsGuard guard;
		OvCore::Scripting::SetJsonLimits({ 32, 3 });
		CHECK(OvCore::Scripting::GetJsonLimits().maxDepth == 3);

		CHECK(Run("return Json.Encode({ { { 1 } } })") == "[[[1]]]");
		CHECK(Run("local text, error = Json.Encode({ { { { 1 } } } }) return tostring(error)").starts_with("json: "));
		CHECK(Run("local value, error = Json.Decode('[[[[1]]]]') return tostring(error)").starts_with("json: "));
		CHECK(Run("local text, error = Json.Encode(string.rep('x', 40)) return tostring(error)").starts_with("json: "));
		CHECK(Run("local value, error = Json.Decode('\"' .. string.rep('x', 40) .. '\"') return tostring(error)").starts_with("json: "));
	}
}

TEST_SUITE("Lua Storage")
{
	TEST_CASE_FIXTURE(LuaFixture, "scripts read and write through the sandbox")
	{
		OvTests::TempFolder folder;
		OvTools::Filesystem::SaveStorage storage(folder.GetPath());
		OvCore::Global::ServiceLocator::Provide(storage);

		CHECK(Run(R"(
			assert(Storage.Write("slots/1.json", Json.Encode({ level = 2 })))
			local text = assert(Storage.Read("slots/1.json"))
			assert(Json.Decode(text).level == 2)
			assert(Storage.Exists("slots") and not Storage.Exists("slots/2.json"))
			local names = assert(Storage.List(""))
			return table.concat(names, ",")
		)") == "slots/");

		CHECK(Run(R"(local ok, error = Storage.Write("../escape.json", "x") return tostring(ok) .. " " .. error)") == "nil invalid path");
		CHECK(Run(R"(local ok, error = Storage.Write(42, "x") return tostring(ok) .. " " .. error)") == "nil invalid path");
		CHECK(Run(R"(local ok, error = Storage.Write("a.json", {}) return tostring(ok) .. " " .. error)") == "nil invalid data");
		CHECK(Run(R"(local value, error = Storage.Read("none.json") return tostring(value) .. " " .. error)") == "nil not found");
		CHECK(Run(R"(local ok, error = Storage.Delete("slots") return tostring(ok) .. " " .. error)") == "nil folder not empty");
		CHECK_FALSE(std::filesystem::exists(folder.GetPath().parent_path() / "escape.json"));
	}
}
