/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <soloud_wav.h>

#include <OvDebug/Logger.h>

#include <OvAudio/Resources/Loaders/SoundLoader.h>

OvAudio::Resources::Sound* OvAudio::Resources::Loaders::SoundLoader::Create(const std::string& p_filepath)
{
	std::unique_ptr<SoLoud::Wav> wav = std::make_unique<SoLoud::Wav>();

	// A missing or unreadable file gives no sound, rather than a sound that plays nothing
	if (wav->load(p_filepath.c_str()) != SoLoud::SO_NO_ERROR)
	{
		OVLOG_WARNING("Cannot read the sound \"" + p_filepath + "\": a WAV, OGG, MP3 or FLAC file was expected");
		return nullptr;
	}

	return new Sound(p_filepath, std::move(wav));
}

void OvAudio::Resources::Loaders::SoundLoader::Reload(Sound& p_sound, const std::string& p_path)
{
	*const_cast<std::string*>(&p_sound.path) = p_path;
	p_sound.audioData->load(p_path.c_str());
}

bool OvAudio::Resources::Loaders::SoundLoader::Destroy(Sound*& p_soundInstance)
{
	if (p_soundInstance)
	{
		delete p_soundInstance;
		p_soundInstance = nullptr;

		return true;
	}

	return false;
}
