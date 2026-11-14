/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <OvTests/AudioTestUtils.h>

// Under MSVC, <soloud.h> includes <windows.h>, whose macros (NO_ERROR, near, far...) break other headers of the engine,
// such as baregl's: the tests reach SoLoud from here, after every other header
#include <soloud.h>

float OvTests::Audio::SpeedOf(AudioEngine& p_engine, const OvAudio::Data::SoundInstance& p_instance)
{
	return p_engine.GetBackend().getRelativePlaySpeed(p_instance.GetHandle());
}
