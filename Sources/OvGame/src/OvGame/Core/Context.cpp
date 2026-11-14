/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <OvCore/Input/GameInput.h>
#include <filesystem>

#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Helpers/ProjectSettings.h>
#include <OvCore/Rendering/FramebufferUtil.h>
#include <OvCore/Scripting/ScriptEngine.h>

#include <OvDebug/Logger.h>

#include <OvGame/Core/Context.h>
#include <OvTools/Utils/PathParser.h>

using namespace OvCore::Global;
using namespace OvCore::ResourceManagement;

std::array<int, 4> CalculateOptimalWindowSizeAndPosition(
	const std::array<int, 4>& p_workAreaSize,
	std::array<int, 2> p_resolution
)
{
	const auto [workAreaX, workAreaY, workAreaWidth, workAreaHeight] = p_workAreaSize;

	std::array<int, 2> finalResolution = p_resolution;

	// Check if the given resolution fits in the workArea. If not, keep reducing it until it fits
	while (finalResolution[0] > workAreaWidth || finalResolution[1] > workAreaHeight)
	{
		finalResolution[0] /= 2;
		finalResolution[1] /= 2;
	}

	if (finalResolution != p_resolution)
	{
		OVLOG_WARNING("The target resolution didn't fit in the work area. It has been reduced to " + std::to_string(p_resolution[0]) + "x" + std::to_string(p_resolution[1]));
	}

	return { 
		workAreaX + workAreaWidth / 2 - p_resolution[0] / 2,
		workAreaY + workAreaHeight / 2 - p_resolution[1] / 2,
		p_resolution[0],
		p_resolution[1]
	};
}

