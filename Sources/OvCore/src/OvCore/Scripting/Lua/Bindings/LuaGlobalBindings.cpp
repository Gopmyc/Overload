/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <OvDebug/Logger.h>
#include <OvMaths/FVector2.h>
#include <OvMaths/FVector3.h>
#include <OvTools/Time/Clock.h>
#include <OvTools/Utils/PathParser.h>
#include <OvTools/Utils/Random.h>


#include "OvCore/ECS/Actor.h"
#include "OvCore/ECS/PhysicsWrapper.h"
#include "OvCore/Global/QuitRequest.h"
#include "OvCore/Global/ServiceLocator.h"
#include "OvCore/Helpers/InputHelpers.h"
#include "OvCore/SceneSystem/SceneManager.h"
#include "OvCore/ResourceManagement/ModelManager.h"
#include "OvCore/ResourceManagement/ShaderManager.h"
#include "OvCore/ResourceManagement/TextureManager.h"
#include "OvCore/ResourceManagement/MaterialManager.h"
#include "OvCore/ResourceManagement/SoundManager.h"
#include "OvCore/Scripting/Common/ScriptPropertyValue.h"
#include "OvCore/Scripting/ScriptEngine.h"

#include <OvPhysics/Core/PhysicsEngine.h>
#include <OvPhysics/Entities/PhysicalObject.h>
#include <OvPhysics/Settings/PhysicsSettings.h>

#include <OvRendering/Geometry/Vertex.h>
#include <OvRendering/Resources/Mesh.h>

#include <OvWindowing/Cursor/ECursorShape.h>
#include <OvWindowing/Inputs/InputManager.h>

#include <sol/sol.hpp>

namespace
{
	using Results = std::tuple<sol::object, sol::object>;

	/**
	* Reads the flat array of numbers p_name of p_geometry: nothing when it is absent, or an error when it is not an
	* array of finite numbers whose size is p_valuesPerVertex * p_vertexCount (any multiple of p_valuesPerVertex when
	* p_vertexCount is 0)
	*/
	std::optional<std::string> ReadNumbers(
		const sol::table& p_geometry,
		const char* p_name,
		size_t p_valuesPerVertex,
		size_t p_vertexCount,
		std::vector<float>& p_values
	)
	{
		const sol::object array = p_geometry.raw_get<sol::object>(p_name);

		if (array.get_type() == sol::type::lua_nil)
		{
			return std::nullopt;
		}

		if (array.get_type() != sol::type::table)
		{
			return std::format("{} must be an array of numbers", p_name);
		}

		const sol::table table = array.as<sol::table>();
		const size_t size = table.size();

		if (size == 0 || size % p_valuesPerVertex != 0 || (p_vertexCount != 0 && size != p_valuesPerVertex * p_vertexCount))
		{
			return p_vertexCount != 0 ?
				std::format("{} must hold {} numbers ({} per vertex), not {}", p_name, p_valuesPerVertex * p_vertexCount, p_valuesPerVertex, size) :
				std::format("{} must hold a multiple of {} numbers, not {}", p_name, p_valuesPerVertex, size);
		}

		p_values.resize(size);

		for (size_t index = 0; index < size; ++index)
		{
			const auto value = table.raw_get<sol::optional<double>>(index + 1);
			if (!value || !std::isfinite(*value))
			{
				return std::format("{}[{}] is not a finite number", p_name, index + 1);
			}

			p_values[index] = static_cast<float>(*value);
		}

		return std::nullopt;
	}

