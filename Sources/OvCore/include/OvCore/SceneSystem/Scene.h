/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <OvCore/API/ISerializable.h>
#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/CBoneAttachment.h>
#include <OvCore/ECS/Components/CCamera.h>
#include <OvCore/ECS/Components/CLight.h>
#include <OvCore/ECS/Components/CModelRenderer.h>
#include <OvCore/ECS/Components/CParticleSystem.h>
#include <OvCore/ECS/Components/CPostProcessStack.h>
#include <OvCore/ECS/Components/CReflectionProbe.h>
#include <OvCore/ECS/Components/CSkinnedMeshRenderer.h>
#include <OvCore/Navigation/NavMesh.h>
#include <OvTools/Eventing/Event.h>
#include <OvTools/Utils/OptRef.h>

namespace OvCore::SceneSystem
{
	/**
	* The scene is a set of actors
	*/
	class Scene : public API::ISerializable
	{
	public:
		/**
		* Contains a set of vectors of components that are sorted. It allows fast
		* manipulation of components without parsing the whole scene
		*/
		struct FastAccessComponents
		{
			std::vector<ECS::Components::CModelRenderer*> modelRenderers;
			std::vector<ECS::Components::CCamera*> cameras;
			std::vector<ECS::Components::CLight*> lights;
			std::vector<ECS::Components::CPostProcessStack*> postProcessStacks;
			std::vector<ECS::Components::CReflectionProbe*> reflectionProbes;
			std::vector<ECS::Components::CBoneAttachment*> boneAttachments;
			std::vector<ECS::Components::CParticleSystem*> particleSystems;
			std::vector<ECS::Components::CSkinnedMeshRenderer*> skinnedMeshRenderers;
		};

		/**
		* Constructor of the scene
		*/
		Scene(
			const std::filesystem::path& p_projectAssetsPath = "",
			const std::filesystem::path& p_engineAssetsPath = ""
		);

		/**
		* Handle the memory de-allocation of every actors
		*/
		~Scene();

		/**
		* Add a default camera to the scene
		*/
		void AddDefaultCamera();

		/**
		* Add default lights to the scene
		*/
		void AddDefaultLights();

		/**
		* Add default lights to the scene
		*/
		void AddDefaultReflections();

		/**
		* Add default post-process stack to the scene
		*/
		void AddDefaultPostProcessStack();

		/**
		* Add default skysphere to the scene
		*/
		void AddDefaultSkysphere();

		/**
		* Add default atmosphere to the scene
		*/
		void AddDefaultAtmosphere();

		/**
		* Play the scene
		*/
		void Play();

		/**
		* Returns true if the scene is playing
		*/
		bool IsPlaying() const;

		/**
		* Update every actors
		* @param p_deltaTime
		*/
		void Update(float p_deltaTime);

		/**
		* Update every actors 60 frames per seconds
		* @param p_deltaTime
		*/
		void FixedUpdate(float p_deltaTime);

		/**
		* Update every actors lately
		* @param p_deltaTime
		*/
		void LateUpdate(float p_deltaTime);

		/**
		* Invoked at the start of Update, before any actor's: the script engine's global Update event
		*/
		static OvTools::Eventing::Event<float> UpdateEvent;

		/**
		* Invoked in LateUpdate once every actor's has run, before the bone attachments follow their bones: the script
		* engine's global LateUpdate event
		*/
		static OvTools::Eventing::Event<float> LateUpdateEvent;

		/**
		* Works out the pose of every skinned mesh that needs it, once the scripts have run and before the bone
		* attachments read it: a pose set many times in the frame is worked out once. Called by LateUpdate.
		*/
		void UpdateSkinnedMeshes();

		/**
		* Places every bone attachment on its bone, parents first. Called at the end of LateUpdate, once every script
		* has run.
		*/
		void UpdateBoneAttachments();

		/**
		* Moves every particle system's clock on, once the actors are where the frame leaves them
		* @param p_deltaTime
		*/
		void UpdateParticleSystems(float p_deltaTime);

		/**
		* Create an actor with a default name and return a reference to it.
		*/
		ECS::Actor& CreateActor();

		/**
		* Create an actor with the given name and return a reference to it.
		* @param p_name
		* @param p_tag
		*/
		ECS::Actor& CreateActor(const std::string& p_name, const std::string& p_tag = "");

		/**
		* Instantiate a prefab and return its root actor.
		* @param p_prefabPath
		*/
		ECS::Actor* InstantiatePrefab(const std::string& p_prefabPath);

