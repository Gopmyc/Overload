/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <OvTools/Filesystem/SaveStorage.h>
#include <OvTools/Utils/SystemCalls.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <system_error>

#ifdef _WIN32
#include <io.h>
#include <share.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace
{
	constexpr std::string_view kTemporarySuffix = ".~tmp";

	constexpr std::array<std::string_view, 22> kReservedNames = {
		"con", "prn", "aux", "nul",
		"com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
		"lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9"
	};

	bool IsNameCharacter(char p_character)
	{
		return
			(p_character >= 'a' && p_character <= 'z') ||
			(p_character >= '0' && p_character <= '9') ||
			p_character == '_' ||
			p_character == '-';
	}

	FILE* OpenFile(const std::filesystem::path& p_path, bool p_write)
	{
#ifdef _WIN32
		// _wfsopen shares the file as _wfopen does, without the deprecation MSVC turns into an error
		return _wfsopen(p_path.c_str(), p_write ? L"wb" : L"rb", _SH_DENYNO);
#else
		return std::fopen(p_path.c_str(), p_write ? "wb" : "rb");
#endif
	}

	// Down to the disk, not only out of the process: a rename after a crash must not reveal an empty file
	bool SyncFile(FILE* p_file)
	{
#ifdef _WIN32
		return _commit(_fileno(p_file)) == 0;
#else
		return fsync(fileno(p_file)) == 0;
#endif
	}

	void SyncFolder([[maybe_unused]] const std::filesystem::path& p_folder)
	{
#ifndef _WIN32
		const int descriptor = open(p_folder.c_str(), O_RDONLY | O_DIRECTORY);

		if (descriptor >= 0)
		{
			fsync(descriptor);
			close(descriptor);
		}
#endif
	}

	bool IsTemporary(const std::filesystem::path& p_path)
	{
		const std::string name = p_path.filename().string();

		return name.size() > kTemporarySuffix.size() && name.ends_with(kTemporarySuffix);
	}
}

OvTools::Filesystem::SaveStorage::SaveStorage(std::filesystem::path p_root, SaveStorageLimits p_limits) :
	m_root(std::move(p_root)),
	m_limits(p_limits)
{
}

const std::filesystem::path& OvTools::Filesystem::SaveStorage::GetRoot() const
{
	return m_root;
}

const OvTools::Filesystem::SaveStorageLimits& OvTools::Filesystem::SaveStorage::GetLimits() const
{
	return m_limits;
}

bool OvTools::Filesystem::SaveStorage::IsValidName(std::string_view p_name)
{
	if (p_name.empty() || p_name.size() > kMaxNameLength)
	{
		return false;
	}

	// Parts of name characters joined by single dots: no leading, trailing or doubled dot, so never "." nor ".."
	bool partStarted = false;

	for (const char character : p_name)
	{
		if (character == '.')
		{
			if (!partStarted)
			{
				return false;
			}

			partStarted = false;
		}
		else if (IsNameCharacter(character))
		{
			partStarted = true;
		}
		else
		{
			return false;
		}
	}

	if (!partStarted)
	{
		return false;
	}

	// Windows opens a device for these names, whatever their extension
	const std::string_view stem = p_name.substr(0, p_name.find('.'));

	return std::find(kReservedNames.begin(), kReservedNames.end(), stem) == kReservedNames.end();
}

std::filesystem::path OvTools::Filesystem::SaveStorage::GetDefaultRoot(std::string_view p_applicationName)
{
	std::string folder;

	for (const char character : p_applicationName)
	{
		const bool kept =
			(character >= 'a' && character <= 'z') ||
			(character >= 'A' && character <= 'Z') ||
			(character >= '0' && character <= '9') ||
			character == '_' || character == '-' || character == ' ';

		if (kept && !(character == ' ' && (folder.empty() || folder.back() == ' ')))
		{
			folder += character;
		}
	}

	while (!folder.empty() && folder.back() == ' ')
	{
		folder.pop_back();
	}

	// A name Windows reserves for a device would open that device instead of a folder
	std::string lower = folder;
	std::transform(lower.begin(), lower.end(), lower.begin(), [](char c) { return static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c); });

	if (folder.empty() || std::find(kReservedNames.begin(), kReservedNames.end(), lower) != kReservedNames.end())
	{
		folder = "Game";
	}

	return std::filesystem::path{ OvTools::Utils::SystemCalls::GetPathToUserData() } / folder / "Saves";
}

std::optional<std::filesystem::path> OvTools::Filesystem::SaveStorage::ValidatePath(std::string_view p_path, bool p_allowRoot)
{
	if (p_path.empty())
	{
		return p_allowRoot ? std::optional<std::filesystem::path>{ std::filesystem::path{} } : std::nullopt;
	}

	if (p_path.size() > kMaxPathLength)
	{
		return std::nullopt;
	}

	std::filesystem::path relative;
	size_t depth = 0;
	size_t start = 0;

	while (start <= p_path.size())
	{
		const size_t end = std::min(p_path.find('/', start), p_path.size());
		const std::string_view name = p_path.substr(start, end - start);

		if (!IsValidName(name) || ++depth > kMaxDepth)
		{
			return std::nullopt;
		}

		relative /= std::string(name);
		start = end + 1;
	}

	return relative;
}

