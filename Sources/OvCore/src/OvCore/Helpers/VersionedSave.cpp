/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <cmath>
#include <format>

#include <OvCore/Helpers/VersionedSave.h>
#include <OvDebug/Logger.h>
#include <OvTools/Filesystem/SaveStorage.h>

using OvTools::Utils::Json::Value;

OvTools::Utils::Json::Value OvCore::Helpers::VersionedSave::Wrap(int p_version, Value p_data)
{
	return Value(OvTools::Utils::Json::Object{
		{ "version", Value(p_version) },
		{ "data", std::move(p_data) }
	});
}

OvCore::Helpers::VersionedSave::Unwrapped OvCore::Helpers::VersionedSave::Unwrap(const Value& p_value)
{
	if (const auto* members = p_value.AsObject(); members && members->size() == 2)
	{
		const Value* version = p_value.Find("version");
		const Value* data = p_value.Find("data");
		const double* number = version ? version->AsNumber() : nullptr;

		if (number && data && (data->IsObject() || data->IsArray()) &&
			*number >= 1.0 && *number <= 1000000.0 && std::floor(*number) == *number)
		{
			return { static_cast<int>(*number), *data };
		}
	}

	return { 0, p_value };
}

std::string OvCore::Helpers::VersionedSave::AsideName(std::string_view p_name, std::string_view p_suffix)
{
	const size_t dot = p_name.find_last_of('.');
	const size_t slash = p_name.find_last_of('/');

	// An extension: a dot after the last folder, with a stem before it
	if (dot != std::string_view::npos && dot > 0 && (slash == std::string_view::npos || dot > slash + 1))
	{
		return std::format("{}.{}{}", p_name.substr(0, dot), p_suffix, p_name.substr(dot));
	}

	return std::format("{}.{}", p_name, p_suffix);
}

namespace
{
	void SetAside(OvTools::Filesystem::SaveStorage& p_storage, std::string_view p_name, const std::string& p_text, std::string_view p_suffix, const std::string& p_reason)
	{
		const std::string aside = OvCore::Helpers::VersionedSave::AsideName(p_name, p_suffix);
		std::string error;

		if (p_storage.Write(aside, p_text, error))
		{
			OVLOG_WARNING(std::format("{}: {}; set aside as {}, the defaults stand", p_name, p_reason, aside));
		}
		else
		{
			OVLOG_WARNING(std::format("{}: {}; it could not be set aside as {} ({})", p_name, p_reason, aside, error));
		}
	}
}

std::optional<OvTools::Utils::Json::Value> OvCore::Helpers::VersionedSave::Read(OvTools::Filesystem::SaveStorage& p_storage, std::string_view p_name, int p_version)
{
	std::string error;
	const auto text = p_storage.Read(p_name, error);

	if (!text)
	{
		if (error != "not found")
		{
			OVLOG_WARNING(std::format("{}: {}", p_name, error));
		}

		return std::nullopt;
	}

	std::string parseError;
	const auto document = OvTools::Utils::Json::Parse(*text, parseError);

	if (!document)
	{
		SetAside(p_storage, p_name, *text, "broken", "not JSON, " + parseError);
		return std::nullopt;
	}

	Unwrapped unwrapped = Unwrap(*document);

	if (unwrapped.version > p_version)
	{
		SetAside(p_storage, p_name, *text, std::format("v{}", unwrapped.version),
			std::format("version {} is newer than the engine's, {}", unwrapped.version, p_version));
		return std::nullopt;
	}

	return std::move(unwrapped.data);
}

bool OvCore::Helpers::VersionedSave::Write(OvTools::Filesystem::SaveStorage& p_storage, std::string_view p_name, int p_version, Value p_data)
{
	std::string error;

	if (!p_storage.Write(p_name, OvTools::Utils::Json::Write(Wrap(p_version, std::move(p_data))), error))
	{
		OVLOG_WARNING(std::format("{}: cannot be written, {}", p_name, error));
		return false;
	}

	return true;
}
