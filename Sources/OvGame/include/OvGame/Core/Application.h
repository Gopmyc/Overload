/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <OvTools/Eventing/Event.h>

#include "OvGame/Core/Context.h"
#include "OvGame/Core/Game.h"

namespace OvGame::Core
{
	/**
	* Entry point of OvGame
	*/
	class Application
	{
	public:
		/**
		* Constructor
		* @param p_options
		*/
		Application(const LaunchOptions& p_options);

		/**
		* Destructor
		*/
		~Application();

		/**
		* Runs the game until it quits, or for the frames the options ask, and returns the exit code: 0 on success, 1 when
		* the start scene could not be loaded, 3 when the screenshot was not written. Errors logged during the run
		* are for the caller to count (the game logs some of them while it is destroyed)
		*/
		int Run();

		/**
		* Returns true if the app is running
		*/
		bool IsRunning() const;

	private:
		/**
		* Writes the last frame drawn to the options' screenshot, and returns whether it could
		*/
		bool TakeScreenshot() const;

	private:
		const LaunchOptions m_options;
		Context m_context;
		Game m_game;
		OvTools::Eventing::ListenerID m_quitListener;
	};
}