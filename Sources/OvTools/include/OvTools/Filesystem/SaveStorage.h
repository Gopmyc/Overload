/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace OvTools::Filesystem
{
	/**
	* How much a save folder may hold
	*/
	struct SaveStorageLimits
	{
		size_t maxFileSize = 4 * 1024 * 1024;
		size_t maxTotalSize = 64 * 1024 * 1024;
		size_t maxEntryCount = 1024;
	};

	/**
	* The one folder where the scripts keep what must outlive a session (saved games, settings), behind a sandbox.
	* A script names a file by a relative path such as "slots/1.json", and nothing else:
	* - a path has 1 to 4 names separated by "/", 255 bytes at most; a name is 1 to 64 lower case letters, digits,
	*   "_" and "-", in parts joined by single dots ("settings.json"), and never a name Windows reserves ("con", "nul",
	*   "com1"...). So no "..", no absolute path, no drive, no backslash: a path can only lead inside the folder;
	* - every name already on the disk along the way must be a plain folder or a plain file: a symbolic link or a
	*   Windows junction planted in the folder is refused, so nothing leads out of it either;
	* - a file holds 4 MiB at most, and the folder 64 MiB and 1024 entries (by default: see SaveStorageLimits), so a
	*   script cannot fill the disk;
	* - a file is written whole or not at all: to a temporary file first, flushed to the disk, then renamed over the
	*   old one, so a crash in the middle leaves the previous version;
	* - what is read is handed back as bytes, never run.
	* The folder is made by the first write. Every function reports what went wrong in its error string: "invalid
	* path", "unsafe path", "not found", "not a file", "not a folder", "folder not empty", "file too large",
	* "storage full", or "io error: ...". It is meant for the main thread.
	*/
	class SaveStorage final
	{
	public:
		static constexpr size_t kMaxDepth = 4;
		static constexpr size_t kMaxNameLength = 64;
		static constexpr size_t kMaxPathLength = 255;

		/**
		* A storage over the given folder, which need not exist yet
		* @param p_root
		* @param p_limits
		*/
		explicit SaveStorage(std::filesystem::path p_root, SaveStorageLimits p_limits = {});

		/**
		* The folder the storage keeps its files in
		*/
		const std::filesystem::path& GetRoot() const;

		/**
		* How much the folder may hold
		*/
		const SaveStorageLimits& GetLimits() const;

		/**
		* The relative path a script's path names, or nothing when a script may not name it
		* @param p_path
		* @param p_allowRoot whether the empty path, the folder itself, is accepted
		*/
		static std::optional<std::filesystem::path> ValidatePath(std::string_view p_path, bool p_allowRoot = false);

		/**
		* Whether a script may use the name for a file or a folder
		* @param p_name
		*/
		static bool IsValidName(std::string_view p_name);

		/**
		* Where an application keeps its saves: "Saves" in a folder named after it (letters, digits, spaces, "_" and
		* "-" kept, "Game" when none is left) in the user's data folder (SystemCalls::GetPathToUserData)
		* @param p_applicationName
		*/
		static std::filesystem::path GetDefaultRoot(std::string_view p_applicationName);

		/**
		* Writes the whole file, making the folders that lead to it
		* @param p_path
		* @param p_data
		* @param p_error
		*/
		bool Write(std::string_view p_path, std::string_view p_data, std::string& p_error);

		/**
		* The whole content of a file
		* @param p_path
		* @param p_error
		*/
		std::optional<std::string> Read(std::string_view p_path, std::string& p_error);

		/**
		* Whether a file or a folder is there
		* @param p_path
		*/
		bool Exists(std::string_view p_path);

		/**
		* Removes a file, or a folder once it is empty
		* @param p_path
		* @param p_error
		*/
		bool Delete(std::string_view p_path, std::string& p_error);

		/**
		* The names in a folder ("" for the storage itself), sorted, a folder's name ending with "/"
		* @param p_path
		* @param p_error
		*/
		std::optional<std::vector<std::string>> List(std::string_view p_path, std::string& p_error);

	private:
		bool IsSafe(const std::filesystem::path& p_relative) const;
		bool HasRoomFor(const std::filesystem::path& p_target, size_t p_size, std::string& p_error) const;
		void RemoveTemporaryFiles();

	private:
		std::filesystem::path m_root;
		SaveStorageLimits m_limits;
		bool m_temporaryFilesRemoved = false;
	};
}
