/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <OvPhysics/Core/PhysicsEngine.h>

#include <OvWindowing/Context/Device.h>
#include <OvWindowing/Inputs/InputManager.h>
#include <OvWindowing/Window.h>

#include <OvUI/Core/UIManager.h>

#include <OvCore/ResourceManagement/FontManager.h>
#include <OvCore/ResourceManagement/ModelManager.h>
#include <OvCore/ResourceManagement/TextureManager.h>
#include <OvCore/ResourceManagement/ShaderManager.h>
#include <OvCore/ResourceManagement/MaterialManager.h>
#include <OvCore/ResourceManagement/SoundManager.h>
#include <OvCore/ResourceManagement/UIResourceRegistry.h>
#include <OvCore/SceneSystem/SceneManager.h>
#include <OvCore/Scripting/ScriptEngine.h>

#include <OvAudio/Core/AudioEngine.h>

#include <OvTools/Filesystem/IniFile.h>
#include <OvTools/Filesystem/SaveStorage.h>
#include <OvTools/Time/Clock.h>

#include <OvGame/Core/LaunchOptions.h>

namespace OvGame::Core
{
	/**
	* The Context handle the engine features setup
	*/
	class Context
	{
	public:
		/**
		* Constructor
		* @param p_options (where the project and the engine are, and how the window shows)
		*/
		Context(const LaunchOptions& p_options);

		/**
		* Destructor
		*/
		~Context();

	public:
		const std::filesystem::path engineAssetsPath;
		const std::filesystem::path projectAssetsPath;

		/**
		* The scene the game starts on: the command line's, or the project's start_scene
		*/
		std::string startScene;

		std::unique_ptr<OvWindowing::Context::Device> device;
		std::unique_ptr<OvWindowing::Window> window;
		std::unique_ptr<OvWindowing::Inputs::InputManager> inputManager;
		std::unique_ptr<OvRendering::Context::Driver> driver;
		std::unique_ptr<OvUI::Core::UIManager> uiManager;
		std::unique_ptr<OvPhysics::Core::PhysicsEngine> physicsEngine;
		std::unique_ptr<OvAudio::Core::AudioEngine> audioEngine;
		std::unique_ptr<OvCore::Scripting::ScriptEngine> scriptEngine;
		std::unique_ptr<OvTools::Filesystem::SaveStorage> saveStorage;
		std::unique_ptr<baregl::Framebuffer> framebuffer;

		OvCore::SceneSystem::SceneManager sceneManager;
		OvTools::Time::Clock clock;

		OvCore::ResourceManagement::ModelManager modelManager;
		OvCore::ResourceManagement::TextureManager textureManager;
		OvCore::ResourceManagement::ShaderManager shaderManager;
		OvCore::ResourceManagement::MaterialManager materialManager;
		OvCore::ResourceManagement::FontManager fontManager;
		OvCore::ResourceManagement::SoundManager soundManager;
		OvCore::ResourceManagement::UIResourceRegistry uiResourceRegistry;
		
		OvTools::Filesystem::IniFile projectSettings;
	};
}
