/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cmath>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <tuple>

#include <OvCore/Scripting/Lua/LuaComponentRef.h>

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Navigation/NavMesh.h>
#include <OvCore/Navigation/SceneGeometry.h>
#include <OvCore/SceneSystem/SceneManager.h>

namespace
{
	using OvCore::Navigation::NavMesh;
	using OvCore::Navigation::NavMeshSettings;
	using OvMaths::FVector3;

	OvCore::SceneSystem::Scene& CurrentScene(const char* p_function)
	{
		if (auto* scene = OvCore::Global::ServiceLocator::Get<OvCore::SceneSystem::SceneManager>().GetCurrentScene())
		{
			return *scene;
		}

		throw sol::error(std::format("Navigation.{}: no scene is loaded", p_function));
	}

	/**
	* The settings given to Build, over the defaults: a key it does not know raises an error, a misspelt key would
	* otherwise be silently dropped
	*/
	NavMeshSettings ReadSettings(const sol::optional<sol::table>& p_table)
	{
		NavMeshSettings settings;

		if (!p_table)
		{
			return settings;
		}

		for (const auto& [key, value] : *p_table)
		{
			const std::string name = key.is<std::string>() ? key.as<std::string>() : std::string{};

			if (value.get_type() != sol::type::number)
			{
				throw sol::error(std::format("Navigation.Build: setting '{}' must be a number", name));
			}

			const double number = value.as<double>();
			const auto whole = [&](double p_max) {
				if (!std::isfinite(number) || number < 0.0 || std::floor(number) != number || number > p_max)
				{
					throw sol::error(std::format("Navigation.Build: setting '{}' must be a whole number from 0 to {}", name, p_max));
				}
				return static_cast<uint32_t>(number);
			};

			if (name == "agentRadius") settings.agentRadius = static_cast<float>(number);
			else if (name == "agentHeight") settings.agentHeight = static_cast<float>(number);
			else if (name == "agentMaxClimb") settings.agentMaxClimb = static_cast<float>(number);
			else if (name == "agentMaxSlope") settings.agentMaxSlope = static_cast<float>(number);
			else if (name == "cellSize") settings.cellSize = static_cast<float>(number);
			else if (name == "cellHeight") settings.cellHeight = static_cast<float>(number);
			else if (name == "tileSize") settings.tileSize = whole(256.0);
			else if (name == "layerMask") settings.layerMask = whole(65535.0);
			else
			{
				throw sol::error(std::format(
					"Navigation.Build: unknown setting '{}' (agentRadius, agentHeight, agentMaxClimb, agentMaxSlope, cellSize, cellHeight, tileSize, layerMask)",
					name
				));
			}
		}

		return settings;
	}

	std::tuple<bool, sol::optional<std::string>> Result(const std::optional<std::string>& p_error)
	{
		if (p_error)
		{
			return { false, *p_error };
		}

		return { true, sol::nullopt };
	}
}

