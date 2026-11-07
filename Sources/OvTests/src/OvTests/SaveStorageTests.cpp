/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <string>

#include <doctest/doctest.h>

#include <OvTests/TestUtils.h>
#include <OvTools/Filesystem/SaveStorage.h>

using OvTools::Filesystem::SaveStorage;
using OvTools::Filesystem::SaveStorageLimits;

TEST_SUITE("Save storage")
{
	TEST_CASE("a script may only name paths inside the folder")
	{
		for (const char* path : { "settings.json", "slots/1.json", "a/b/c/d.json", "save_2-b.v1.json" })
		{
			CHECK_MESSAGE(SaveStorage::ValidatePath(path), path);
		}

		for (const char* path : {
			"", "/abs.json", "../up.json", "a/../b.json", "./a.json", "a//b.json", "a/", "Slots/1.json",
			"a\\b.json", "c:/x.json", "a/b/c/d/e.json", "name..json", ".hidden", "trailing.", "con", "nul.json",
			"com1.txt", "with space.json", "\xC3\xA9.json" })
		{
			CHECK_FALSE_MESSAGE(SaveStorage::ValidatePath(path), path);
		}

		CHECK(SaveStorage::ValidatePath("", true));
		CHECK_FALSE(SaveStorage::ValidatePath(std::string(65, 'a')));
		CHECK(SaveStorage::ValidatePath(std::string(64, 'a')));
	}

	TEST_CASE("files are written, read, listed and deleted")
	{
		OvTests::TempFolder folder;
		SaveStorage storage(folder.GetPath() / "Saves");
		std::string error;

		CHECK_FALSE(storage.Exists("slots"));
		REQUIRE(storage.Write("slots/1.json", "{\"level\":3}", error));
		REQUIRE(storage.Write("settings.json", "{}", error));
		CHECK(storage.Exists("slots"));
		CHECK(storage.Exists("slots/1.json"));

		const auto content = storage.Read("slots/1.json", error);
		REQUIRE(content);
		CHECK(*content == "{\"level\":3}");

		REQUIRE(storage.Write("slots/1.json", "{\"level\":4}", error));
		CHECK(*storage.Read("slots/1.json", error) == "{\"level\":4}");

		const auto names = storage.List("", error);
		REQUIRE(names);
		CHECK(*names == std::vector<std::string>{ "settings.json", "slots/" });

		CHECK_FALSE(storage.Delete("slots", error));
		CHECK(error == "folder not empty");
		CHECK(storage.Delete("slots/1.json", error));
		CHECK(storage.Delete("slots", error));
		CHECK_FALSE(storage.Exists("slots"));
	}

	TEST_CASE("each failure says what went wrong")
	{
		OvTests::TempFolder folder;
		SaveStorage storage(folder.GetPath());
		std::string error;

		CHECK_FALSE(storage.Write("../x.json", "x", error));
		CHECK(error == "invalid path");

		CHECK_FALSE(storage.Read("missing.json", error));
		CHECK(error == "not found");

		REQUIRE(storage.Write("folder/file.json", "x", error));
		CHECK_FALSE(storage.Read("folder", error));
		CHECK(error == "not a file");
		CHECK_FALSE(storage.List("folder/file.json", error));
		CHECK(error == "not a folder");
		CHECK_FALSE(storage.Delete("missing.json", error));
		CHECK(error == "not found");
	}

	TEST_CASE("the limits hold")
	{
		OvTests::TempFolder folder;
		SaveStorageLimits limits;
		limits.maxFileSize = 8;
		limits.maxTotalSize = 10;
		limits.maxEntryCount = 3;
		SaveStorage storage(folder.GetPath(), limits);
		std::string error;

		CHECK_FALSE(storage.Write("big.json", std::string(9, 'x'), error));
		CHECK(error == "file too large");

		REQUIRE(storage.Write("a.json", std::string(8, 'x'), error));
		CHECK_FALSE(storage.Write("b.json", std::string(3, 'x'), error));
		CHECK(error == "storage full");

		// Replacing a file counts its new size only
		CHECK(storage.Write("a.json", std::string(7, 'x'), error));
		CHECK(storage.Write("b.json", std::string(3, 'x'), error));

		// A folder is an entry too
		CHECK_FALSE(storage.Write("c/d.json", "", error));
		CHECK(error == "storage full");
		CHECK(storage.Write("c.json", "", error));
		CHECK_FALSE(storage.Write("d.json", "", error));
		CHECK(error == "storage full");
	}

	TEST_CASE("the folders a write makes count against the entries")
	{
		OvTests::TempFolder folder;
		SaveStorageLimits limits;
		limits.maxEntryCount = 2;
		SaveStorage storage(folder.GetPath() / "Saves", limits);
		std::string error;

		CHECK_FALSE(storage.Write("a/b/c.json", "", error));
		CHECK(error == "storage full");
		CHECK_FALSE(storage.Exists("a"));
		CHECK(storage.Write("a/c.json", "", error));
		CHECK(storage.Write("a/c.json", "again", error));
		CHECK_FALSE(storage.Write("a/d.json", "", error));
	}

#ifndef _WIN32
	TEST_CASE("a link planted in the folder is never followed")
	{
		OvTests::TempFolder folder;
		const auto outside = folder.WriteFile("outside/secret.json", "secret");
		std::filesystem::create_directories(folder.GetPath() / "Saves");
		std::filesystem::create_directory_symlink(folder.GetPath() / "outside", folder.GetPath() / "Saves" / "link");
		std::filesystem::create_symlink(outside, folder.GetPath() / "Saves" / "file.json");

		SaveStorage storage(folder.GetPath() / "Saves");
		std::string error;

		CHECK_FALSE(storage.Read("link/secret.json", error));
		CHECK(error == "unsafe path");
		CHECK_FALSE(storage.Write("link/new.json", "x", error));
		CHECK(error == "unsafe path");
		CHECK_FALSE(storage.Read("file.json", error));
		CHECK(error == "unsafe path");
		CHECK_FALSE(std::filesystem::exists(folder.GetPath() / "outside" / "new.json"));
	}
#endif

	TEST_CASE("the default folder is named after the application")
	{
		CHECK(SaveStorage::GetDefaultRoot("My Game!").parent_path().filename() == "My Game");
		CHECK(SaveStorage::GetDefaultRoot("../..").parent_path().filename() == "Game");
		CHECK(SaveStorage::GetDefaultRoot("CON").parent_path().filename() == "Game");
		CHECK(SaveStorage::GetDefaultRoot("Adventurers").filename() == "Saves");
	}
}