bool OvTools::Filesystem::SaveStorage::IsSafe(const std::filesystem::path& p_relative) const
{
	std::filesystem::path current = m_root;

	for (const auto& name : p_relative)
	{
		current /= name;

		std::error_code error;
		const auto status = std::filesystem::symlink_status(current, error);

		// Nothing further exists, so nothing further can lead elsewhere
		if (status.type() == std::filesystem::file_type::not_found)
		{
			return true;
		}

		if (error)
		{
			return false;
		}

		// Plain folders and files only: a link, a junction or a device is refused, whatever the platform calls it
		if (status.type() != std::filesystem::file_type::directory && status.type() != std::filesystem::file_type::regular)
		{
			return false;
		}
	}

	return true;
}

bool OvTools::Filesystem::SaveStorage::HasRoomFor(const std::filesystem::path& p_target, size_t p_size, std::string& p_error) const
{
	std::error_code error;

	if (!std::filesystem::exists(m_root, error))
	{
		return true;
	}

	size_t total = 0;
	size_t entries = 0;
	bool replaces = false;

	// Links are never followed, and an entry the walk cannot read is left out rather than failing the write
	for (auto it = std::filesystem::recursive_directory_iterator(m_root, std::filesystem::directory_options::skip_permission_denied, error);
		!error && it != std::filesystem::recursive_directory_iterator();
		it.increment(error))
	{
		std::error_code entryError;
		const auto status = it->symlink_status(entryError);

		if (entryError || IsTemporary(it->path()))
		{
			continue;
		}

		++entries;

		if (status.type() == std::filesystem::file_type::regular)
		{
			if (it->path() == p_target)
			{
				replaces = true;
				continue;
			}

			total += static_cast<size_t>(it->file_size(entryError));
		}
	}

	if (total + p_size > m_limits.maxTotalSize || entries + (replaces ? 0 : 1) > m_limits.maxEntryCount)
	{
		p_error = "storage full";
		return false;
	}

	return true;
}

void OvTools::Filesystem::SaveStorage::RemoveTemporaryFiles()
{
	m_temporaryFilesRemoved = true;

	std::error_code error;
	std::vector<std::filesystem::path> leftovers;

	for (auto it = std::filesystem::recursive_directory_iterator(m_root, std::filesystem::directory_options::skip_permission_denied, error);
		!error && it != std::filesystem::recursive_directory_iterator();
		it.increment(error))
	{
		std::error_code entryError;

		if (it->symlink_status(entryError).type() == std::filesystem::file_type::regular && IsTemporary(it->path()))
		{
			leftovers.push_back(it->path());
		}
	}

	for (const auto& path : leftovers)
	{
		std::filesystem::remove(path, error);
	}
}

bool OvTools::Filesystem::SaveStorage::Write(std::string_view p_path, std::string_view p_data, std::string& p_error)
{
	const auto relative = ValidatePath(p_path);

	if (!relative)
	{
		p_error = "invalid path";
		return false;
	}

	if (p_data.size() > m_limits.maxFileSize)
	{
		p_error = "file too large";
		return false;
	}

	if (!IsSafe(*relative))
	{
		p_error = "unsafe path";
		return false;
	}

	const std::filesystem::path target = m_root / *relative;
	std::error_code error;

	if (std::filesystem::is_directory(std::filesystem::symlink_status(target, error)))
	{
		p_error = "not a file";
		return false;
	}

	if (!HasRoomFor(target, p_data.size(), p_error))
	{
		return false;
	}

	std::filesystem::create_directories(target.parent_path(), error);

	if (error || !std::filesystem::is_directory(std::filesystem::symlink_status(target.parent_path(), error)))
	{
		p_error = "io error: cannot make the folder";
		return false;
	}

	if (!m_temporaryFilesRemoved)
	{
		RemoveTemporaryFiles();
	}

	std::filesystem::path temporary = target;
	temporary += std::string(kTemporarySuffix);

	FILE* file = OpenFile(temporary, true);

	if (file == nullptr)
	{
		p_error = "io error: cannot open the file";
		return false;
	}

	const bool written =
		std::fwrite(p_data.data(), 1, p_data.size(), file) == p_data.size() &&
		std::fflush(file) == 0 &&
		SyncFile(file);

	const bool closed = std::fclose(file) == 0;

	if (!written || !closed)
	{
		std::filesystem::remove(temporary, error);
		p_error = "io error: cannot write the file";
		return false;
	}

	std::filesystem::rename(temporary, target, error);

	if (error)
	{
		std::error_code removeError;
		std::filesystem::remove(temporary, removeError);
		p_error = "io error: cannot replace the file";
		return false;
	}

	SyncFolder(target.parent_path());
	return true;
}

