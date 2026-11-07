/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/



#include "OvCore/Scripting/Common/ScriptPropertyValue.h"

#include <charconv>
#include <format>
#include <stdexcept>

#include "OvCore/ECS/Actor.h"
#include "OvCore/Global/ServiceLocator.h"
#include "OvCore/SceneSystem/SceneManager.h"

namespace OvCore::Scripting
{
	bool ActorRef::IsAlive() const
	{
		if (guid == 0)
			return false;

		auto* scene = OVSERVICE(OvCore::SceneSystem::SceneManager).GetCurrentScene();
		if (!scene)
			return false;

		auto* actor = scene->FindActorByGUID(guid);
		return actor && actor->IsAlive();
	}

	OvCore::ECS::Actor& ActorRef::Resolve() const
	{
    if (guid == 0)
        throw std::runtime_error("attempt to use a null actor reference");

    auto* scene = OVSERVICE(OvCore::SceneSystem::SceneManager).GetCurrentScene();
    if (!scene)
        throw std::runtime_error("ActorRef: no active scene");

    auto* actor = scene->FindActorByGUID(guid);
    if (!actor || !actor->IsAlive())
        throw std::runtime_error(std::format(
            "attempt to use a destroyed actor (GUID: {:016X})", guid));

    return *actor;
	}
}

namespace
{
	std::string_view Trim(std::string_view p_text)
	{
		constexpr std::string_view kBlanks = " \t\r\n";
		const size_t first = p_text.find_first_not_of(kBlanks);
		if (first == std::string_view::npos)
			return {};
		return p_text.substr(first, p_text.find_last_not_of(kBlanks) - first + 1);
	}
}

std::optional<OvCore::Scripting::ListValue::EItemType> OvCore::Scripting::ListValue::ParseItemType(std::string_view p_name)
{
	if (p_name == "number") return EItemType::NUMBER;
	if (p_name == "string") return EItemType::STRING;
	if (p_name == "bool") return EItemType::BOOL;
	return std::nullopt;
}

std::string_view OvCore::Scripting::ListValue::GetItemTypeName(EItemType p_itemType)
{
	switch (p_itemType)
	{
	case EItemType::NUMBER: return "number";
	case EItemType::STRING: return "string";
	case EItemType::BOOL: return "bool";
	}

	return "number";
}

std::optional<std::string> OvCore::Scripting::ListValue::NormalizeItem(EItemType p_itemType, std::string_view p_text)
{
	const std::string_view text = Trim(p_text);

	switch (p_itemType)
	{
	case EItemType::NUMBER:
	{
		double number = 0.0;
		const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), number);
		if (text.empty() || error != std::errc{} || end != text.data() + text.size())
			return std::nullopt;
		return std::format("{}", number);
	}
	case EItemType::BOOL:
		if (text == "true" || text == "1") return std::string("true");
		if (text == "false" || text == "0") return std::string("false");
		return std::nullopt;
	case EItemType::STRING:
		if (text.find(',') != std::string_view::npos)
			return std::nullopt;
		return std::string(text);
	}

	return std::nullopt;
}

std::string OvCore::Scripting::ListValue::Join() const
{
	std::string text;

	for (size_t index = 0; index < items.size(); ++index)
	{
		if (index > 0)
			text += ", ";
		text += items[index];
	}

	return text;
}

void OvCore::Scripting::ListValue::Split(std::string_view p_text)
{
	items.clear();

	if (Trim(p_text).empty())
		return;

	size_t start = 0;
	while (true)
	{
		const size_t end = p_text.find(',', start);
		const std::string_view part = p_text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);

		// An empty part (",," or a trailing comma) is no item, whatever the list's type
		if (auto item = NormalizeItem(itemType, part); item && !Trim(part).empty())
			items.push_back(std::move(*item));

		if (end == std::string_view::npos)
			return;

		start = end + 1;
	}
}
