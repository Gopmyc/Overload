/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cmath>
#include <string>

#include <doctest/doctest.h>

#include <OvTools/Utils/Json.h>

namespace Json = OvTools::Utils::Json;

TEST_SUITE("Json")
{
	TEST_CASE("a text is read into values whose objects keep their members' order")
	{
		std::string error;
		const auto value = Json::Parse(R"( { "b": [1, -2.5e2, true, false, null], "a": { "name": "x" }, "b": "again" } )", error);
		REQUIRE_MESSAGE(value, error);
		REQUIRE(value->IsObject());

		const Json::Object& object = *value->AsObject();
		REQUIRE(object.size() == 3);
		CHECK(object[0].name == "b");
		CHECK(object[1].name == "a");

		// The first member of a name is the one found
		const Json::Array* list = value->Find("b")->AsArray();
		REQUIRE(list);
		REQUIRE(list->size() == 5);
		CHECK(*(*list)[0].AsNumber() == 1.0);
		CHECK(*(*list)[1].AsNumber() == -250.0);
		CHECK(*(*list)[2].AsBool());
		CHECK_FALSE(*(*list)[3].AsBool());
		CHECK((*list)[4].IsNull());

		CHECK(*value->Find("a")->Find("name")->AsString() == "x");
		CHECK(value->Find("missing") == nullptr);
		CHECK((*list)[0].Find("b") == nullptr);
		CHECK((*list)[0].AsString() == nullptr);
	}

	TEST_CASE("escapes are read, characters beyond the first plane included")
	{
		std::string error;
		const auto value = Json::Parse(R"("tab\t quote\" slash\/ e\u00e9 clef\ud834\udd1e")", error);
		REQUIRE_MESSAGE(value, error);
		CHECK(*value->AsString() == "tab\t quote\" slash/ e\xC3\xA9 clef\xF0\x9D\x84\x9E");
	}

	TEST_CASE("a bad text fails with where and why")
	{
		const auto fails = [](std::string_view p_text, std::string_view p_expected)
		{
			std::string error;
			const auto value = Json::Parse(p_text, error);
			CHECK_FALSE(value);
			CHECK_MESSAGE(error.find(p_expected) != std::string::npos, p_text << " -> " << error);
		};

		fails("", "unexpected end");
		fails("{\"a\": 1,}", "line 1, column 9: expected a member name");
		fails("[1 2]", "expected ',' or ']'");
		fails("{\"a\" 1}", "expected ':'");
		fails("\"\\ud834\"", "a lone surrogate");
		fails("\"\\q\"", "an unknown escape");
		fails("\"a\nb\"", "a control character");
		fails("01", "unexpected character after the value");
		fails("1.", "expected a digit");
		fails("1e999", "a number out of range");
		fails("nul", "unexpected character");
		fails("[1]\n  x", "line 2, column 3: unexpected character after the value");
		fails("\"\xC0\xAF\"", "not valid UTF-8");
	}

	TEST_CASE("nesting deeper than the limit fails")
	{
		std::string error;
		CHECK(Json::Parse("[[[1]]]", error, 3));
		CHECK_FALSE(Json::Parse("[[[[1]]]]", error, 3));
		CHECK(error.find("nested too deep") != std::string::npos);
	}

	TEST_CASE("a value written reads back as the same value")
	{
		const Json::Value value(Json::Object{
			{ "name", Json::Value("Jump \"high\"\n\x01") },
			{ "inputs", Json::Value(Json::Array{ Json::Value("SPACE"), Json::Value(""), Json::Value(nullptr) }) },
			{ "count", Json::Value(3) },
			{ "scale", Json::Value(0.1) },
			{ "empty", Json::Value(Json::Object{}) },
			{ "none", Json::Value(Json::Array{}) },
			{ "on", Json::Value(true) },
		});

		const std::string text = Json::Write(value);
		CHECK(text.find("\"count\": 3") != std::string::npos);
		CHECK(text.find("\\u0001") != std::string::npos);

		std::string error;
		const auto read = Json::Parse(text, error);
		REQUIRE_MESSAGE(read, error);
		CHECK(*read == value);

		// A number that is no number is written as null
		CHECK(Json::Write(Json::Value(std::nan(""))) == "null\n");
	}
}
