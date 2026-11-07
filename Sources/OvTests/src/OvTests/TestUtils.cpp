/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <fstream>
#include <random>

#include <OvDebug/Logger.h>
#include <OvTests/TestUtils.h>

OvTests::TempFolder::TempFolder()
{
	std::random_device device;
	std::mt19937_64 generator(device());

	do
	{
		m_path = std::filesystem::temp_directory_path() / ("ovtests-" + std::to_string(generator()));
	} while (std::filesystem::exists(m_path));

	std::filesystem::create_directories(m_path);
}

OvTests::TempFolder::~TempFolder()
{
	std::error_code error;
	std::filesystem::remove_all(m_path, error);
}

const std::filesystem::path& OvTests::TempFolder::GetPath() const
{
	return m_path;
}

std::filesystem::path OvTests::TempFolder::WriteFile(const std::filesystem::path& p_relativePath, std::string_view p_content) const
{
	const auto path = m_path / p_relativePath;
	std::filesystem::create_directories(path.parent_path());

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	file.write(p_content.data(), static_cast<std::streamsize>(p_content.size()));
	return path;
}

OvTests::LogCapture::LogCapture()
{
	m_listener = OvDebug::Logger::LogEvent += [this](const OvDebug::LogData& p_log)
	{
		m_logs.push_back(p_log);
	};
}

OvTests::LogCapture::~LogCapture()
{
	OvDebug::Logger::LogEvent -= m_listener;
}

size_t OvTests::LogCapture::Count(OvDebug::ELogLevel p_level) const
{
	return static_cast<size_t>(std::count_if(m_logs.begin(), m_logs.end(), [p_level](const OvDebug::LogData& p_log)
	{
		return p_log.logLevel == p_level;
	}));
}

bool OvTests::LogCapture::Contains(OvDebug::ELogLevel p_level, std::string_view p_text) const
{
	return std::any_of(m_logs.begin(), m_logs.end(), [p_level, p_text](const OvDebug::LogData& p_log)
	{
		return p_log.logLevel == p_level && p_log.message.find(p_text) != std::string::npos;
	});
}
