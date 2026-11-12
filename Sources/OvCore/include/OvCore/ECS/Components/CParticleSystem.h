/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include <OvCore/ECS/Components/AComponent.h>
#include <OvCore/Particles/ParticleEmitter.h>
#include <OvRendering/Geometry/BoundingSphere.h>

namespace baregl { class Buffer; }
namespace OvRendering::Resources { class Mesh; class Model; }
namespace OvCore::Resources { class Material; }

namespace OvCore::ECS::Components
{
	/**
	* Emits particles drawn by the GPU in one draw per mesh: a record per particle at its birth, from which the
	* shader works out where it is, how big, and its colour, at any time. Billboards by default, or any model with a
	* material whose shader reads the particles (Common/Particles.ovfxh)
	*/
	class CParticleSystem : public AComponent
	{
	public:
		static constexpr const char* kDefaultMaterialPath = ":Materials/Particle.ovmat";

		/**
		* Constructor
		* @param p_owner
		*/
		CParticleSystem(ECS::Actor& p_owner);

		/**
		* Destructor
		*/
		~CParticleSystem() override;

		/**
		* Returns the name of the component
		*/
		std::string GetName() override;

		/**
		* Returns the type name of the component
		*/
		virtual std::string GetTypeName() override;

		/**
		* Starts emitting from the start of a cycle
		*/
		void Play();

		/**
		* Stops emitting: the particles alive live on
		*/
		void Stop();

		/**
		* Removes every particle
		*/
		void Clear();

		/**
		* Returns true while the system emits
		*/
		bool IsPlaying() const;

		/**
		* Emits particles now, playing or not, and returns how many were born
		* @param p_count
		* @param p_overrides
		*/
		uint32_t Emit(uint32_t p_count, const Particles::ParticleOverrides& p_overrides = {});

		/**
		* Returns the particles alive
		*/
		uint32_t GetAliveCount() const;

		/**
		* Changes the settings, each brought into its range. A new number of particles clears the system
		* @param p_settings
		*/
		void SetSettings(const Particles::ParticleSettings& p_settings);

		/**
		* Returns the settings
		*/
		const Particles::ParticleSettings& GetSettings() const;

		/**
		* Sets the model each particle is drawn as, or nullptr for a billboard
		* @param p_model
		*/
		void SetModel(OvRendering::Resources::Model* p_model);

		/**
		* Returns the model each particle is drawn as, or nullptr for a billboard
		*/
		OvRendering::Resources::Model* GetModel() const;

		/**
		* Sets the material the particles are drawn with, or nullptr for the engine's (:Materials/Particle.ovmat)
		* @param p_material
		*/
		void SetMaterial(OvCore::Resources::Material* p_material);

		/**
		* Returns the material set, or nullptr when the engine's is used
		*/
		OvCore::Resources::Material* GetMaterial() const;

		/**
		* Returns the emitter, which holds the particles
		*/
		const Particles::ParticleEmitter& GetEmitter() const;

		/**
		* Moves the system's clock on and emits: the scene calls it once a frame, after its late update and the bone
		* attachments, so that an emitter follows what moved it in that frame
		* @param p_deltaTime (the frame's step in game time)
		*/
		void Simulate(float p_deltaTime);

		/**
		* Returns a sphere in the world holding every particle alive, or nothing when none is
		*/
		std::optional<OvRendering::Geometry::BoundingSphere> GetWorldBounds() const;

		/**
		* Returns the material the particles are drawn with: the one set, or the engine's
		* @note Loads the engine's material on first use: call it where the GPU is ready
		*/
		OvCore::Resources::Material* ResolveMaterial() const;

		/**
		* Returns the quad a billboard is drawn as
		* @note Made on first use: call it where the GPU is ready
		*/
		OvRendering::Resources::Mesh& GetQuad();

		/**
		* Sends what changed to the GPU, then binds the particles to a shader storage binding point
		* @note Made on first use: call it where the GPU is ready
		* @param p_bindingPoint
		*/
		void BindBuffer(uint32_t p_bindingPoint);

		/**
		* Plays when the scene starts, if set to
		*/
		virtual void OnStart() override;

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
		Particles::ParticleEmitter m_emitter;
		OvRendering::Resources::Model* m_model = nullptr;
		OvCore::Resources::Material* m_material = nullptr;

		std::unique_ptr<OvRendering::Resources::Mesh> m_quad;
		std::unique_ptr<baregl::Buffer> m_buffer;
		uint32_t m_bufferCapacity = 0;
		std::optional<Particles::ParticleBufferHeader> m_uploadedHeader;
	};

	template<>
	struct ComponentTraits<OvCore::ECS::Components::CParticleSystem>
	{
		static constexpr std::string_view Name = "class OvCore::ECS::Components::CParticleSystem";
	};
}
