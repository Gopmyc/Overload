/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <string>

#include <OvCore/Scripting/Lua/LuaComponentRef.h>

#include <OvAudio/Core/AudioEngine.h>
#include <OvCore/Global/ServiceLocator.h>

void BindLuaAudio(sol::state& p_luaState)
{
	p_luaState.create_named_table("Audio",
		"GetDriver", []() { return std::string(OVSERVICE(OvAudio::Core::AudioEngine).GetDriverName()); },
		"GetVoiceCount", []() { return OVSERVICE(OvAudio::Core::AudioEngine).GetVoiceCount(); }
	);
}
