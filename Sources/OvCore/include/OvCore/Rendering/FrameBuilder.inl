/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <ranges>

#include <OvCore/Rendering/FrameBuilder.h>

namespace OvCore::Rendering::FrameBuilder
{
	template<EOrderingMode OrderingMode, bool BatchMaterial>
	inline void Draw(
		OvRendering::Core::CompositeRenderer& p_renderer,
		OvRendering::Data::PipelineState p_pso,
		const FilteredDrawableMap<OrderingMode, BatchMaterial>& p_filteredDrawables,
		const PreparationInput& p_input
	)
	{
		for (const auto& filteredDrawable : p_filteredDrawables | std::views::values)
		{
			p_renderer.DrawEntity(p_pso, Prepare(filteredDrawable, p_input));
		}
	}
}
