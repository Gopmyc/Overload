/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include <OvRendering/Resources/Loaders/ModelLoader.h>

#include "OvCore/ResourceManagement/AResourceManager.h"

namespace OvCore::ResourceManagement
{
	/**
	* ResourceManager of models
	*/
	class ModelManager : public AResourceManager<OvRendering::Resources::Model>
	{
	public:
		/**
		* Create the resource identified by the given path
		* @param p_path
		*/
		virtual OvRendering::Resources::Model* CreateResource(const std::filesystem::path & p_path) override;

		/**
		* Destroy the given resource
		* @param p_resource
		*/
		virtual void DestroyResource(OvRendering::Resources::Model* p_resource) override;

		/**
		* Reload the given resource
		* @param p_resource
		* @param p_path
		*/
		virtual void ReloadResource(OvRendering::Resources::Model* p_resource, const std::filesystem::path& p_path) override;

		/**
		* Registers a model made of the given meshes under the given path, which needs no file. A model made this way
		* already registered there is given the meshes instead, in place: everything using it draws them from then on.
		* Returns nullptr (the meshes are then left to the caller) when a model loaded from a file has this path
		* @param p_path
		* @param p_meshes
		* @param p_materialNames
		*/
		OvRendering::Resources::Model* CreateFromGeometry(
			const std::filesystem::path& p_path,
			std::vector<OvRendering::Resources::Mesh*> p_meshes,
			std::vector<std::string> p_materialNames
		);

	private:
		std::unordered_set<const OvRendering::Resources::Model*> m_geometryModels;
	};
}