	/**
	* Model(), or nil and why: one mesh from flat arrays (Positions and Indices required, Normals, TexCoords, Tangents
	* and Bitangents optional), registered under the path or replacing the meshes of the model made there before
	*/
	Results CreateModel(sol::this_state p_state, const sol::object& p_path, const sol::object& p_geometry, const sol::object& p_materialName)
	{
		using namespace OvRendering;
		using OvMaths::FVector3;

		const auto failure = [&p_state](const std::string& p_error) -> Results {
			return { sol::make_object(p_state, sol::lua_nil), sol::make_object(p_state, p_error) };
		};

		if (p_path.get_type() != sol::type::string || p_path.as<std::string>().empty())
		{
			return failure("the path must be a non-empty string");
		}

		if (p_geometry.get_type() != sol::type::table)
		{
			return failure("the geometry must be a table");
		}

		if (p_materialName.get_type() != sol::type::lua_nil && p_materialName.get_type() != sol::type::string)
		{
			return failure("the material name must be a string");
		}

		const sol::table geometry = p_geometry.as<sol::table>();
		std::vector<float> positions, indexValues, normals, texCoords, tangents, bitangents;

		if (auto error = ReadNumbers(geometry, "Positions", 3, 0, positions))
		{
			return failure(*error);
		}

		if (auto error = ReadNumbers(geometry, "Indices", 3, 0, indexValues))
		{
			return failure(*error);
		}

		if (positions.empty() || indexValues.empty())
		{
			return failure("Positions and Indices are required");
		}

		const size_t vertexCount = positions.size() / 3;

		if (vertexCount > std::numeric_limits<uint32_t>::max())
		{
			return failure("too many vertices");
		}

		for (const auto& [name, valuesPerVertex, values] : {
			std::tuple{ "Normals", size_t{ 3 }, &normals },
			std::tuple{ "TexCoords", size_t{ 2 }, &texCoords },
			std::tuple{ "Tangents", size_t{ 3 }, &tangents },
			std::tuple{ "Bitangents", size_t{ 3 }, &bitangents }
		})
		{
			if (auto error = ReadNumbers(geometry, name, valuesPerVertex, vertexCount, *values))
			{
				return failure(*error);
			}
		}

		std::vector<uint32_t> indices(indexValues.size());

		for (size_t index = 0; index < indexValues.size(); ++index)
		{
			const float value = indexValues[index];
			if (value < 0.0f || value != std::floor(value) || value >= static_cast<float>(vertexCount))
			{
				return failure(std::format("Indices[{}] is not the index of a vertex (from 0 to {})", index + 1, vertexCount - 1));
			}

			indices[index] = static_cast<uint32_t>(value);
		}

		std::vector<Geometry::Vertex> vertices(vertexCount, Geometry::Vertex{});

		for (size_t vertex = 0; vertex < vertexCount; ++vertex)
		{
			std::copy_n(&positions[vertex * 3], 3, vertices[vertex].position);

			if (!normals.empty())
			{
				std::copy_n(&normals[vertex * 3], 3, vertices[vertex].normals);
			}

			if (!texCoords.empty())
			{
				std::copy_n(&texCoords[vertex * 2], 2, vertices[vertex].texCoords);
			}

			if (!tangents.empty())
			{
				std::copy_n(&tangents[vertex * 3], 3, vertices[vertex].tangent);
			}

			if (!bitangents.empty())
			{
				std::copy_n(&bitangents[vertex * 3], 3, vertices[vertex].bitangent);
			}
		}

		// Missing normals are smoothed: each vertex takes the sum of its triangles' normals, weighted by their area
		if (normals.empty())
		{
			std::vector<FVector3> sums(vertexCount, FVector3::Zero);

			for (size_t index = 0; index + 2 < indices.size(); index += 3)
			{
				const auto position = [&vertices](uint32_t p_vertex) {
					const auto& vertex = vertices[p_vertex].position;
					return FVector3{ vertex[0], vertex[1], vertex[2] };
				};

				const FVector3 a = position(indices[index]);
				const FVector3 normal = FVector3::Cross(position(indices[index + 1]) - a, position(indices[index + 2]) - a);

				for (size_t corner = 0; corner < 3; ++corner)
				{
					sums[indices[index + corner]] += normal;
				}
			}

			for (size_t vertex = 0; vertex < vertexCount; ++vertex)
			{
				const float length = FVector3::Length(sums[vertex]);
				const FVector3 normal = length > 0.0f ? sums[vertex] / length : FVector3::Up;
				vertices[vertex].normals[0] = normal.x;
				vertices[vertex].normals[1] = normal.y;
				vertices[vertex].normals[2] = normal.z;
			}
		}

		const std::string materialName = p_materialName.get_type() == sol::type::string ? p_materialName.as<std::string>() : "Material";
		auto* mesh = new Resources::Mesh(vertices, indices, 0);
		auto* model = OVSERVICE(OvCore::ResourceManagement::ModelManager).CreateFromGeometry(p_path.as<std::string>(), { mesh }, { materialName });

		if (!model)
		{
			delete mesh;
			return failure("a model loaded from a file already has this path");
		}

		return { sol::make_object(p_state, model), sol::make_object(p_state, sol::lua_nil) };
	}
}

