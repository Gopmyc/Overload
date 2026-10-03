/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <tracy/Tracy.hpp>

#include <OvEditor/Core/Application.h>

OvEditor::Core::Application::Application(const std::filesystem::path& p_projectFolder) :
	m_context(p_projectFolder),
	m_editor(m_context)
{
}

OvEditor::Core::Application::~Application()
{
}

void OvEditor::Core::Application::Run()
{
	auto& clock = m_context.clock;

	while (IsRunning())
	{
		m_editor.PreUpdate();
		m_editor.Update(clock.GetDeltaTime());
		m_editor.PostUpdate();
		clock.Update();
		FrameMark;
	}
}

bool OvEditor::Core::Application::IsRunning() const
{
	return !m_context.window->ShouldClose();
}