		/**
		* Instantiate a prefab under the given parent and return its root actor.
		* @param p_prefabPath
		* @param p_parent
		*/
		ECS::Actor* InstantiatePrefab(const std::string& p_prefabPath, ECS::Actor& p_parent);

		/**
		* Destroy and actor and return true on success
		* @param p_target (The actor to remove from the scene)
		*/
		bool DestroyActor(ECS::Actor& p_target);

		/**
		* Move the given actor at the end of the actor list, which defines the order in which
		* actors are stored, updated and serialized
		* @param p_actor
		*/
		void MoveActorToBack(ECS::Actor& p_actor);

		/**
		* Collect garbages by removing Destroyed-marked actors
		*/
		void CollectGarbages();

		/**
		* Return the first actor identified by the given name, or nullptr on fail
		* @param p_name
		*/
		ECS::Actor* FindActorByName(const std::string& p_name) const;

		/**
		* Return the first actor identified by the given tag, or nullptr on fail
		* @param p_tag
		*/
		ECS::Actor* FindActorByTag(const std::string& p_tag) const;

		/**
		* Return the actor identified by the given ID (Returns nullptr on fail)
		* @param p_id
		*/
		ECS::Actor* FindActorByID(int64_t p_id) const;

		/**
		* Return the actor identified by the given GUID (Returns nullptr on fail)
		* @param p_guid
		*/
		ECS::Actor* FindActorByGUID(uint64_t p_guid) const;

		/**
		* Return every actors identified by the given name
		* @param p_name
		*/
		std::vector<std::reference_wrapper<ECS::Actor>> FindActorsByName(const std::string& p_name) const;

		/**
		* Return every actors identified by the given tag
		* @param p_tag
		*/
		std::vector<std::reference_wrapper<ECS::Actor>> FindActorsByTag(const std::string& p_tag) const;

		/**
		* Parse the scene to find the main camera
		*/
		OvCore::ECS::Components::CCamera* FindMainCamera() const;

		/**
		* Callback method called everytime a component is added on an actor of the scene
		* @param p_component
		*/
		void OnComponentAdded(ECS::Components::AComponent& p_compononent);

		/**
		* Callback method called everytime a component is removed on an actor of the scene
		* @param p_component
		*/
		void OnComponentRemoved(ECS::Components::AComponent& p_compononent);

		/**
		* Return a reference on the actor map
		*/
		std::vector<OvCore::ECS::Actor*>& GetActors();

		/**
		* Return the fast access components data structure
		*/
		const FastAccessComponents& GetFastAccessComponents() const;

		/**
		* Returns the scene's navigation mesh, built from its static colliders by Navigation.Build, and freed with it
		*/
		Navigation::NavMesh& GetNavigation();

		/**
		* Serialize the scene
		* @param p_doc
		* @param p_root
		*/
		virtual void OnSerialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_root) override;

		/**
		* Deserialize the scene
		* @param p_doc
		* @param p_root
		*/
		virtual void OnDeserialize(tinyxml2::XMLDocument& p_doc, tinyxml2::XMLNode* p_root) override;

	private:
		std::filesystem::path GetRealAssetPath(const std::string& p_path) const;
		void BeginBatchActorCreation();
		void EndBatchActorCreation(bool p_startCreatedActors);
		ECS::Actor* InstantiatePrefabInternal(const std::string& p_prefabPath, OvTools::Utils::OptRef<ECS::Actor> p_parent);
		void IndexActorGUID(ECS::Actor& p_actor, uint64_t p_guid);
		void UnindexActorGUID(const ECS::Actor* p_actor, uint64_t p_guid);

		int64_t m_availableID = 1;
		bool m_isPlaying = false;
		bool m_batchActorCreation = false;
		std::vector<ECS::Actor*> m_actors;
		std::vector<std::reference_wrapper<ECS::Actor>> m_batchCreatedActors;

		/* Actors by GUID. When two actors share one, the first keeps the entry and the other waits in the shadow map */
		std::unordered_map<uint64_t, ECS::Actor*> m_actorsByGUID;
		std::unordered_multimap<uint64_t, ECS::Actor*> m_shadowedActorsByGUID;

		FastAccessComponents m_fastAccessComponents;
		std::filesystem::path m_projectAssetsPath;
		std::filesystem::path m_engineAssetsPath;
		std::unique_ptr<Navigation::NavMesh> m_navigation;
	};
}
