/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <OvTools/Utils/Json.h>

namespace OvTools::Filesystem
{
	class SaveStorage;
}

/**
* The envelope a save file keeps its version in, {"version": n, "data": ...}, as the standard library's SaveFile writes
* it for the scripts (Docs/Saving.md). The engine's own files use it too
*/
namespace OvCore::Helpers::VersionedSave
{
	/**
	* A file's version and data
	*/
	struct Unwrapped
	{
		int version = 0;
		OvTools::Utils::Json::Value data;
	};

	/**
	* Returns the envelope of the data at this version (1 or more)
	* @param p_version
	* @param p_data
	*/
	OvTools::Utils::Json::Value Wrap(int p_version, OvTools::Utils::Json::Value p_data);

	/**
	* Returns the version and the data of a file's JSON: an envelope's (an object of exactly "version", a whole number
	* of 1 or more, and "data", an object or an array), or version 0 and the whole value
	* @param p_value
	*/
	Unwrapped Unwrap(const OvTools::Utils::Json::Value& p_value);

	/**
	* Returns the name a file is set aside under: "settings.json" and "broken" give "settings.broken.json"
	* @param p_name
	* @param p_suffix
	*/
	std::string AsideName(std::string_view p_name, std::string_view p_suffix);

	/**
	* Reads a save file whose data has had the same shape from version 0 to p_version: its data, or nothing when there
	* is no file, or none to use. A file that is no JSON is copied beside it as <name>.broken.json, one of a newer
	* version as <name>.v<n>.json, with a warning: the defaults stand, and the next write replaces it. A file that
	* cannot be read is warned about
	* @param p_storage
	* @param p_name
	* @param p_version
	*/
	std::optional<OvTools::Utils::Json::Value> Read(OvTools::Filesystem::SaveStorage& p_storage, std::string_view p_name, int p_version);

	/**
	* Writes the data at this version, in its envelope; a failure is warned about. Returns whether it was written
	* @param p_storage
	* @param p_name
	* @param p_version
	* @param p_data
	*/
	bool Write(OvTools::Filesystem::SaveStorage& p_storage, std::string_view p_name, int p_version, OvTools::Utils::Json::Value p_data);
}