OvGame::Core::Context::Context(const LaunchOptions& p_options) :
	engineAssetsPath(p_options.engineAssets.value_or(std::filesystem::current_path() / "Data" / "Engine")),
	projectAssetsPath(p_options.projectFile ? p_options.projectFile->parent_path() / "Assets" : std::filesystem::current_path() / "Data" / "User" / "Assets"),
	projectSettings(p_options.projectFile.value_or(std::filesystem::current_path() / "Data" / "User" / "Game.ini").string()),
	sceneManager(projectAssetsPath.string(), engineAssetsPath.string())
{
	startScene = p_options.scene.value_or(projectSettings.GetOrDefault<std::string>("start_scene", {}));

	ModelManager::ProvideAssetPaths(projectAssetsPath, engineAssetsPath);
	TextureManager::ProvideAssetPaths(projectAssetsPath, engineAssetsPath);
	ShaderManager::ProvideAssetPaths(projectAssetsPath, engineAssetsPath);
	MaterialManager::ProvideAssetPaths(projectAssetsPath, engineAssetsPath);
	FontManager::ProvideAssetPaths(projectAssetsPath, engineAssetsPath);
	SoundManager::ProvideAssetPaths(projectAssetsPath, engineAssetsPath);

	materialManager.ProvideStandardShaderDefinition({
		.shaderPath = ":Shaders/Standard.ovfx"
	});
	uiResourceRegistry.ProvideDefinition({
		.imageMaterialPath = ":Materials/Image.ovmat",
		.textMaterialPath = ":Materials/Text.ovmat",
		.defaultFontPath = ":Fonts/Roboto-Regular.ttf",
		.litImageMaterialPath = ":Materials/ImageLit.ovmat",
		.litTextMaterialPath = ":Materials/TextLit.ovmat"
	});

	/* Settings */
	// Only request a multisampled default framebuffer when multisampling is enabled.
	// Otherwise the "samples" setting (4 by default) would allocate an unused MSAA back buffer.
	const bool multisampling = projectSettings.GetOrDefault<bool>("multisampling", false);
	const int samples = multisampling ? projectSettings.GetOrDefault<int>("samples", 0) : 0;

	OvWindowing::Settings::DeviceSettings deviceSettings;
	deviceSettings.samples = static_cast<uint8_t>(samples);

	OvWindowing::Settings::WindowSettings windowSettings;
	projectSettings.TryGet("executable_name", windowSettings.title);
	std::array<int, 2> targetWindowResolution = { 1280, 720 };
	projectSettings.TryGet("x_resolution", targetWindowResolution[0]);
	projectSettings.TryGet("y_resolution", targetWindowResolution[1]);
	windowSettings.maximized = false;
	projectSettings.TryGet("resizable", windowSettings.resizable);
	projectSettings.TryGet("fullscreen", windowSettings.fullscreen);
	windowSettings.samples = static_cast<uint32_t>(samples);
	windowSettings.visible = !p_options.hiddenWindow;

	/* Window creation */
	device = std::make_unique<OvWindowing::Context::Device>(deviceSettings);
	if (windowSettings.fullscreen)
	{
		windowSettings.width = targetWindowResolution[0];
		windowSettings.height = targetWindowResolution[1];
	}
	else
	{
		const auto workAreaSize = device->GetWorkAreaSize();
		const auto bestFitWindowSizeAndPosition = CalculateOptimalWindowSizeAndPosition(workAreaSize, targetWindowResolution);
		windowSettings.x = bestFitWindowSizeAndPosition[0];
		windowSettings.y = bestFitWindowSizeAndPosition[1];
		windowSettings.width = bestFitWindowSizeAndPosition[2];
		windowSettings.height = bestFitWindowSizeAndPosition[3];
	}
	window = std::make_unique<OvWindowing::Window>(*device, windowSettings);
	bool useFallbackWindowIcon = true;
	if (const std::string windowIconPath = projectSettings.GetOrDefault("window_icon", std::string{}); !windowIconPath.empty())
	{
		const std::filesystem::path realPath = OvTools::Utils::PathParser::GetRealPath(
			std::filesystem::path{ windowIconPath },
			engineAssetsPath,
			projectAssetsPath
		);

		useFallbackWindowIcon = !std::filesystem::exists(realPath) || !window->SetIcon(realPath.string());
	}
	if (useFallbackWindowIcon)
	{
		const auto fallbackWindowIconPath = engineAssetsPath / "Textures" / "Overload.png";
		if (!window->SetIcon(fallbackWindowIconPath.string()))
		{
			OVLOG_WARNING("Fallback window icon could not be loaded from: " + fallbackWindowIconPath.string());
		}
	}
	inputManager = std::make_unique<OvWindowing::Inputs::InputManager>(*window);
	window->MakeCurrentContext();

	device->SetVsync(projectSettings.GetOrDefault<bool>("vsync", true));

	OvRendering::Data::PipelineState basePSO;
	basePSO.multisample = multisampling;

	/* Graphics context creation */
	driver = std::make_unique<OvRendering::Context::Driver>(OvRendering::Settings::DriverSettings{
#ifdef _DEBUG
		true,
#else
		false,
#endif
		basePSO
	});

	uiManager = std::make_unique<OvUI::Core::UIManager>(
		*window,
		OvUI::Styling::EStyle::DEFAULT_DARK
	);

	const auto fontPath = engineAssetsPath / "Fonts" / "Roboto-Regular.ttf";

	uiManager->LoadFont("Roboto", fontPath.string(), 15.0f);
	uiManager->UseFont("Roboto");
	uiManager->EnableEditorLayoutSave(false);
	uiManager->EnableDocking(false);

	/* Audio */
	audioEngine = std::make_unique<OvAudio::Core::AudioEngine>(
		p_options.nullAudio ? OvAudio::Core::EDriver::NULL_DRIVER : OvAudio::Core::EDriver::AUTO
	);

	/* Physics engine */
	physicsEngine = std::make_unique<OvPhysics::Core::PhysicsEngine>(OvCore::Helpers::ProjectSettings::ReadPhysicsSettings(projectSettings));

	/* Scripting */
	scriptEngine = std::make_unique<OvCore::Scripting::ScriptEngine>(
		projectAssetsPath,
		engineAssetsPath
	);

	/* What the scripts may do: their saves, their Json and the longest frame step */
	const auto runtimeLimits = OvCore::Helpers::ProjectSettings::ReadRuntimeLimits(projectSettings);

	saveStorage = std::make_unique<OvTools::Filesystem::SaveStorage>(
		OvTools::Filesystem::SaveStorage::GetDefaultRoot(OvCore::Helpers::ProjectSettings::GetSaveName(projectSettings)),
		runtimeLimits.storage
	);

	OvCore::Scripting::SetJsonLimits(runtimeLimits.json);
	clock.SetMaxDeltaTime(runtimeLimits.maxFrameDelta);

	/* Service Locator providing */
	ServiceLocator::Provide<OvPhysics::Core::PhysicsEngine>(*physicsEngine);
	ServiceLocator::Provide<ModelManager>(modelManager);
	ServiceLocator::Provide<TextureManager>(textureManager);
	ServiceLocator::Provide<ShaderManager>(shaderManager);
	ServiceLocator::Provide<MaterialManager>(materialManager);
	ServiceLocator::Provide<FontManager>(fontManager);
	ServiceLocator::Provide<SoundManager>(soundManager);
	ServiceLocator::Provide<UIResourceRegistry>(uiResourceRegistry);
	ServiceLocator::Provide<OvWindowing::Inputs::InputManager>(*inputManager);
	ServiceLocator::Provide<OvWindowing::Window>(*window);
	ServiceLocator::Provide<OvCore::SceneSystem::SceneManager>(sceneManager);
	ServiceLocator::Provide<OvAudio::Core::AudioEngine>(*audioEngine);
	ServiceLocator::Provide<OvCore::Scripting::ScriptEngine>(*scriptEngine);
	ServiceLocator::Provide<OvTools::Filesystem::SaveStorage>(*saveStorage);

	/* The project's input actions, and the player's bindings from the save folder */
	inputActions = std::make_unique<OvCore::Input::InputActions>();
	OvCore::Input::LoadProjectActions(projectSettings, projectAssetsPath, *saveStorage, *inputActions);
	ServiceLocator::Provide<OvCore::Input::InputActions>(*inputActions);

	localization = std::make_unique<OvCore::Localization::Localization>();
	OvCore::Localization::LoadProjectTranslations(projectSettings, projectAssetsPath, *saveStorage, *localization);
	ServiceLocator::Provide<OvCore::Localization::Localization>(*localization);
	ServiceLocator::Provide<OvTools::Time::Clock>(clock);

	framebuffer = std::make_unique<baregl::Framebuffer>("Main");

	OvCore::Rendering::FramebufferUtil::SetupFramebuffer(
		*framebuffer,
		windowSettings.width,
		windowSettings.height,
		true, false, false
	);

	// A minimized window reports 0x0: the framebuffer keeps its last size until it is restored
	window->FramebufferResizeEvent.AddListener([this](unsigned short width, unsigned short height) {
		if (width > 0 && height > 0)
			framebuffer->Resize(width, height);
	});
}

OvGame::Core::Context::~Context()
{
	modelManager.UnloadResources();
	textureManager.UnloadResources();
	shaderManager.UnloadResources();
	materialManager.UnloadResources();
	fontManager.UnloadResources();
	soundManager.UnloadResources();
}
