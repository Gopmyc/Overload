/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <OvCore/Rendering/EVisibilityFlags.h>
#include <OvCore/SceneSystem/Scene.h>
#include <OvRendering/Core/CompositeRenderer.h>
#include <OvRendering/Data/Frustum.h>
#include <OvRendering/Data/Material.h>
#include <OvRendering/Entities/Camera.h>
#include <OvRendering/Entities/Drawable.h>
#include <OvTools/Utils/OptRef.h>

/**
* Stateless utilities building the drawables of a frame in four steps:
* - Parse: find the drawables of a scene (once per frame)
* - Filter: select and sort the parsed drawables for a given point of view
* - Prepare: create the drawable to submit for a given render pass
* - Draw: prepare and submit filtered drawables to a renderer
*/
namespace OvCore::Rendering::FrameBuilder
{
	enum class EOrderingMode
	{
		BACK_TO_FRONT,
		FRONT_TO_BACK,
	};

	template<EOrderingMode OrderingMode, bool BatchMaterial>
	struct DrawOrder
	{
		int order;
		uintptr_t materialKey;
		float distance;

		/**
		* Determines the order of the drawables.
		* @param p_other
		*/
		bool operator<(const DrawOrder& p_other) const
		{
			if (order == p_other.order)
			{
				if constexpr (BatchMaterial)
				{
					if (materialKey != p_other.materialKey)
					{
						return materialKey < p_other.materialKey;
					}
				}

				if constexpr (OrderingMode == EOrderingMode::BACK_TO_FRONT)
				{
					return distance > p_other.distance;
				}
				else
				{
					return distance < p_other.distance;
				}
			}
			else
			{
				return order < p_other.order;
			}
		}
	};

	/**
	* Parsed drawable selected by the filtering step, along with its resolved material.
	*/
	struct FilteredDrawable
	{
		std::reference_wrapper<const OvRendering::Entities::Drawable> drawable;
		std::reference_wrapper<OvRendering::Data::Material> material;
	};

	/**
	* Filtered drawables sorted by draw order (drawables with an equal order keep their insertion order).
	* A sorted vector is used instead of a multimap: no allocation per drawable, and better locality.
	*/
	template<EOrderingMode OrderingMode, bool BatchMaterial = false>
	using FilteredDrawableMap = std::vector<std::pair<DrawOrder<OrderingMode, BatchMaterial>, FilteredDrawable>>;

	/**
	* Input data for the parsing step.
	*/
	struct ParsingInput
	{
		OvCore::SceneSystem::Scene& scene;
	};

	/**
	* Result of the parsing step, containing the drawables found in the scene.
	*/
	struct ParsingResult
	{
		std::vector<OvRendering::Entities::Drawable> drawables;
	};

	/**
	* Input data for the filtering step.
	*/
	struct FilteringInput
	{
		const OvRendering::Entities::Camera& camera;
		OvTools::Utils::OptRef<const OvRendering::Data::Frustum> frustumOverride;
		OvTools::Utils::OptRef<OvRendering::Data::Material> overrideMaterial;
		OvTools::Utils::OptRef<OvRendering::Data::Material> fallbackMaterial;
		EVisibilityFlags requiredVisibilityFlags = EVisibilityFlags::NONE;
		bool includeUI = true; // Whether to include UI drawables in the filtering
		bool includeTransparent = true; // Whether to include transparent drawables in the filtering
		bool includeOpaque = true; // Whether to include opaque drawables in the filtering
		std::function<bool(const OvRendering::Entities::Drawable&, const OvRendering::Data::Material&)> filter; // Optional filter applied to the parsed drawable and its resolved material
	};

	/**
	* Result of the filtering step, categorized by type (opaque, transparent, UI) and sorted by draw order.
	*/
	struct FilteringResult
	{
		FilteredDrawableMap<EOrderingMode::FRONT_TO_BACK, true> opaques;
		FilteredDrawableMap<EOrderingMode::BACK_TO_FRONT> transparents;
		FilteredDrawableMap<EOrderingMode::BACK_TO_FRONT> ui;
	};

	/**
	* Input data for the preparation step.
	*/
	struct PreparationInput
	{
		std::optional<std::string> pass = std::nullopt; // Pass assigned to the prepared drawable
		OvTools::Utils::OptRef<OvRendering::Data::Material> passFallbackMaterial; // Used when the resolved material doesn't have the given pass
		std::function<void(OvRendering::Entities::Drawable&)> customPreparation; // Optional logic applied to the prepared drawable (invoked right before submission when using Draw)
	};

	/**
	* Parse the given scene to find its drawables.
	* @param p_input
	*/
	ParsingResult Parse(const ParsingInput& p_input);

	/**
	* Select and sort the parsed drawables based on the given input.
	* This is where culling and sorting happens.
	* @param p_parsingResult
	* @param p_input
	* @note The filtering result references the parsed drawables, which must outlive it.
	*/
	FilteringResult Filter(const ParsingResult& p_parsingResult, const FilteringInput& p_input);

	/**
	* Returns true if the bounds of the given parsed drawable intersect the given frustum.
	* Drawables without bounds are always considered inside the frustum.
	* @param p_drawable
	* @param p_frustum
	*/
	bool IsInFrustum(const OvRendering::Entities::Drawable& p_drawable, const OvRendering::Data::Frustum& p_frustum);

	/**
	* Create the drawable to submit from a filtered drawable, based on the given input.
	* @param p_filteredDrawable
	* @param p_input
	*/
	OvRendering::Entities::Drawable Prepare(const FilteredDrawable& p_filteredDrawable, const PreparationInput& p_input);

	/**
	* Prepare and draw the given filtered drawables, in order.
	* @param p_renderer
	* @param p_pso
	* @param p_filteredDrawables
	* @param p_input
	*/
	template<EOrderingMode OrderingMode, bool BatchMaterial>
	void Draw(
		OvRendering::Core::CompositeRenderer& p_renderer,
		OvRendering::Data::PipelineState p_pso,
		const FilteredDrawableMap<OrderingMode, BatchMaterial>& p_filteredDrawables,
		const PreparationInput& p_input
	);

	/**
	* Prepare and draw all the filtered drawables (opaques, then transparents, then UI).
	* @param p_renderer
	* @param p_pso
	* @param p_filteringResult
	* @param p_input
	*/
	void Draw(
		OvRendering::Core::CompositeRenderer& p_renderer,
		OvRendering::Data::PipelineState p_pso,
		const FilteringResult& p_filteringResult,
		const PreparationInput& p_input
	);
}

#include <OvCore/Rendering/FrameBuilder.inl>
