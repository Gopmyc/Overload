/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <fstream>
#include <iterator>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <doctest/doctest.h>

#include <OvTests/TestUtils.h>

namespace
{
	/**
	* The names a shader include declares at its top level: functions and prototypes, structs, uniforms, constants,
	* macros, and the fields of the engine buffer
	*/
	std::set<std::string> ReadDeclaredNames(const std::filesystem::path& p_include)
	{
		static const std::string kTypes = R"((?:void|bool|int|uint|float|double|[biud]?vec[234]|mat[234](?:x[234])?|sampler\w+|[A-Z]\w*))";
		static const std::vector<std::regex> kDeclarations = {
			std::regex(R"(^\s*)" + kTypes + R"(\s+([A-Za-z_]\w*)\s*\()"),
			std::regex(R"(^\s*struct\s+([A-Za-z_]\w*))"),
			std::regex(R"(^\s*uniform\s+\w+\s+([A-Za-z_]\w*)\s*[;=])"),
			std::regex(R"(^\s*const\s+\w+\s+([A-Za-z_]\w*)\s*=)"),
			std::regex(R"(^#define\s+([A-Za-z_]\w*))"),
		};
		static const std::regex kBufferField(R"(^\s+(?:int|uint|bool|float|[biud]?vec[234]|mat[234])\s+(ubo_\w+)\s*;)");

		std::set<std::string> names;
		std::ifstream file(p_include);
		std::string line;
		int depth = 0;

		while (std::getline(file, line))
		{
			const std::string code = line.substr(0, line.find("//"));
			std::smatch match;

			if (std::regex_search(code, match, kBufferField))
			{
				names.insert(match[1]);
			}
			else if (depth == 0)
			{
				for (const auto& declaration : kDeclarations)
				{
					if (std::regex_search(code, match, declaration))
					{
						if (match[1] != "main")
							names.insert(match[1]);
						break;
					}
				}
			}

			depth += static_cast<int>(std::count(code.begin(), code.end(), '{')) - static_cast<int>(std::count(code.begin(), code.end(), '}'));
		}

		return names;
	}

	struct DocumentedInclude
	{
		std::string path;
		std::set<std::string> names;
	};

	/**
	* The public includes the table of Docs/Shaders.md lists, each with the names it gives as public or internal
	*/
	std::vector<DocumentedInclude> ReadDocumentedIncludes(const std::filesystem::path& p_document)
	{
		static const std::regex kName("`([A-Za-z_]\\w*)");
		std::vector<DocumentedInclude> includes;
		std::ifstream file(p_document);
		std::string line;

		while (std::getline(file, line))
		{
			if (!line.starts_with("| `") || line.find(".ovfxh`") == std::string::npos)
				continue;

			std::vector<std::string> cells;
			std::stringstream row(line.substr(1));
			for (std::string cell; std::getline(row, cell, '|'); )
				cells.push_back(cell);

			DocumentedInclude include;
			include.path = cells[0].substr(cells[0].find('`') + 1);
			include.path = include.path.substr(0, include.path.find('`'));

			for (size_t column = 1; column < cells.size(); ++column)
				for (auto it = std::sregex_iterator(cells[column].begin(), cells[column].end(), kName); it != std::sregex_iterator(); ++it)
					include.names.insert((*it)[1]);

			includes.push_back(include);
		}

		return includes;
	}

	std::string Join(const std::vector<std::string>& p_names)
	{
		std::string text;
		for (const auto& name : p_names)
			text += (text.empty() ? "" : ", ") + name;
		return text;
	}
}

TEST_SUITE("Shader API")
{
	TEST_CASE("Docs/Shaders.md lists every name of the public includes, no more, no less")
	{
		const auto resources = OvTests::FindEngineResources();
		REQUIRE_MESSAGE(!resources.empty(), "Resources/Engine not found above the test program or the current folder");

		const auto document = resources.parent_path().parent_path() / "Docs" / "Shaders.md";
		REQUIRE(std::filesystem::exists(document));

		const auto includes = ReadDocumentedIncludes(document);
		CHECK(includes.size() >= 10);

		for (const auto& include : includes)
		{
			CAPTURE(include.path);
			const auto file = resources / "Shaders" / include.path;
			REQUIRE_MESSAGE(std::filesystem::exists(file), "no include " << include.path);

			const auto declared = ReadDeclaredNames(file);
			std::vector<std::string> undocumented, missing;
			std::set_difference(declared.begin(), declared.end(), include.names.begin(), include.names.end(), std::back_inserter(undocumented));
			std::set_difference(include.names.begin(), include.names.end(), declared.begin(), declared.end(), std::back_inserter(missing));

			CHECK_MESSAGE(undocumented.empty(), include.path << " declares, Docs/Shaders.md does not list: " << Join(undocumented));
			CHECK_MESSAGE(missing.empty(), "Docs/Shaders.md lists, " << include.path << " does not declare: " << Join(missing));
		}
	}

	TEST_CASE("the shader API version is defined once, for every shader")
	{
		const auto resources = OvTests::FindEngineResources();
		REQUIRE(!resources.empty());

		const auto version = ReadDeclaredNames(resources / "Shaders" / "Common" / "ShaderApi.ovfxh");
		CHECK(version == std::set<std::string>{ "SHADER_API_VERSION_MAJOR", "SHADER_API_VERSION_MINOR" });

		std::ifstream engineBuffer(resources / "Shaders" / "Common" / "Buffers" / "EngineUBO.ovfxh");
		std::string firstLine;
		std::getline(engineBuffer, firstLine);
		CHECK(firstLine == "#include \":Shaders/Common/ShaderApi.ovfxh\"");
	}
}
