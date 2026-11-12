/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cmath>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include <OvMaths/FVector3.h>

#include "OvCore/Helpers/DistanceField.h"

#include <sol/sol.hpp>

namespace
{
	using OvCore::Helpers::DistanceField;
	using OvCore::Helpers::MeshData;

	// true, or false and why: the ground is only set when every argument is valid
	std::tuple<bool, std::optional<std::string>> SetGround(
		DistanceField& p_field,
		const sol::object& p_count,
		float p_spacing,
		float p_originX,
		float p_originZ,
		const sol::object& p_heights,
		float p_cover,
		float p_sink
	)
	{
		const auto count = p_count.as<sol::optional<double>>();
		if (!count || *count != std::floor(*count) || *count < 2.0 || *count > 4096.0)
		{
			return { false, "the sample count must be a whole number from 2 to 4096" };
		}

		if (p_heights.get_type() != sol::type::table)
		{
			return { false, "the heights must be an array of numbers" };
		}

		const auto sampleCount = static_cast<uint32_t>(*count);
		const sol::table table = p_heights.as<sol::table>();
		const size_t expected = static_cast<size_t>(sampleCount) * sampleCount;

		if (table.size() != expected)
		{
			return { false, std::format("{} heights given for {} samples", table.size(), expected) };
		}

		std::vector<float> heights(expected);

		for (size_t index = 0; index < expected; ++index)
		{
			const auto height = table.raw_get<sol::optional<double>>(index + 1);
			if (!height || !std::isfinite(*height))
			{
				return { false, std::format("the height {} is not a finite number", index + 1) };
			}

			heights[index] = static_cast<float>(*height);
		}

		if (!p_field.SetGround(sampleCount, p_spacing, p_originX, p_originZ, std::move(heights), p_cover, p_sink))
		{
			return { false, "the spacing must be positive" };
		}

		return { true, std::nullopt };
	}

	// The mesh data and the samples (x, y, z, nx, ny, nz each), or nil and nil when no surface crosses the block
	std::tuple<sol::object, sol::object> Mesh(
		sol::this_state p_state,
		const DistanceField& p_field,
		int32_t p_x,
		int32_t p_y,
		int32_t p_z,
		uint32_t p_cells,
		float p_voxel,
		sol::optional<float> p_sampleSpacing
	)
	{
		std::vector<DistanceField::Sample> samples;
		const bool sampled = p_sampleSpacing.has_value() && *p_sampleSpacing > 0.0f;
		auto mesh = p_field.Mesh(p_x, p_y, p_z, p_cells, p_voxel, sampled ? &samples : nullptr, sampled ? *p_sampleSpacing : 0.0f);

		if (!mesh)
		{
			return { sol::make_object(p_state, sol::lua_nil), sol::make_object(p_state, sol::lua_nil) };
		}

		sol::state_view lua(p_state);
		sol::table points = lua.create_table(static_cast<int>(samples.size() * 6), 0);

		int index = 1;
		for (const auto& sample : samples)
		{
			points.raw_set(index, sample.position.x, index + 1, sample.position.y, index + 2, sample.position.z,
				index + 3, sample.normal.x, index + 4, sample.normal.y, index + 5, sample.normal.z);
			index += 6;
		}

		return { sol::make_object(p_state, std::make_shared<MeshData>(std::move(*mesh))), sol::make_object(p_state, points) };
	}
}

void BindLuaGeometry(sol::state& p_luaState)
{
	using OvMaths::FVector3;

	p_luaState.new_usertype<MeshData>("MeshData",
		sol::no_constructor,
		"GetVertexCount", [](const MeshData& p_mesh) { return p_mesh.vertices.size(); },
		"GetTriangleCount", [](const MeshData& p_mesh) { return p_mesh.indices.size() / 3; }
	);

	p_luaState.new_usertype<DistanceField>("DistanceField",
		sol::constructors<DistanceField()>(),
		"SetSeed", [](DistanceField& p_field, double p_seed) { p_field.SetSeed(static_cast<uint32_t>(static_cast<int64_t>(p_seed))); },
		"SetNoise", [](DistanceField& p_field, float p_amplitude, float p_frequency, sol::optional<uint32_t> p_octaves) { p_field.SetNoise(p_amplitude, p_frequency, p_octaves.value_or(2)); },
		"SetBlend", &DistanceField::SetBlend,
		"AddTube", [](DistanceField& p_field, const FVector3& p_from, const FVector3& p_to, float p_fromRadius, float p_toRadius, sol::optional<float> p_floor, sol::optional<float> p_squash) {
			p_field.AddTube(p_from, p_to, p_fromRadius, p_toRadius, p_floor.value_or(1.0f), p_squash.value_or(1.0f));
		},
		"AddEllipsoid", [](DistanceField& p_field, const FVector3& p_centre, const FVector3& p_radii, sol::optional<float> p_floor) {
			p_field.AddEllipsoid(p_centre, p_radii, p_floor.value_or(1.0f));
		},
		"AddPillar", &DistanceField::AddPillar,
		"SetGround", &SetGround,
		"AddOpening", &DistanceField::AddOpening,
		"AddLight", &DistanceField::AddLight,
		"GetValue", &DistanceField::GetValue,
		"FindFloor", [](const DistanceField& p_field, const FVector3& p_point, float p_depth) -> sol::optional<float> {
			const auto floor = p_field.FindFloor(p_point, p_depth);
			return floor ? sol::optional<float>(*floor) : sol::nullopt;
		},
		"GetGroundHeight", [](const DistanceField& p_field, float p_x, float p_z) -> sol::optional<float> {
			const auto height = p_field.GetGroundHeight(p_x, p_z);
			return height ? sol::optional<float>(*height) : sol::nullopt;
		},
		"Mesh", &Mesh
	);
}