std::optional<std::string> OvTools::Filesystem::SaveStorage::Read(std::string_view p_path, std::string& p_error)
{
	const auto relative = ValidatePath(p_path);

	if (!relative)
	{
		p_error = "invalid path";
		return std::nullopt;
	}

	if (!IsSafe(*relative))
	{
		p_error = "unsafe path";
		return std::nullopt;
	}

	const std::filesystem::path target = m_root / *relative;
	std::error_code error;
	const auto status = std::filesystem::symlink_status(target, error);

	if (status.type() == std::filesystem::file_type::not_found)
	{
		p_error = "not found";
		return std::nullopt;
	}

	if (status.type() != std::filesystem::file_type::regular)
	{
		p_error = "not a file";
		return std::nullopt;
	}

	const auto size = std::filesystem::file_size(target, error);

	if (error)
	{
		p_error = "io error: cannot read the file";
		return std::nullopt;
	}

	if (size > m_limits.maxFileSize)
	{
		p_error = "file too large";
		return std::nullopt;
	}

	FILE* file = OpenFile(target, false);

	if (file == nullptr)
	{
		p_error = "io error: cannot open the file";
		return std::nullopt;
	}

	std::string data(static_cast<size_t>(size), '\0');
	const size_t read = std::fread(data.data(), 1, data.size(), file);
	const bool atEnd = std::fgetc(file) == EOF;

	std::fclose(file);

	// The file changed size under us: report it rather than hand back a part of it
	if (read != data.size() || !atEnd)
	{
		p_error = "io error: cannot read the file";
		return std::nullopt;
	}

	return data;
}

bool OvTools::Filesystem::SaveStorage::Exists(std::string_view p_path)
{
	const auto relative = ValidatePath(p_path);

	if (!relative || !IsSafe(*relative))
	{
		return false;
	}

	std::error_code error;
	const auto type = std::filesystem::symlink_status(m_root / *relative, error).type();

	return type == std::filesystem::file_type::regular || type == std::filesystem::file_type::directory;
}

bool OvTools::Filesystem::SaveStorage::Delete(std::string_view p_path, std::string& p_error)
{
	const auto relative = ValidatePath(p_path);

	if (!relative)
	{
		p_error = "invalid path";
		return false;
	}

	if (!IsSafe(*relative))
	{
		p_error = "unsafe path";
		return false;
	}

	const std::filesystem::path target = m_root / *relative;
	std::error_code error;
	const auto type = std::filesystem::symlink_status(target, error).type();

	if (type == std::filesystem::file_type::not_found)
	{
		p_error = "not found";
		return false;
	}

	if (type == std::filesystem::file_type::directory && !std::filesystem::is_empty(target, error))
	{
		p_error = "folder not empty";
		return false;
	}

	if (!std::filesystem::remove(target, error) || error)
	{
		p_error = "io error: cannot remove it";
		return false;
	}

	return true;
}

std::optional<std::vector<std::string>> OvTools::Filesystem::SaveStorage::List(std::string_view p_path, std::string& p_error)
{
	const auto relative = ValidatePath(p_path, true);

	if (!relative)
	{
		p_error = "invalid path";
		return std::nullopt;
	}

	if (!IsSafe(*relative))
	{
		p_error = "unsafe path";
		return std::nullopt;
	}

	const std::filesystem::path folder = relative->empty() ? m_root : m_root / *relative;
	std::error_code error;
	const auto type = std::filesystem::symlink_status(folder, error).type();

	// The storage itself is there before its first write, empty
	if (type == std::filesystem::file_type::not_found && relative->empty())
	{
		return std::vector<std::string>{};
	}

	if (type == std::filesystem::file_type::not_found)
	{
		p_error = "not found";
		return std::nullopt;
	}

	if (type != std::filesystem::file_type::directory)
	{
		p_error = "not a folder";
		return std::nullopt;
	}

	std::vector<std::string> names;

	for (auto it = std::filesystem::directory_iterator(folder, error);
		!error && it != std::filesystem::directory_iterator();
		it.increment(error))
	{
		std::error_code entryError;
		const auto entryType = it->symlink_status(entryError).type();
		const std::string name = it->path().filename().string();

		// Only what a script could name: no temporary file, no link, nothing put there under another name
		if (entryError || !IsValidName(name))
		{
			continue;
		}

		if (entryType == std::filesystem::file_type::regular)
		{
			names.push_back(name);
		}
		else if (entryType == std::filesystem::file_type::directory)
		{
			names.push_back(name + "/");
		}
	}

	if (error)
	{
		p_error = "io error: cannot list the folder";
		return std::nullopt;
	}

	std::sort(names.begin(), names.end());
	return names;
}
