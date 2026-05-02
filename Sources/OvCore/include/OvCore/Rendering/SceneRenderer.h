/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <OvRendering/Core/CompositeRenderer.h>
#include <OvRendering/Data/Frustum.h>
#include <OvRendering/Entities/Drawable.h>
#include <OvRendering/HAL/UniformBuffer.h>
#include <OvRendering/HAL/ShaderStorageBuffer.h>
#include <OvRendering/Resources/Mesh.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CCamera.h>
#include <OvCore/Rendering/EVisibilityFlags.h>
#include <OvCore/Resources/Material.h>
#include <OvCore/SceneSystem/Scene.h>

namespace OvCore::ECS::Components
{
	class CMaterialRenderer;
	class CSkinnedMeshRenderer;
}

namespace OvCore::Rendering
{
	/**
	* Extension of the CompositeRenderer adding support for the scene system (parsing/drawing entities)
	*/
	class SceneRenderer : public OvRendering::Core::CompositeRenderer
	{
	public:
		enum class EOrderingMode
		{
			BACK_TO_FRONT,
			FRONT_TO_BACK,
		};

		template<EOrderingMode OrderingMode>
		struct DrawOrder
		{
			int order;
			float distance;

			/**
			* Determines the order of the drawables.
			* Current order is: order -> distance
			* @param p_other
			*/
			bool operator<(const DrawOrder& p_other) const
			{
				if (order == p_other.order)
				{
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

		template<EOrderingMode OrderingMode>
		struct FilteredDrawableBucket
		{
			struct OrderedDrawable
			{
				DrawOrder<OrderingMode> sortKey;
				uint32_t drawableIndex;
			};

			std::vector<OvRendering::Entities::Drawable> drawables;
			std::vector<OrderedDrawable> orderedDrawables;
		};

		/**
		* Input data for the scene renderer.
		*/
		struct SceneDescriptor
		{
			OvCore::SceneSystem::Scene& scene;
			OvTools::Utils::OptRef<const OvRendering::Data::Frustum> frustumOverride;
			OvTools::Utils::OptRef<OvRendering::Data::Material> overrideMaterial;
			OvTools::Utils::OptRef<OvRendering::Data::Material> fallbackMaterial;
		};

		struct SceneParsingInput
		{
			OvCore::SceneSystem::Scene& scene;
		};

		/**
		* Parsed drawable data, stored as a cache-friendly, typed structure.
		*/
		struct ParsedDrawable
		{
			OvCore::ECS::Actor* actor = nullptr;
			const OvCore::ECS::Components::CMaterialRenderer* materialRenderer = nullptr;
			const OvCore::ECS::Components::CSkinnedMeshRenderer* skinnedRenderer = nullptr;
			OvRendering::Resources::Mesh* mesh = nullptr;
			uint32_t materialIndex = 0;
			bool meshHasSkinningData = false;
			std::optional<OvRendering::Geometry::BoundingSphere> bounds;
		};

		/**
		* Result of the scene parsing, containing the drawables to be rendered.
		*/
		struct SceneDrawablesDescriptor
		{
			const ParsedDrawable* drawables = nullptr;
			size_t count = 0;
		};

		/**
		* Additional information for a drawable computed by the scene renderer.
		*/
		struct SceneDrawableDescriptor
		{
			OvCore::ECS::Actor& actor;
			EVisibilityFlags visibilityFlags = EVisibilityFlags::NONE;
			std::optional<OvRendering::Geometry::BoundingSphere> bounds;
		};

		/**
		* Filtered drawables for the scene, categorized by their render pass, and sorted by their draw order.
		*/
		struct SceneFilteredDrawablesDescriptor
		{
			FilteredDrawableBucket<EOrderingMode::FRONT_TO_BACK> opaques;
			FilteredDrawableBucket<EOrderingMode::BACK_TO_FRONT> transparents;
			FilteredDrawableBucket<EOrderingMode::BACK_TO_FRONT> ui;
		};

		struct SceneDrawablesFilteringInput
		{
			const OvRendering::Entities::Camera& camera;
			OvTools::Utils::OptRef<const OvRendering::Data::Frustum> frustumOverride;
			OvTools::Utils::OptRef<OvRendering::Data::Material> overrideMaterial;
			OvTools::Utils::OptRef<OvRendering::Data::Material> fallbackMaterial;
			EVisibilityFlags requiredVisibilityFlags = EVisibilityFlags::NONE;
			bool includeUI = true; // Whether to include UI drawables in the filtering
			bool includeTransparent = true; // Whether to include transparent drawables in the filtering
			bool includeOpaque = true; // Whether to include opaque drawables in the filtering
		};

		/**
		* Constructor of the Renderer
		* @param p_driver
		* @param p_stencilWrite (if set to true, also write all the scene geometry to the stencil buffer)
		*/
		SceneRenderer(OvRendering::Context::Driver& p_driver, bool p_stencilWrite = false);

		/**
		* Begin Frame
		* @param p_frameDescriptor
		*/
		virtual void BeginFrame(const OvRendering::Data::FrameDescriptor& p_frameDescriptor) override;

		/**
		* Draw a model with a single material
		* @param p_pso
		* @param p_model
		* @param p_material
		* @param p_modelMatrix
		*/
		virtual void DrawModelWithSingleMaterial(
			OvRendering::Data::PipelineState p_pso,
			OvRendering::Resources::Model& p_model,
			OvRendering::Data::Material& p_material,
			const OvMaths::FMatrix4& p_modelMatrix
		);

		/**
		* Parse the scene (as defined in the SceneDescriptor) to find the drawables to render.
		* @param p_sceneDescriptor
		* @param p_options
		*/
		SceneDrawablesDescriptor ParseScene(
			const SceneParsingInput& p_input
		);

		/**
		* Filter and prepare drawables based on the given context.
		* This is where culling and sorting happens.
		* @param p_drawables
		* @param p_filteringInput
		*/
		SceneFilteredDrawablesDescriptor FilterDrawables(
			const SceneDrawablesDescriptor& p_drawables,
			const SceneDrawablesFilteringInput& p_filteringInput
		);

	private:
		struct ParseCacheEntry
		{
			const OvCore::ECS::Components::CModelRenderer* modelRenderer = nullptr;
			const OvRendering::Resources::Model* model = nullptr;
			const OvCore::ECS::Components::CMaterialRenderer* materialRenderer = nullptr;
			const OvCore::ECS::Components::CSkinnedMeshRenderer* skinnedRenderer = nullptr;
			OvCore::ECS::Components::CModelRenderer::EFrustumBehaviour frustumBehaviour = OvCore::ECS::Components::CModelRenderer::EFrustumBehaviour::MESH_BOUNDS;
			OvRendering::Geometry::BoundingSphere customBounds{};
		};

		bool ShouldRebuildParseCache(
			const OvCore::SceneSystem::Scene& p_scene,
			const std::vector<ParseCacheEntry>& p_currentEntries
		) const;

		void RebuildParseCache(
			const OvCore::SceneSystem::Scene& p_scene,
			const std::vector<ParseCacheEntry>& p_currentEntries
		);

	private:
		const OvCore::SceneSystem::Scene* m_cachedScene = nullptr;
		std::vector<ParseCacheEntry> m_parseCacheEntries;
		std::vector<ParsedDrawable> m_parsedDrawablesCache;
	};
}
