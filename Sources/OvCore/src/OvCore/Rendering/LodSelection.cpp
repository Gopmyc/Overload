/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <format>

#include <OvCore/Rendering/LodSelection.h>
#include <OvMaths/FVector4.h>

uint32_t OvCore::Rendering::LodSelection::SelectLevel(float p_distance, std::span<const float> p_starts, float p_bias, uint32_t p_previous, float p_margin)
{
	const auto count = static_cast<uint32_t>(std::min<size_t>(p_starts.size() + 1, kMaxLevels));
	if (count <= 1)
	{
		return 0;
	}

	const float bias = p_bias > 0.0f ? p_bias : 1.0f;

	// Level n ends where level n + 1 starts
	const auto boundary = [&](uint32_t p_level) { return p_starts[p_level] * bias; };

	uint32_t level = std::min(p_previous, count - 1);
	while (level + 1 < count && p_distance > boundary(level) * (1.0f + p_margin))
	{
		++level;
	}
	while (level > 0 && p_distance < boundary(level - 1) * (1.0f - p_margin))
	{
		--level;
	}
	return level;
}

float OvCore::Rendering::LodSelection::DistanceToBounds(const OvMaths::FVector3& p_point, const OvMaths::FMatrix4& p_world, const OvRendering::Geometry::BoundingSphere& p_bounds)
{
	const auto centre = p_world * OvMaths::FVector4{ p_bounds.position.x, p_bounds.position.y, p_bounds.position.z, 1.0f };
	return OvMaths::FVector3::Distance(p_point, { centre.x, centre.y, centre.z });
}

std::optional<std::string> OvCore::Rendering::LodSelection::CompareBones(const std::optional<OvRendering::Animation::Skeleton>& p_model, const std::optional<OvRendering::Animation::Skeleton>& p_level)
{
	const bool modelHasBones = p_model && p_model->IsValid();
	const bool levelHasBones = p_level && p_level->IsValid();

	if (!modelHasBones)
	{
		return std::nullopt;
	}

	if (!levelHasBones)
	{
		return std::string{ "it has no bones, and the model has" };
	}

	if (p_level->bones.size() != p_model->bones.size())
	{
		return std::format("it has {} bones, and the model {}", p_level->bones.size(), p_model->bones.size());
	}

	for (size_t i = 0; i < p_model->bones.size(); ++i)
	{
		if (p_level->bones[i].name != p_model->bones[i].name)
		{
			return std::format("its bone {} is '{}', and the model's '{}'", i, p_level->bones[i].name, p_model->bones[i].name);
		}
	}

	return std::nullopt;
}