void BindLuaGlobal(sol::state& p_luaState)
{
	using namespace OvWindowing;
	using namespace OvWindowing::Inputs;
	using namespace OvMaths;
	using namespace OvCore::Scripting;
	using namespace OvCore::ECS;
	using namespace OvCore::SceneSystem;
	using namespace OvCore::ResourceManagement;

	// Asset reference type, used in script local tables to declare asset properties.
	// The inspector renders an asset picker for any field initialised with one of the
	// factory functions below (Model(), Texture(), Shader(), Material(), Sound(), Prefab()).
	p_luaState.new_usertype<AssetRef>("AssetRef",
		"path", &AssetRef::path
	);

	using EFT = OvTools::Utils::PathParser::EFileType;
	p_luaState["Model"]    = []() { return AssetRef{EFT::MODEL,    ""}; };
	p_luaState["Texture"]  = []() { return AssetRef{EFT::TEXTURE,  ""}; };
	p_luaState["Shader"]   = []() { return AssetRef{EFT::SHADER,   ""}; };
	p_luaState["Material"] = []() { return AssetRef{EFT::MATERIAL, ""}; };
	p_luaState["Sound"]    = []() { return AssetRef{EFT::SOUND,    ""}; };
	p_luaState["Prefab"]   = []() { return AssetRef{EFT::PREFAB,   ""}; };

	p_luaState.new_usertype<Scene>("Scene",
		"FindActorByName", [](Scene& s, const std::string& name) -> std::optional<ActorRef> {
			if (auto* actor = s.FindActorByName(name))
				return ActorRef{actor->GetGUID()};
			return std::nullopt;
		},
		"FindActorByTag", [](Scene& s, const std::string& tag) -> std::optional<ActorRef> {
			if (auto* actor = s.FindActorByTag(tag))
				return ActorRef{actor->GetGUID()};
			return std::nullopt;
		},
		"FindActorsByName", [](Scene& s, const std::string& name) {
			std::vector<ActorRef> result;
			for (auto& actor : s.FindActorsByName(name))
				result.push_back(ActorRef{actor.get().GetGUID()});
			return sol::as_table(std::move(result));
		},
		"FindActorsByTag", [](Scene& s, const std::string& tag) {
			std::vector<ActorRef> result;
			for (auto& actor : s.FindActorsByTag(tag))
				result.push_back(ActorRef{actor.get().GetGUID()});
			return sol::as_table(std::move(result));
		},
		"CreateActor", sol::overload(
			[](Scene& s) -> ActorRef {
				auto& actor = s.CreateActor();
				return ActorRef{actor.GetGUID()};
			},
			[](Scene& s, const std::string& name, const std::string& tag) -> ActorRef {
				auto& actor = s.CreateActor(name, tag);
				return ActorRef{actor.GetGUID()};
			}
		),
		"InstantiatePrefab", sol::overload(
			[](Scene& s, const AssetRef& prefab) -> std::optional<ActorRef> {
				if (auto* actor = s.InstantiatePrefab(prefab.path))
					return ActorRef{actor->GetGUID()};
				return std::nullopt;
			},
			[](Scene& s, const AssetRef& prefab, ActorRef& parent) -> std::optional<ActorRef> {
				if (auto* actor = s.InstantiatePrefab(prefab.path, parent.Resolve()))
					return ActorRef{actor->GetGUID()};
				return std::nullopt;
			}
		)
	);

	

	p_luaState.new_enum<EKey>("Key", {
		{"UNKNOWN",			EKey::KEY_UNKNOWN},
		{"SPACE",			EKey::KEY_SPACE},
		{"APOSTROPHE",		EKey::KEY_APOSTROPHE},
		{"COMMA",			EKey::KEY_COMMA},
		{"MINUS",			EKey::KEY_MINUS},
		{"PERIOD",			EKey::KEY_PERIOD},
		{"SLASH",			EKey::KEY_SLASH},
		{"ALPHA_0",			EKey::KEY_0},
		{"ALPHA_1",			EKey::KEY_1},
		{"ALPHA_2",			EKey::KEY_2},
		{"ALPHA_3",			EKey::KEY_3},
		{"ALPHA_4",			EKey::KEY_4},
		{"ALPHA_5",			EKey::KEY_5},
		{"ALPHA_6",			EKey::KEY_6},
		{"ALPHA_7",			EKey::KEY_7},
		{"ALPHA_8",			EKey::KEY_8},
		{"ALPHA_9",			EKey::KEY_9},
		{"SEMICOLON",		EKey::KEY_SEMICOLON},
		{"EQUAL",			EKey::KEY_EQUAL},
		{"A",				EKey::KEY_A},
		{"B",				EKey::KEY_B},
		{"C",				EKey::KEY_C},
		{"D",				EKey::KEY_D},
		{"E",				EKey::KEY_E},
		{"F",				EKey::KEY_F},
		{"G",				EKey::KEY_G},
		{"H",				EKey::KEY_H},
		{"I",				EKey::KEY_I},
		{"J",				EKey::KEY_J},
		{"K",				EKey::KEY_K},
		{"L",				EKey::KEY_L},
		{"M",				EKey::KEY_M},
		{"N",				EKey::KEY_N},
		{"O",				EKey::KEY_O},
		{"P",				EKey::KEY_P},
		{"Q",				EKey::KEY_Q},
		{"R",				EKey::KEY_R},
		{"S",				EKey::KEY_S},
		{"T",				EKey::KEY_T},
		{"U",				EKey::KEY_U},
		{"V",				EKey::KEY_V},
		{"W",				EKey::KEY_W},
		{"X",				EKey::KEY_X},
		{"Y",				EKey::KEY_Y},
		{"Z",				EKey::KEY_Z},
		{"LEFT_BRACKET",	EKey::KEY_LEFT_BRACKET},
		{"BACKSLASH",		EKey::KEY_BACKSLASH},
		{"RIGHT_BRACKET",	EKey::KEY_RIGHT_BRACKET},
		{"GRAVE_ACCENT",	EKey::KEY_GRAVE_ACCENT},
		{"WORLD_1",			EKey::KEY_WORLD_1},
		{"WORLD_2",			EKey::KEY_WORLD_2},
		{"ESCAPE",			EKey::KEY_ESCAPE},
		{"ENTER",			EKey::KEY_ENTER},
		{"TAB",				EKey::KEY_TAB},
		{"BACKSPACE",		EKey::KEY_BACKSPACE},
		{"INSERT",			EKey::KEY_INSERT},
		{"DELETE",			EKey::KEY_DELETE},
		{"RIGHT",			EKey::KEY_RIGHT},
		{"LEFT",			EKey::KEY_LEFT},
		{"DOWN",			EKey::KEY_DOWN},
		{"UP",				EKey::KEY_UP},
		{"PAGE_UP",			EKey::KEY_PAGE_UP},
		{"PAGE_DOWN",		EKey::KEY_PAGE_DOWN},
		{"HOME",			EKey::KEY_HOME},
		{"END",				EKey::KEY_END},
		{"CAPS_LOCK",		EKey::KEY_CAPS_LOCK},
		{"SCROLL_LOCK",		EKey::KEY_SCROLL_LOCK},
		{"NUM_LOCK",		EKey::KEY_NUM_LOCK},
		{"PRINT_SCREEN",	EKey::KEY_PRINT_SCREEN},
		{"PAUSE",			EKey::KEY_PAUSE},
		{"F1",				EKey::KEY_F1},
		{"F2",				EKey::KEY_F2},
		{"F3",				EKey::KEY_F3},
		{"F4",				EKey::KEY_F4},
		{"F5",				EKey::KEY_F5},
		{"F6",				EKey::KEY_F6},
		{"F7",				EKey::KEY_F7},
		{"F8",				EKey::KEY_F8},
		{"F9",				EKey::KEY_F9},
		{"F10",				EKey::KEY_F10},
		{"F11",				EKey::KEY_F11},
		{"F12",				EKey::KEY_F12},
		{"F13",				EKey::KEY_F13},
		{"F14",				EKey::KEY_F14},
		{"F15",				EKey::KEY_F15},
		{"F16",				EKey::KEY_F16},
		{"F17",				EKey::KEY_F17},
		{"F18",				EKey::KEY_F18},
		{"F19",				EKey::KEY_F19},
		{"F20",				EKey::KEY_F20},
		{"F21",				EKey::KEY_F21},
		{"F22",				EKey::KEY_F22},
		{"F23",				EKey::KEY_F23},
		{"F24",				EKey::KEY_F24},
		{"F25",				EKey::KEY_F25},
		{"KP_0",			EKey::KEY_KP_0},
		{"KP_1",			EKey::KEY_KP_1},
		{"KP_2",			EKey::KEY_KP_2},
		{"KP_3",			EKey::KEY_KP_3},
		{"KP_4",			EKey::KEY_KP_4},
		{"KP_5",			EKey::KEY_KP_5},
		{"KP_6",			EKey::KEY_KP_6},
		{"KP_7",			EKey::KEY_KP_7},
		{"KP_8",			EKey::KEY_KP_8},
		{"KP_9",			EKey::KEY_KP_9},
		{"KP_DECIMAL",		EKey::KEY_KP_DECIMAL},
		{"KP_DIVIDE",		EKey::KEY_KP_DIVIDE},
		{"KP_MULTIPLY",		EKey::KEY_KP_MULTIPLY},
		{"KP_SUBTRACT",		EKey::KEY_KP_SUBTRACT},
		{"KP_ADD",			EKey::KEY_KP_ADD},
		{"KP_ENTER",		EKey::KEY_KP_ENTER},
		{"KP_EQUAL",		EKey::KEY_KP_EQUAL},
		{"LEFT_SHIFT",		EKey::KEY_LEFT_SHIFT},
		{"LEFT_CONTROL",	EKey::KEY_LEFT_CONTROL},
		{"LEFT_ALT",		EKey::KEY_LEFT_ALT},
		{"LEFT_SUPER",		EKey::KEY_LEFT_SUPER},
		{"RIGHT_SHIFT",		EKey::KEY_RIGHT_SHIFT},
		{"RIGHT_CONTROL",	EKey::KEY_RIGHT_CONTROL},
		{"RIGHT_ALT",		EKey::KEY_RIGHT_ALT},
		{"RIGHT_SUPER",		EKey::KEY_RIGHT_SUPER},
		{"MENU",			EKey::KEY_MENU}
	});

	p_luaState.new_enum<EMouseButton>("MouseButton", {
		{"BUTTON_1",		EMouseButton::MOUSE_BUTTON_1},
		{"BUTTON_2",		EMouseButton::MOUSE_BUTTON_2},
		{"BUTTON_3",		EMouseButton::MOUSE_BUTTON_3},
		{"BUTTON_4",		EMouseButton::MOUSE_BUTTON_4},
		{"BUTTON_5",		EMouseButton::MOUSE_BUTTON_5},
		{"BUTTON_6",		EMouseButton::MOUSE_BUTTON_6},
		{"BUTTON_7",		EMouseButton::MOUSE_BUTTON_7},
		{"BUTTON_8",		EMouseButton::MOUSE_BUTTON_8},
		{"BUTTON_LEFT",		EMouseButton::MOUSE_BUTTON_LEFT},
		{"BUTTON_RIGHT",	EMouseButton::MOUSE_BUTTON_RIGHT},
		{"BUTTON_MIDDLE",	EMouseButton::MOUSE_BUTTON_MIDDLE},
	});

	p_luaState.new_enum<Cursor::ECursorShape>("CursorShape", {
		{"ARROW",		Cursor::ECursorShape::ARROW},
		{"IBEAM",		Cursor::ECursorShape::IBEAM},
		{"CROSSHAIR",	Cursor::ECursorShape::CROSSHAIR},
		{"HAND",		Cursor::ECursorShape::HAND},
		{"HRESIZE",		Cursor::ECursorShape::HRESIZE},
		{"VRESIZE",		Cursor::ECursorShape::VRESIZE}
	});

	p_luaState.create_named_table("Debug",
		"Log", [](const std::string& p_message) { OVLOG(p_message); },
		"LogInfo", [](const std::string& p_message) { OVLOG_INFO(p_message); },
		"LogWarning", [](const std::string& p_message) { OVLOG_WARNING(p_message); },
		"LogError", [](const std::string& p_message) { OVLOG_ERROR(p_message); }
	);

	p_luaState.create_named_table("Inputs",
		"GetKeyDown", [](EKey p_key) { return OvCore::Helpers::InputHelpers::GetKeyDown(p_key); },
		"GetKeyUp", [](EKey p_key) { return OvCore::Helpers::InputHelpers::GetKeyUp(p_key); },
		"GetKey", [](EKey p_key) { return OvCore::Helpers::InputHelpers::GetKey(p_key); },
		"GetKeyName", [](EKey p_key) { return OVSERVICE(InputManager).GetKeyName(p_key); },
		"GetMouseButtonDown", [](EMouseButton p_button) { return OvCore::Helpers::InputHelpers::GetMouseButtonDown(p_button); },
		"GetMouseButtonUp", [](EMouseButton p_button) { return OvCore::Helpers::InputHelpers::GetMouseButtonUp(p_button); },
		"GetMouseButton", [](EMouseButton p_button) { return OvCore::Helpers::InputHelpers::GetMouseButton(p_button); },
		"GetMousePos", []() { return OvCore::Helpers::InputHelpers::GetMousePosition(); },
		"GetViewportSize", []() { return OvCore::Helpers::InputHelpers::GetViewportSize(); },
		"GetMouseScroll", []() { return OvCore::Helpers::InputHelpers::GetMouseScroll(); },
		"LockMouse", []() { OvCore::Helpers::InputHelpers::SetMouseLocked(true); },
		"UnlockMouse", []() { OvCore::Helpers::InputHelpers::SetMouseLocked(false); },
		"SetCursorShape", [](Cursor::ECursorShape p_shape) { OvCore::Helpers::InputHelpers::SetCursorShape(p_shape); }
	);

	p_luaState.create_named_table("Scenes",
		"GetCurrentScene", []() -> Scene& { return *OVSERVICE(SceneManager).GetCurrentScene(); },
		"Load", [](const std::string& p_path) { OVSERVICE(SceneManager).LoadAndPlayDelayed(p_path); }
	);

	p_luaState.create_named_table("Resources",
		"GetModel", [](const std::string& p_resPath) { return OVSERVICE(ModelManager).GetResource(p_resPath); },
		"GetShader", [](const std::string& p_resPath) { return OVSERVICE(ShaderManager).GetResource(p_resPath); },
		"GetTexture", [](const std::string& p_resPath) { return OVSERVICE(TextureManager).GetResource(p_resPath); },
		"GetMaterial", [](const std::string& p_resPath) { return OVSERVICE(MaterialManager).GetResource(p_resPath); },
		"GetSound", [](const std::string& p_resPath) { return OVSERVICE(SoundManager).GetResource(p_resPath); },
		"GetScript", [](const std::string& p_resPath) { return OVSERVICE(ScriptEngine).GetScript(p_resPath); },
		"CreateModel", &CreateModel
	);

	p_luaState.create_named_table("Math",
		"RandomInt", [](int p_min, int p_max) { return OvTools::Utils::Random::Generate(p_min, p_max); },
		"RandomFloat", [](float p_min, float p_max) { return OvTools::Utils::Random::Generate(p_min, p_max); },
		"CheckPercentage", [](float p_percentage) { return OvTools::Utils::Random::CheckPercentage(p_percentage); },
		"Lerp", [](float a, float b, float t) -> float { return a + t * (b - a); }
	);

	p_luaState.new_usertype<PhysicsWrapper::RaycastHit>("RaycastHit",
		"FirstResultObject", &PhysicsWrapper::RaycastHit::FirstResultObject,
		"ResultObjects", &PhysicsWrapper::RaycastHit::ResultObjects
	);

	// A layer from Lua is an index (0 to PhysicsSettings::kLayerCount - 1), as in the files and the project settings
	const auto isLayer = [](int p_layer)
	{
		return p_layer >= 0 && p_layer < OvPhysics::Settings::PhysicsSettings::kLayerCount;
	};

	p_luaState.create_named_table("Physics",
		"Raycast", [](const OvMaths::FVector3& p_origin, const OvMaths::FVector3& p_direction, float p_distance) { return PhysicsWrapper::Raycast(p_origin, p_direction, p_distance); },
		"GetLayerIndex", [](const std::string& p_name) -> std::optional<int>
		{
			if (const auto layer = OVSERVICE(OvPhysics::Core::PhysicsEngine).FindLayer(p_name))
			{
				return static_cast<int>(*layer);
			}

			return std::nullopt;
		},
		"GetLayerName", [isLayer](int p_layer) -> std::string
		{
			return isLayer(p_layer) ? OVSERVICE(OvPhysics::Core::PhysicsEngine).GetLayerName(static_cast<uint16_t>(p_layer)) : std::string{};
		},
		"DoLayersCollide", [isLayer](int p_layer1, int p_layer2)
		{
			return isLayer(p_layer1) && isLayer(p_layer2) &&
				OVSERVICE(OvPhysics::Core::PhysicsEngine).DoLayersCollide(static_cast<uint16_t>(p_layer1), static_cast<uint16_t>(p_layer2));
		},
		"SetLayersCollide", [isLayer](int p_layer1, int p_layer2, bool p_collide)
		{
			if (!isLayer(p_layer1) || !isLayer(p_layer2))
			{
				OVLOG_WARNING(std::format("Physics.SetLayersCollide: no layer {} or {}", p_layer1, p_layer2));
				return;
			}

			OVSERVICE(OvPhysics::Core::PhysicsEngine).SetLayersCollide(static_cast<uint16_t>(p_layer1), static_cast<uint16_t>(p_layer2), p_collide);
		}
	);

	p_luaState.create_named_table("Time",
		"GetScale", []() { return OVSERVICE(OvTools::Time::Clock).GetTimeScale(); },
		"SetScale", [](float p_scale) { OVSERVICE(OvTools::Time::Clock).SetTimeScale(std::max(p_scale, 0.0f)); },
		"GetUnscaledDeltaTime", []() { return OVSERVICE(OvTools::Time::Clock).GetDeltaTimeUnscaled(); },
		"GetGameTime", []() { return OVSERVICE(OvTools::Time::Clock).GetTimeSinceStart(); }
	);

	p_luaState.create_named_table("Application",
		"Quit", []() { OvCore::Global::QuitRequest::RequestedEvent.Invoke(); },
		"GetEngineVersion", []() { return std::string(ADVENTURER_VERSION); }
	);
}
