/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <OvCore/Input/GameInput.h>
#include <tracy/Tracy.hpp>

#include <OvDebug/Logger.h>
#include <OvGame/Core/Game.h>
#include <OvTools/Utils/PathParser.h>
#include <OvUI/Widgets/Texts/Text.h>

#ifdef _DEBUG
#include <OvRendering/Features/FrameInfoRenderFeature.h>
#endif

OvGame::Core::Game::Game(Context & p_context) :
	m_context(p_context),
	m_sceneRenderer(*p_context.driver),
	m_fpsCounter(*p_context.window)
	#ifdef _DEBUG
	,
	m_driverInfo(*m_context.driver, *m_context.window),
	m_frameInfo(*m_context.window)
	#endif
{
	m_context.uiManager->SetCanvas(m_canvas);
	m_canvas.AddPanel(m_fpsCounter);
	#ifdef _DEBUG
	m_canvas.AddPanel(m_driverInfo);
	m_canvas.AddPanel(m_frameInfo);
	m_sceneRenderer.AddFeature<
		OvRendering::Features::FrameInfoRenderFeature,
		OvRendering::Features::EFeatureExecutionPolicy::ALWAYS
	>();
	#endif

	const std::string startupScenePath = OvTools::Utils::PathParser::MakeNonWindowsStyle(m_context.startScene);

	// A scene that cannot be loaded leaves an empty one, so that the run ends cleanly and says why
	if (!m_context.sceneManager.LoadScene(startupScenePath))
	{
		OVLOG_ERROR("The start scene \"" + startupScenePath + "\" cannot be loaded");
		m_startSceneLoaded = false;
		m_context.sceneManager.LoadEmptyScene();
	}

	m_context.sceneManager.GetCurrentScene()->Play();
}

OvGame::Core::Game::~Game()
{
	m_context.sceneManager.UnloadCurrentScene();
}

void OvGame::Core::Game::PreUpdate()
{
	ZoneScoped;

	m_context.device->PollEvents();
	m_context.inputManager->UpdateGamepads();
	m_context.inputActions->Update(OvCore::Input::GameInputSource{});
}

void RenderCurrentScene(
	OvCore::Rendering::SceneRenderer& p_renderer,
	const OvGame::Core::Context& p_context
)
{
	ZoneScoped;

	if (auto currentScene = p_context.sceneManager.GetCurrentScene())
	{
		if (auto camera = currentScene->FindMainCamera())
		{
			auto [windowWidth, windowHeight] = p_context.window->GetSize();

			p_renderer.AddDescriptor<OvCore::Rendering::SceneRenderer::SceneDescriptor>({
				*currentScene,
			});

			OvRendering::Data::FrameDescriptor frameDescriptor;
			frameDescriptor.renderWidth = windowWidth;
			frameDescriptor.renderHeight = windowHeight;
			frameDescriptor.camera = camera->GetCamera();
			frameDescriptor.outputBuffer = *p_context.framebuffer;

			p_renderer.BeginFrame(frameDescriptor);
			p_renderer.DrawFrame();
			p_renderer.EndFrame();
		}
		else
		{
			p_renderer.Clear(true, true, true);
		}
	}
	else
	{
		p_renderer.Clear(true, true, true);
	}
}

void OvGame::Core::Game::Update(float p_deltaTime)
{
	// A minimized window has nothing to draw into (0x0 on Windows): the game goes on, unseen
	const auto [windowWidth, windowHeight] = m_context.window->GetSize();
	const bool drawable = !m_context.window->IsMinimized() && windowWidth > 0 && windowHeight > 0;

	if (auto currentScene = m_context.sceneManager.GetCurrentScene())
	{
		{
			ZoneScopedN("Physics Update");

			if (m_context.physicsEngine->Update(p_deltaTime))
				currentScene->FixedUpdate(p_deltaTime);

			// The rest of the frame sees the interpolated bodies between their last two steps
			m_context.physicsEngine->Interpolate();
		}

		{
			ZoneScopedN("Scene Update");
			currentScene->Update(p_deltaTime);
			currentScene->LateUpdate(p_deltaTime);
		}

		{
			ZoneScopedN("Audio Update");
			m_context.audioEngine->Update();
		}

		if (drawable)
		{
			RenderCurrentScene(m_sceneRenderer, m_context);
			m_context.framebuffer->BlitToBackBuffer(windowWidth, windowHeight);
		}

		{
			// The actors destroyed during the frame are freed at its end, as in the editor's play mode
			ZoneScopedN("Scene garbage collection");
			currentScene->CollectGarbages();
		}
	}

	m_context.sceneManager.Update();

	if  (m_context.inputManager->IsKeyPressed(OvWindowing::Inputs::EKey::KEY_F12))
		m_showDebugInformation = !m_showDebugInformation;

	if (m_showDebugInformation)
	{
		m_fpsCounter.Update(p_deltaTime);
		#ifdef _DEBUG
		auto& frameInfoRenderFeature = m_sceneRenderer.GetFeature<OvRendering::Features::FrameInfoRenderFeature>();
		auto& frameInfo = frameInfoRenderFeature.GetFrameInfo();
		m_frameInfo.Update(frameInfo);
        m_driverInfo.Update();
		#endif

		if (drawable)
			m_context.uiManager->Render();
	}
}

void OvGame::Core::Game::PostUpdate()
{
	ZoneScoped;

	m_context.window->SwapBuffers();
	m_context.inputManager->ClearEvents();
	m_context.driver->OnFrameCompleted();
	m_context.driver->OnFramePresented();
}

bool OvGame::Core::Game::HasStartSceneLoaded() const
{
	return m_startSceneLoaded;
}
