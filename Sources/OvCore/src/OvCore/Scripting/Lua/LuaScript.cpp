/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <charconv>
#include <optional>
#include <string>

#include <sol/sol.hpp>

#include <OvDebug/Logger.h>
#include <OvDebug/Assertion.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/ResourceManagement/MaterialManager.h>
#include <OvCore/ResourceManagement/ModelManager.h>
#include <OvCore/ResourceManagement/ShaderManager.h>
#include <OvCore/ResourceManagement/SoundManager.h>
#include <OvCore/ResourceManagement/TextureManager.h>
#include <OvCore/SceneSystem/SceneManager.h>
#include <OvCore/Scripting/Lua/LuaScript.h>

namespace
{
	/**
	* The property a userdata stands for: an asset or actor reference, a vector, or a declared colour, enumeration or
	* list
	*/
	std::optional<OvCore::Scripting::ScriptPropertyValue> ReadUserdataProperty(const sol::object& p_value)
	{
		using namespace OvCore::Scripting;

		if (p_value.is<AssetRef>()) return p_value.as<AssetRef>();
		if (p_value.is<ActorRef>()) return p_value.as<ActorRef>();
		if (p_value.is<OvMaths::FVector2>()) return p_value.as<OvMaths::FVector2>();
		if (p_value.is<OvMaths::FVector3>()) return p_value.as<OvMaths::FVector3>();
		if (p_value.is<OvMaths::FVector4>()) return p_value.as<OvMaths::FVector4>();
		if (p_value.is<ColorValue>()) return p_value.as<ColorValue>();
		if (p_value.is<EnumValue>()) return p_value.as<EnumValue>();
		if (p_value.is<ListValue>()) return p_value.as<ListValue>();
		return std::nullopt;
	}
}

template<>
OvCore::Scripting::LuaScriptBase::TScript() = default;

template<>
OvCore::Scripting::LuaScriptBase::~TScript() = default;

template<>
bool OvCore::Scripting::LuaScriptBase::IsValid() const
{
	return m_context.table && m_context.table->valid();
}

OvCore::Scripting::LuaScript::LuaScript(sol::table table)
{
	m_context.table = std::make_unique<sol::table>(table);
}

void OvCore::Scripting::LuaScript::SetOwner(OvCore::ECS::Actor& p_owner)
{
	(*m_context.table)["owner"] = ActorRef{ p_owner.GetGUID() };
}

template<>
std::map<std::string, OvCore::Scripting::ScriptPropertyValue> OvCore::Scripting::LuaScriptBase::GetDefaultProperties() const
{
	std::map<std::string, ScriptPropertyValue> properties;

	if (!IsValid()) return properties;

	m_context.table->for_each([&](const sol::object& key, const sol::object& value) {
		if (!key.is<std::string>()) return;
		const std::string keyStr = key.as<std::string>();
		if (keyStr.starts_with('_')) return;

		switch (value.get_type())
		{
			case sol::type::boolean:  properties[keyStr] = value.as<bool>();        break;
			case sol::type::number:   properties[keyStr] = value.as<double>();      break;
			case sol::type::string:   properties[keyStr] = value.as<std::string>(); break;
			case sol::type::userdata:
				if (auto property = ReadUserdataProperty(value))
					properties[keyStr] = std::move(*property);
				break;
			default: break;
		}
	});

	return properties;
}

template<>
std::optional<OvCore::Scripting::ScriptPropertyValue> OvCore::Scripting::LuaScriptBase::GetProperty(const std::string& p_key) const
{
	if (!IsValid()) return std::nullopt;

	const sol::object obj = (*m_context.table)[p_key];

	switch (obj.get_type())
	{
		case sol::type::boolean:  return obj.as<bool>();
		case sol::type::number:   return obj.as<double>();
		case sol::type::string:   return obj.as<std::string>();
		case sol::type::userdata:
			return ReadUserdataProperty(obj);
		default:                  return std::nullopt;
	}
}

template<>
void OvCore::Scripting::LuaScriptBase::SetProperty(const std::string& p_key, const OvCore::Scripting::ScriptPropertyValue& p_value)
{
	if (!IsValid()) return;

	std::visit([&](auto&& v) {
		using T = std::decay_t<decltype(v)>;
		if constexpr (std::is_same_v<T, AssetRef>)
		{
			// Resolve loadable assets to resources. Prefabs stay as AssetRef for Scene::InstantiatePrefab.
			using EFT = OvTools::Utils::PathParser::EFileType;
			if (v.path.empty())
			{
				(*m_context.table)[p_key] = sol::nil;
			}
			else
			{
				switch (v.fileType)
				{
				case EFT::MODEL:    (*m_context.table)[p_key] = OvCore::Global::ServiceLocator::Get<OvCore::ResourceManagement::ModelManager>()[v.path];    break;
				case EFT::TEXTURE:  (*m_context.table)[p_key] = OvCore::Global::ServiceLocator::Get<OvCore::ResourceManagement::TextureManager>()[v.path];  break;
				case EFT::SHADER:   (*m_context.table)[p_key] = OvCore::Global::ServiceLocator::Get<OvCore::ResourceManagement::ShaderManager>()[v.path];   break;
				case EFT::MATERIAL: (*m_context.table)[p_key] = OvCore::Global::ServiceLocator::Get<OvCore::ResourceManagement::MaterialManager>()[v.path]; break;
				case EFT::SOUND:    (*m_context.table)[p_key] = OvCore::Global::ServiceLocator::Get<OvCore::ResourceManagement::SoundManager>()[v.path];    break;
				case EFT::PREFAB:   (*m_context.table)[p_key] = v; break;
				default:            (*m_context.table)[p_key] = sol::nil; break;
				}
			}
		}
		else if constexpr (std::is_same_v<T, ActorRef>)
		{
			// Only actors of the current scene are exposed to Lua
			if (v.guid == 0)
			{
				(*m_context.table)[p_key] = sol::nil;
			}
			else
			{
				auto* scene = OvCore::Global::ServiceLocator::Get<OvCore::SceneSystem::SceneManager>().GetCurrentScene();
				auto* actor = scene ? scene->FindActorByGUID(v.guid) : nullptr;
				if (actor)
					(*m_context.table)[p_key] = ActorRef{ actor->GetGUID() };
				else
					(*m_context.table)[p_key] = sol::nil;
			}
		}
		else if constexpr (std::is_same_v<T, ColorValue>)
		{
			(*m_context.table)[p_key] = v.rgba;
		}
		else if constexpr (std::is_same_v<T, EnumValue>)
		{
			(*m_context.table)[p_key] = v.value;
		}
		else if constexpr (std::is_same_v<T, ListValue>)
		{
			// A fresh table for each Behaviour, of numbers, strings or booleans as the list declares
			sol::state_view lua(m_context.table->lua_state());
			sol::table items = lua.create_table(static_cast<int>(v.items.size()), 0);

			for (size_t index = 0; index < v.items.size(); ++index)
			{
				const std::string& item = v.items[index];

				switch (v.itemType)
				{
				case ListValue::EItemType::NUMBER:
				{
					double number = 0.0;
					std::from_chars(item.data(), item.data() + item.size(), number);
					items[index + 1] = number;
					break;
				}
				case ListValue::EItemType::BOOL: items[index + 1] = item == "true"; break;
				case ListValue::EItemType::STRING: items[index + 1] = item; break;
				}
			}

			(*m_context.table)[p_key] = items;
		}
		else
		{
			(*m_context.table)[p_key] = v;
		}
	}, p_value);
}
