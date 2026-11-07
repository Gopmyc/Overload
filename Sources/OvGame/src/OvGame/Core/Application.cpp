/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <vector>

#include <tracy/Tracy.hpp>

#include <OvCore/Global/QuitRequest.h>
#include <OvDebug/Logger.h>
#include <OvGame/Core/Application.h>
#include <OvGame/Utils/PngWriter.h>

OvGame::Core::Application::Application(const LaunchOptions& p_options) :
	m_options(p_options),
	m_context(m_options),
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

int OvGame::Core::Application::Run()
{
	auto& clock = m_context.clock;
	uint32_t frame = 0;

	// A game that quits before its last frame leaves no screenshot
	bool screenshotWritten = !m_options.screenshot;

	while (IsRunning())
	{
		m_game.PreUpdate();
		m_game.Update(clock.GetDeltaTime());

		// The last frame the options ask for: drawn, captured, then the game quits
		if (m_options.frames && ++frame >= *m_options.frames)
		{
			if (m_options.screenshot)
				screenshotWritten = TakeScreenshot();

			m_context.window->SetShouldClose(true);
		}

		m_game.PostUpdate();
		clock.Update();
		FrameMark;
	}

	if (!m_game.HasStartSceneLoaded())
		return 1;

	return screenshotWritten ? 0 : 3;
}

bool OvGame::Core::Application::TakeScreenshot() const
{
	const auto [width, height] = m_context.framebuffer->GetSize();

	// Read as RGBA, whose rows need no padding, then kept as RGB from top to bottom
	std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4);
	m_context.framebuffer->ReadPixels(0, 0, width, height, baregl::types::EPixelDataFormat::RGBA, baregl::types::EPixelDataType::UNSIGNED_BYTE, rgba.data());

	std::vector<uint8_t> rgb(static_cast<size_t>(width) * height * 3);
	for (uint32_t row = 0; row < height; ++row)
	{
		const uint8_t* source = rgba.data() + static_cast<size_t>(height - 1 - row) * width * 4;
		uint8_t* target = rgb.data() + static_cast<size_t>(row) * width * 3;

		for (uint32_t column = 0; column < width; ++column)
		{
			target[column * 3 + 0] = source[column * 4 + 0];
			target[column * 3 + 1] = source[column * 4 + 1];
			target[column * 3 + 2] = source[column * 4 + 2];
		}
	}

	if (!Utils::WritePng(*m_options.screenshot, width, height, rgb))
	{
		OVLOG_ERROR("The screenshot cannot be written to " + m_options.screenshot->string());
		return false;
	}

	OVLOG_INFO("Screenshot written to " + m_options.screenshot->string());
	return true;
}

bool OvGame::Core::Application::IsRunning() const
{
	return !m_context.window->ShouldClose();
}
