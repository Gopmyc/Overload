/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <OvDebug/ILogHandler.h>
#include <OvTools/Eventing/Event.h>

namespace OvTests
{
	/**
	* Keeps the path the test program was started with, to find the engine's resources from it
	* @param p_argument0
	*/
	void SetProgramPath(const char* p_argument0);

	/**
	* The engine's resources (Resources/Engine of the checkout), looked for above the program's folder, then above the
	* current folder. Empty when neither leads to them
	*/
	std::filesystem::path FindEngineResources();

	/**
	* A folder of its own in the system's temporary folder, removed with everything in it when the test is done
	*/
	class TempFolder final
	{
	public:
		TempFolder();
		~TempFolder();

		TempFolder(const TempFolder&) = delete;
		TempFolder& operator=(const TempFolder&) = delete;

		const std::filesystem::path& GetPath() const;

		/**
		* Writes a file under the folder, making the folders that lead to it, and returns its path
		* @param p_relativePath
		* @param p_content
		*/
		std::filesystem::path WriteFile(const std::filesystem::path& p_relativePath, std::string_view p_content) const;

	private:
		std::filesystem::path m_path;
	};

	/**
	* The logs written while it lives
	*/
	class LogCapture final
	{
	public:
		LogCapture();
		~LogCapture();

		LogCapture(const LogCapture&) = delete;
		LogCapture& operator=(const LogCapture&) = delete;

		/**
		* How many logs of the level were written
		* @param p_level
		*/
		size_t Count(OvDebug::ELogLevel p_level) const;

		/**
		* Whether a log of the level holds the text
		* @param p_level
		* @param p_text
		*/
		bool Contains(OvDebug::ELogLevel p_level, std::string_view p_text) const;

	private:
		std::vector<OvDebug::LogData> m_logs;
		OvTools::Eventing::ListenerID m_listener;
	};
}
