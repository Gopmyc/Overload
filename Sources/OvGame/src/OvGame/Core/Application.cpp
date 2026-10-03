/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <tracy/Tracy.hpp>

#include <OvCore/Global/QuitRequest.h>
#include <OvGame/Core/Application.h>
#include <OvTools/Time/Clock.h>

OvGame::Core::Application::Application() :
	m_game(m_context)
{
	m_quitListener = OvCore::Global::QuitRequest::RequestedEvent += [this]
	{
		m_context.window->SetShouldClose(true);
	};
}

OvGame::Core::Application::~Application()
{
	OvCore::Global::QuitRequest::RequestedEvent -= m_quitListener;
}

void OvGame::Core::Application::Run()
{
	OvTools::Time::Clock clock;

	while (IsRunning())
	{
		m_game.PreUpdate();
		m_game.Update(clock.GetDeltaTime());
		m_game.PostUpdate();
		clock.Update();
		FrameMark;
	}
}

bool OvGame::Core::Application::IsRunning() const
{
	return !m_context.window->ShouldClose();
}