void BindLuaNavigation(sol::state& p_luaState)
{
	p_luaState.create_named_table("Navigation",
		"Build", [](sol::optional<sol::table> p_settings)
		{
			const NavMeshSettings settings = NavMesh::Sanitize(ReadSettings(p_settings));
			auto& scene = CurrentScene("Build");
			const auto geometry = OvCore::Navigation::SceneGeometry::Collect(scene, settings.layerMask);
			return Result(scene.GetNavigation().Build(geometry, settings));
		},
		"Rebuild", [](const FVector3& p_min, const FVector3& p_max)
		{
			auto& scene = CurrentScene("Rebuild");
			auto& navigation = scene.GetNavigation();

			if (!navigation.IsBuilt())
			{
				return Result(std::string("no navigation mesh is built: call Navigation.Build first"));
			}

			const auto geometry = OvCore::Navigation::SceneGeometry::Collect(scene, navigation.GetSettings().layerMask);
			return Result(navigation.RebuildTiles(geometry, p_min, p_max));
		},
		"Clear", []() { CurrentScene("Clear").GetNavigation().Clear(); },
		"IsBuilt", []() { return CurrentScene("IsBuilt").GetNavigation().IsBuilt(); },
		"GetSettings", [](sol::this_state p_state)
		{
			const auto& settings = CurrentScene("GetSettings").GetNavigation().GetSettings();
			sol::state_view lua(p_state);
			return lua.create_table_with(
				"agentRadius", settings.agentRadius,
				"agentHeight", settings.agentHeight,
				"agentMaxClimb", settings.agentMaxClimb,
				"agentMaxSlope", settings.agentMaxSlope,
				"cellSize", settings.cellSize,
				"cellHeight", settings.cellHeight,
				"tileSize", settings.tileSize,
				"layerMask", settings.layerMask
			);
		},
		"GetPolygonCount", []() { return CurrentScene("GetPolygonCount").GetNavigation().GetPolygonCount(); },
		"FindPath", [](const FVector3& p_from, const FVector3& p_to, sol::this_state p_state) -> std::tuple<sol::object, sol::object>
		{
			sol::state_view lua(p_state);
			auto& navigation = CurrentScene("FindPath").GetNavigation();

			if (!navigation.IsBuilt())
			{
				return { sol::lua_nil, sol::make_object(lua, "no navigation mesh is built") };
			}

			const auto path = navigation.FindPath(p_from, p_to);
			if (!path)
			{
				return { sol::lua_nil, sol::make_object(lua, "a point is too far from the navigation mesh") };
			}

			sol::table corners = lua.create_table(static_cast<int>(path->corners.size()), 0);
			for (const auto& corner : path->corners)
			{
				corners.add(corner);
			}

			return { sol::make_object(lua, corners), sol::make_object(lua, path->complete) };
		},
		"FindNearestPoint", [](const FVector3& p_point, sol::optional<FVector3> p_extents) -> std::optional<FVector3>
		{
			return CurrentScene("FindNearestPoint").GetNavigation().FindNearestPoint(p_point, p_extents.value_or(NavMesh::kDefaultExtents));
		},
		"Raycast", [](const FVector3& p_from, const FVector3& p_to, sol::this_state p_state) -> std::tuple<sol::object, sol::object, sol::object>
		{
			sol::state_view lua(p_state);
			const auto hit = CurrentScene("Raycast").GetNavigation().Raycast(p_from, p_to);

			if (!hit)
			{
				return { sol::lua_nil, sol::lua_nil, sol::lua_nil };
			}

			return { sol::make_object(lua, hit->hit), sol::make_object(lua, hit->point), sol::make_object(lua, hit->normal) };
		},
		"GetRandomPoint", [](sol::optional<FVector3> p_center, sol::optional<float> p_radius) -> std::optional<FVector3>
		{
			auto& navigation = CurrentScene("GetRandomPoint").GetNavigation();

			if (p_center)
			{
				if (!p_radius || !(*p_radius >= 0.0f))
				{
					throw sol::error("Navigation.GetRandomPoint: a center takes a radius, 0 or more");
				}

				return navigation.GetRandomPointAround(*p_center, *p_radius);
			}

			return navigation.GetRandomPoint();
		},
		"SetSeed", [](double p_seed)
		{
			if (!std::isfinite(p_seed) || p_seed < 0.0 || std::floor(p_seed) != p_seed || p_seed > static_cast<double>(std::numeric_limits<uint32_t>::max()))
			{
				throw sol::error("Navigation.SetSeed takes a whole number from 0 to 4294967295");
			}

			CurrentScene("SetSeed").GetNavigation().SetSeed(static_cast<uint64_t>(p_seed));
		},
		"GetDebugGeometry", [](sol::this_state p_state)
		{
			const auto geometry = CurrentScene("GetDebugGeometry").GetNavigation().GetDebugGeometry();
			sol::state_view lua(p_state);

			sol::table positions = lua.create_table(static_cast<int>(geometry.vertices.size()), 0);
			for (const float value : geometry.vertices)
			{
				positions.add(value);
			}

			sol::table indices = lua.create_table(static_cast<int>(geometry.indices.size()), 0);
			for (const int value : geometry.indices)
			{
				indices.add(value);
			}

			return lua.create_table_with("Positions", positions, "Indices", indices);
		}
	);
}
