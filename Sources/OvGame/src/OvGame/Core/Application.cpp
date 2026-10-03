/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <tracy/Tracy.hpp>

#include <OvGame/Core/Application.h>

OvGame::Core::Application::Application() :
	m_game(m_context)
{

}

OvGame::Core::Application::~Application()
{
}

void OvGame::Core::Application::Run()
{
	auto& clock = m_context.clock;

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
