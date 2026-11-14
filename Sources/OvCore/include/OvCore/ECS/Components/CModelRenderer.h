/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <OvMaths/FVector3.h>
#include <OvRendering/Geometry/Vertex.h>
#include <OvRendering/Resources/Model.h>

#include "OvCore/ECS/Components/AComponent.h"
#include "OvCore/Rendering/LodSelection.h"

namespace OvCore::ECS { class Actor; }

namespace OvCore::ECS::Components
{
	/**
	* A ModelRenderer is necessary in combination with a MaterialRenderer to render a model in the world
	*/
	class CModelRenderer : public AComponent
	{
	public:
		/**
		* Defines how the model renderer bounding sphere should be interpreted
		*/
		enum class EFrustumBehaviour
		{
			DISABLED = 0,
			DEPRECATED_MODEL_BOUNDS = 1, // This is not used anymore, but the enum value is kept for compatibility
			MESH_BOUNDS = 2,
			CUSTOM_BOUNDS = 3
		};

		/**
		* A lighter model standing for the renderer's own from a distance on
		*/
		struct LodLevel
		{
			OvRendering::Resources::Model* model = nullptr;
			float distance = 0.0f;
		};

		/**
		* The lighter levels a renderer takes besides its own model
		*/
		static constexpr uint32_t kMaxLodLevels = OvCore::Rendering::LodSelection::kMaxLevels - 1;

		/**
		* Constructor
		* @param p_owner
		*/
		CModelRenderer(ECS::Actor& p_owner);

		/**
		* Returns the name of the component
		*/
		std::string GetName() override;

		/**
		* Returns the type name of the component
		*/
		virtual std::string GetTypeName() override;

		/**
		* Defines the model to use
		* @param p_model
		*/
		void SetModel(OvRendering::Resources::Model* p_model);

		/**
		* Returns the current model
		*/
		OvRendering::Resources::Model* GetModel() const;

		/**
		* Sets a bounding mode
		* @param p_boundingMode
		*/
		void SetFrustumBehaviour(EFrustumBehaviour p_boundingMode);

		/**
		* Returns the current bounding mode
		*/
		EFrustumBehaviour GetFrustumBehaviour() const;

		/**
		* Returns the custom bounding sphere
		*/
		const OvRendering::Geometry::BoundingSphere& GetCustomBoundingSphere() const;

		/**
		* Sets the custom bounding sphere
		* @param p_boundingSphere
		*/
		void SetCustomBoundingSphere(const OvRendering::Geometry::BoundingSphere& p_boundingSphere);

		/**
		* Sets the distance from the camera beyond which the model isn't drawn, in any pass (0: always drawn)
		* @param p_distance
		*/
		void SetMaxDrawDistance(float p_distance);

		/**
		* Returns the distance from the camera beyond which the model isn't drawn (0: always drawn)
		*/
		float GetMaxDrawDistance() const;

		/**
		* Sets the lighter levels, each drawn from its distance on (measured to the centre of the model's bounds), with
		* the renderer's materials by slot. The first kMaxLodLevels are kept; a level the renderer cannot draw is left out
		* and GetLodError says why
		* @param p_levels
		*/
		void SetLods(std::span<const LodLevel> p_levels);

		/**
		* Returns the lighter levels, as set
		*/
		std::vector<LodLevel> GetLods() const;

		/**
		* Sets the factor of every level's distance: above 1 keeps the detail further away, below 1 lets it go nearer
		* @param p_bias (above 0)
		*/
		void SetLodBias(float p_bias);

		/**
		* Returns the factor of every level's distance
		*/
		float GetLodBias() const;

		/**
		* Returns why a level is left out (no model, not beyond the level before, other bones), or nothing
		*/
		std::optional<std::string> GetLodError();

		/**
		* Returns the level the scene's main camera drew last: 0 for the renderer's own model, n for the nth lighter one
		*/
		uint32_t GetLodLevel() const;

		/**
		* Chooses the level for a camera and returns the model to draw: called by the frame builder once a frame and view
		* @param p_viewPosition (nothing: the level chosen last)
		* @param p_mainView (the view of the scene's main camera, whose level is reported; another view, such as the
		* editor's scene view, keeps a level of its own)
		*/
		OvRendering::Resources::Model* SelectLod(const std::optional<OvMaths::FVector3>& p_viewPosition, bool p_mainView);

		/**
		* Lets go of a model about to be unloaded: the renderer's own, or a level's
		* @param p_model
		*/
		void ForgetModel(const OvRendering::Resources::Model* p_model);

		/**
		* Serialize the component
		* @param p_doc
		* @param p_node
		*/
		virtual void OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node) override;

		/**
		* Deserialize the component
		* @param p_doc
		* @param p_node
		*/
		virtual void OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_node) override;

		/**
		* Defines how the component should be drawn in the inspector
		* @param p_root
		*/
		virtual void OnInspector(OvUI::Internal::WidgetContainer& p_root) override;

	private:
		struct ActiveLod
		{
			OvRendering::Resources::Model* model = nullptr;
			uint32_t level = 0; // Its number among the levels as set, from 1
		};

		void RefreshLods();
		void BuildLodWidgets(OvUI::Internal::WidgetContainer& p_container);

	private:
		OvRendering::Resources::Model* m_model = nullptr;
		OvTools::Eventing::Event<> m_modelChangedEvent;
		OvTools::Eventing::Event<> m_lodChangedEvent;
		std::array<LodLevel, kMaxLodLevels> m_lods{};
		uint32_t m_lodCount = 0;
		float m_lodBias = 1.0f;
		bool m_lodsDirty = true;
		std::vector<ActiveLod> m_activeLods;
		std::vector<float> m_activeStarts;
		std::optional<std::string> m_lodError;
		uint32_t m_lodLevel = 0; // Among the active levels, the main view's
		uint32_t m_otherLodLevel = 0; // Another view's
		uint32_t m_lodRevision = 0;
		uint32_t m_inspectedLodRevision = 0;
		OvRendering::Geometry::BoundingSphere m_customBoundingSphere = { {}, 1.0f };
		EFrustumBehaviour m_frustumBehaviour = EFrustumBehaviour::MESH_BOUNDS;
		float m_maxDrawDistance = 0.0f;
	};

	template<>
	struct ComponentTraits<OvCore::ECS::Components::CModelRenderer>
	{
		static constexpr std::string_view Name = "class OvCore::ECS::Components::CModelRenderer";
	};
}
