/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <format>
#include <limits>
#include <optional>
#include <utility>

#include <tinyxml2.h>

#include <OvCore/ECS/Actor.h>
#include <OvCore/ECS/Components/Behaviour.h>
#include <OvCore/Global/ServiceLocator.h>
#include <OvCore/Helpers/GUIDrawer.h>
#include <OvCore/Helpers/GUIHelpers.h>
#include <OvCore/Scripting/ScriptEngine.h>

#include <OvTools/Utils/PathParser.h>

#include <OvDebug/Logger.h>

#include <OvCore/SceneSystem/SceneManager.h>

#include <OvUI/Plugins/DataDispatcher.h>
#include <OvUI/Plugins/DDTarget.h>
#include <OvUI/Widgets/Drags/DragMultipleScalars.h>
#include <OvUI/Widgets/Drags/DragSingleScalar.h>
#include <OvUI/Widgets/InputFields/ActorField.h>
#include <OvUI/Widgets/InputFields/AssetField.h>
#include <OvUI/Widgets/InputFields/InputText.h>
#include <OvUI/Widgets/Layout/Dummy.h>
#include <OvUI/Widgets/Layout/Group.h>
#include <OvUI/Widgets/Layout/TreeNode.h>
#include <OvUI/Widgets/Selection/CheckBox.h>
#include <OvUI/Widgets/Selection/ColorEdit.h>
#include <OvUI/Widgets/Selection/ComboBox.h>
#include <OvUI/Widgets/Texts/TextColored.h>

#include <OvUI/Styling/Style.h>

OvCore::ECS::Components::Behaviour::Behaviour(ECS::Actor& p_owner, const std::string& p_name) :
	name(p_name), AComponent(p_owner)
{
	OVSERVICE(Scripting::ScriptEngine).AddBehaviour(*this);
}

OvCore::ECS::Components::Behaviour::~Behaviour()
{
	OVSERVICE(Scripting::ScriptEngine).RemoveBehaviour(*this);
}

std::string OvCore::ECS::Components::Behaviour::GetName()
{
	return "Behaviour";
}

namespace
{
	/**
	* N numbers separated by spaces, as a vector or a colour is written in a file
	*/
	template<size_t N>
	std::optional<std::array<float, N>> ParseFloats(const char* p_text)
	{
		std::array<float, N> values{};
		const std::string_view text = p_text ? p_text : "";
		size_t position = 0;

		for (size_t index = 0; index < N; ++index)
		{
			position = text.find_first_not_of(' ', position);
			if (position == std::string_view::npos)
				return std::nullopt;

			const auto [end, error] = std::from_chars(text.data() + position, text.data() + text.size(), values[index]);
			if (error != std::errc{})
				return std::nullopt;

			position = static_cast<size_t>(end - text.data());
		}

		return values;
	}

	/**
	* The value a script now declares, keeping what the user had set when it still fits: an enumeration takes the new
	* choices and keeps its value only if it is still one of them, a list keeps its items only for the same item type
	*/
	std::optional<OvCore::Scripting::ScriptPropertyValue> KeepUserValue(const OvCore::Scripting::ScriptPropertyValue& p_user, const OvCore::Scripting::ScriptPropertyValue& p_default)
	{
		using namespace OvCore::Scripting;

		if (p_user.index() != p_default.index())
			return std::nullopt;

		if (const auto* user = std::get_if<EnumValue>(&p_user))
		{
			const auto& choices = std::get<EnumValue>(p_default).choices;
			if (std::find(choices.begin(), choices.end(), user->value) == choices.end())
				return std::nullopt;
			return EnumValue{ choices, user->value };
		}

		if (const auto* user = std::get_if<ListValue>(&p_user); user && user->itemType != std::get<ListValue>(p_default).itemType)
			return std::nullopt;

		return p_user;
	}
}

std::string OvCore::ECS::Components::Behaviour::GetTypeName()
{
	return std::string{ComponentTraits<Behaviour>::Name};
}

void OvCore::ECS::Components::Behaviour::SetScript(std::unique_ptr<Scripting::Script> &&p_scriptContext)
{
	m_script = std::move(p_scriptContext);

	if (!m_script || !m_script->IsValid())
	{
		m_scriptDefaults.clear();
		return;
	}

	m_scriptDefaults = m_script->GetDefaultProperties();

	auto old = std::exchange(m_scriptProperties, {});
	auto oldUnlocked = std::exchange(m_unlockedProperties, {});

	for (const auto& [key, defaultVal] : m_scriptDefaults)
	{
		const bool wasUnlocked = oldUnlocked.contains(key);
		auto it = old.find(key);
		const auto kept = wasUnlocked && it != old.end() ? KeepUserValue(it->second, defaultVal) : std::nullopt;

		m_scriptProperties[key] = kept ? *kept : defaultVal;

		if (wasUnlocked)
			m_unlockedProperties.insert(key);
	}

	// Push user overrides back into the live script.
	for (const auto& [key, val] : m_scriptProperties)
		m_script->SetProperty(key, val);
}

OvTools::Utils::OptRef<OvCore::Scripting::Script> OvCore::ECS::Components::Behaviour::GetScript()
{
	if (m_script)
	{
		return { *m_script };
	}

	return std::nullopt;
}

void OvCore::ECS::Components::Behaviour::RemoveScript()
{
	m_script.reset();
}

void OvCore::ECS::Components::Behaviour::OnAwake()
{
	OVSERVICE(Scripting::ScriptEngine).OnAwake(*this);
}

void OvCore::ECS::Components::Behaviour::OnStart()
{
	OVSERVICE(Scripting::ScriptEngine).OnStart(*this);
}

void OvCore::ECS::Components::Behaviour::OnEnable()
{
	OVSERVICE(Scripting::ScriptEngine).OnEnable(*this);
}

void OvCore::ECS::Components::Behaviour::OnDisable()
{
	OVSERVICE(Scripting::ScriptEngine).OnDisable(*this);
}

void OvCore::ECS::Components::Behaviour::OnDestroy()
{
	OVSERVICE(Scripting::ScriptEngine).OnDestroy(*this);
}

void OvCore::ECS::Components::Behaviour::SetUpdating(bool p_updating)
{
	m_updating = p_updating;
}

bool OvCore::ECS::Components::Behaviour::IsUpdating() const
{
	return m_updating;
}

void OvCore::ECS::Components::Behaviour::OnUpdate(float p_deltaTime)
{
	OVSERVICE(Scripting::ScriptEngine).OnUpdate(*this, p_deltaTime);
}

void OvCore::ECS::Components::Behaviour::OnFixedUpdate(float p_deltaTime)
{
	OVSERVICE(Scripting::ScriptEngine).OnFixedUpdate(*this, p_deltaTime);
}

void OvCore::ECS::Components::Behaviour::OnLateUpdate(float p_deltaTime)
{
	OVSERVICE(Scripting::ScriptEngine).OnLateUpdate(*this, p_deltaTime);
}

void OvCore::ECS::Components::Behaviour::OnCollisionEnter(Components::CPhysicalObject& p_otherObject)
{
	OVSERVICE(Scripting::ScriptEngine).OnCollisionEnter(*this, p_otherObject);
}

void OvCore::ECS::Components::Behaviour::OnCollisionStay(Components::CPhysicalObject& p_otherObject)
{
	OVSERVICE(Scripting::ScriptEngine).OnCollisionStay(*this, p_otherObject);
}

void OvCore::ECS::Components::Behaviour::OnCollisionExit(Components::CPhysicalObject& p_otherObject)
{
	OVSERVICE(Scripting::ScriptEngine).OnCollisionExit(*this, p_otherObject);
}

void OvCore::ECS::Components::Behaviour::OnTriggerEnter(Components::CPhysicalObject& p_otherObject)
{
	OVSERVICE(Scripting::ScriptEngine).OnTriggerEnter(*this, p_otherObject);
}

void OvCore::ECS::Components::Behaviour::OnTriggerStay(Components::CPhysicalObject& p_otherObject)
{
	OVSERVICE(Scripting::ScriptEngine).OnTriggerStay(*this, p_otherObject);
}

void OvCore::ECS::Components::Behaviour::OnTriggerExit(Components::CPhysicalObject& p_otherObject)
{
	OVSERVICE(Scripting::ScriptEngine).OnTriggerExit(*this, p_otherObject);
}

void OvCore::ECS::Components::Behaviour::OnSerialize(tinyxml2::XMLDocument & p_doc, tinyxml2::XMLNode * p_node)
{
	if (m_unlockedProperties.empty()) return;
	if (!m_script->IsValid()) return;

	tinyxml2::XMLNode* propsNode = p_doc.NewElement("script_properties");
	p_node->InsertEndChild(propsNode);

	for (const auto& [fieldKey, fieldValue] : m_scriptProperties)
	{
		if (!m_unlockedProperties.contains(fieldKey)) continue;

		tinyxml2::XMLElement* elem = p_doc.NewElement(fieldKey.c_str());

		std::visit([elem]<typename T>(const T& v) {
			if constexpr (std::is_same_v<T, bool>)
				{ elem->SetAttribute("type", "bool");   elem->SetText(v); }
			else if constexpr (std::is_same_v<T, double>)
				{ elem->SetAttribute("type", "number"); elem->SetText(v); }
			else if constexpr (std::is_same_v<T, OvCore::Scripting::AssetRef>)
				{ elem->SetAttribute("type", "asset"); elem->SetAttribute("assetType", OvTools::Utils::PathParser::FileTypeToString(v.fileType).c_str()); elem->SetText(v.path.c_str()); }
			else if constexpr (std::is_same_v<T, OvCore::Scripting::ActorRef>)
				{ elem->SetAttribute("type", "actor"); elem->SetText(std::to_string(v.guid).c_str()); }
			else if constexpr (std::is_same_v<T, OvMaths::FVector2>)
				{ elem->SetAttribute("type", "vector2"); elem->SetText(std::format("{} {}", v.x, v.y).c_str()); }
			else if constexpr (std::is_same_v<T, OvMaths::FVector3>)
				{ elem->SetAttribute("type", "vector3"); elem->SetText(std::format("{} {} {}", v.x, v.y, v.z).c_str()); }
			else if constexpr (std::is_same_v<T, OvMaths::FVector4>)
				{ elem->SetAttribute("type", "vector4"); elem->SetText(std::format("{} {} {} {}", v.x, v.y, v.z, v.w).c_str()); }
			else if constexpr (std::is_same_v<T, OvCore::Scripting::ColorValue>)
				{ elem->SetAttribute("type", "color"); elem->SetText(std::format("{} {} {} {}", v.rgba.x, v.rgba.y, v.rgba.z, v.rgba.w).c_str()); }
			else if constexpr (std::is_same_v<T, OvCore::Scripting::EnumValue>)
				{ elem->SetAttribute("type", "enum"); elem->SetText(v.value.c_str()); }
			else if constexpr (std::is_same_v<T, OvCore::Scripting::ListValue>)
			{
				elem->SetAttribute("type", "list");
				elem->SetAttribute("itemType", std::string(OvCore::Scripting::ListValue::GetItemTypeName(v.itemType)).c_str());
				for (const auto& item : v.items)
				{
					auto* itemElem = elem->GetDocument()->NewElement("item");
					itemElem->SetText(item.c_str());
					elem->InsertEndChild(itemElem);
				}
			}
			else
				{ elem->SetAttribute("type", "string"); elem->SetText(v.c_str()); }
		}, fieldValue);

		propsNode->InsertEndChild(elem);
	}
}

void OvCore::ECS::Components::Behaviour::OnDeserialize(tinyxml2::XMLDocument & p_doc, tinyxml2::XMLNode * p_node)
{
	const tinyxml2::XMLElement* propsNode = p_node->FirstChildElement("script_properties");
	if (!propsNode) return;

	for (const tinyxml2::XMLElement* elem = propsNode->FirstChildElement(); elem; elem = elem->NextSiblingElement())
	{
		const char* const name = elem->Name();
		const char* const type = elem->Attribute("type");
		if (!name || !type) continue;

		auto it = m_scriptProperties.find(name);
		if (it == m_scriptProperties.end()) continue;

		const std::string_view typeStr{type};
		auto& val = it->second;

		static constexpr std::array<std::string_view, 11> kTypes = {
			"bool", "number", "string", "asset", "actor", "vector2", "vector3", "vector4", "color", "enum", "list"
		};

		if (std::find(kTypes.begin(), kTypes.end(), typeStr) == kTypes.end())
		{
			OVLOG_ERROR(std::format("{}: the property \"{}\" has the type \"{}\", which is none of bool, number, string, asset, actor, vector2, vector3, vector4, color, enum or list: the script's default is kept", this->name, name, typeStr));
			continue;
		}

		// The value read, or nothing when the file and the script disagree on the property
		const auto read = [&]() -> std::optional<OvCore::Scripting::ScriptPropertyValue>
		{
			using namespace OvCore::Scripting;

			if (typeStr == "bool" && std::holds_alternative<bool>(val))
			{
				bool v = false;
				elem->QueryBoolText(&v);
				return v;
			}

			if (typeStr == "number" && std::holds_alternative<double>(val))
			{
				double v = 0.0;
				elem->QueryDoubleText(&v);
				return v;
			}

			if (typeStr == "string" && std::holds_alternative<std::string>(val))
				return std::string(elem->GetText() ? elem->GetText() : "");

			if (typeStr == "asset" && std::holds_alternative<AssetRef>(val))
			{
				const char* assetTypeStr = elem->Attribute("assetType");
				return AssetRef{
					assetTypeStr ? OvTools::Utils::PathParser::StringToFileType(assetTypeStr) : std::get<AssetRef>(val).fileType,
					elem->GetText() ? elem->GetText() : ""
				};
			}

			if (typeStr == "actor" && std::holds_alternative<ActorRef>(val))
			{
				uint64_t guid = 0;
				if (const char* text = elem->GetText())
					std::from_chars(text, text + std::strlen(text), guid);
				return ActorRef{ guid };
			}

			if (typeStr == "vector2" && std::holds_alternative<OvMaths::FVector2>(val))
				if (const auto v = ParseFloats<2>(elem->GetText()))
					return OvMaths::FVector2{ (*v)[0], (*v)[1] };

			if (typeStr == "vector3" && std::holds_alternative<OvMaths::FVector3>(val))
				if (const auto v = ParseFloats<3>(elem->GetText()))
					return OvMaths::FVector3{ (*v)[0], (*v)[1], (*v)[2] };

			if (typeStr == "vector4" && std::holds_alternative<OvMaths::FVector4>(val))
				if (const auto v = ParseFloats<4>(elem->GetText()))
					return OvMaths::FVector4{ (*v)[0], (*v)[1], (*v)[2], (*v)[3] };

			if (typeStr == "color" && std::holds_alternative<ColorValue>(val))
				if (const auto v = ParseFloats<4>(elem->GetText()))
					return ColorValue{ { (*v)[0], (*v)[1], (*v)[2], (*v)[3] } };

			if (typeStr == "enum" && std::holds_alternative<EnumValue>(val))
			{
				const auto& choices = std::get<EnumValue>(val).choices;
				const std::string value = elem->GetText() ? elem->GetText() : "";
				if (std::find(choices.begin(), choices.end(), value) != choices.end())
					return EnumValue{ choices, value };
			}

			if (typeStr == "list" && std::holds_alternative<ListValue>(val))
			{
				const auto& declared = std::get<ListValue>(val);
				const char* itemType = elem->Attribute("itemType");

				if (itemType && ListValue::ParseItemType(itemType) == declared.itemType)
				{
					ListValue list{ declared.itemType, {} };
					for (auto* item = elem->FirstChildElement("item"); item; item = item->NextSiblingElement("item"))
						if (auto normalized = ListValue::NormalizeItem(list.itemType, item->GetText() ? item->GetText() : ""))
							list.items.push_back(std::move(*normalized));
					return list;
				}
			}

			return std::nullopt;
		};

		if (auto value = read())
		{
			val = std::move(*value);
		}
		else
		{
			OVLOG_WARNING(std::format("{}: the property \"{}\" written as {} does not fit what the script declares: the script's default is kept", this->name, name, typeStr));
			continue;
		}

		m_unlockedProperties.insert(name);

		if (m_script)
			m_script->SetProperty(name, val);
	}
}

void OvCore::ECS::Components::Behaviour::OnInspector(OvUI::Internal::WidgetContainer & p_root)
{
	using namespace OvCore::Helpers;

	if (!m_script)
	{
		p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>("No scripting context", OVUI_STYLE(TextDisabled));
		p_root.CreateWidget<OvUI::Widgets::Layout::Dummy>();
	}
	else if (m_script->IsValid())
	{
		p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>("Ready", OVUI_STYLE(Success));
		p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>("Script properties will appear below", OVUI_STYLE(TextDisabled));

		for (const auto& [fieldKey, fieldValue] : m_scriptProperties)
		{
			const bool isUnlocked = m_unlockedProperties.contains(fieldKey);

			std::visit([&, key = fieldKey, unlocked = isUnlocked]<typename T>(const T&) {
				auto getter = [this, key]() -> T {
					if (auto live = m_script->GetProperty(key))
						if (auto* val = std::get_if<T>(&*live))
							return *val;
					auto it = m_scriptProperties.find(key);
					return it != m_scriptProperties.end() ? std::get<T>(it->second) : T{};
				};
				auto setter = [this, key](T newVal) {
					m_scriptProperties[key] = newVal;
					if (m_script) m_script->SetProperty(key, std::move(newVal));
				};

				// Label row: [unlock checkbox] [field title], grouped together
				auto& labelGroup = p_root.CreateWidget<OvUI::Widgets::Layout::Group>();
				auto& unlockBox = labelGroup.CreateWidget<OvUI::Widgets::Selection::CheckBox>(unlocked);
				unlockBox.lineBreak = false;
				labelGroup.CreateWidget<OvUI::Widgets::Texts::TextColored>(key, OVUI_STYLE(InspectorTitle));

				// Input widget on the row below
				OvUI::Widgets::AWidget* inputPtr = nullptr;
				if constexpr (std::is_same_v<T, bool>)
				{
					auto& w = p_root.CreateWidget<OvUI::Widgets::Selection::CheckBox>();
					auto& d = w.template AddPlugin<OvUI::Plugins::DataDispatcher<bool>>();
					d.RegisterGatherer([getter]() { bool v = getter(); return reinterpret_cast<bool&>(v); });
					d.RegisterProvider([setter](bool v) { setter(reinterpret_cast<bool&>(v)); });
					inputPtr = &w;
				}
				else if constexpr (std::is_same_v<T, double>)
				{
					auto& w = p_root.CreateWidget<OvUI::Widgets::Drags::DragSingleScalar<double>>(
						GUIDrawer::GetDataType<double>(),
						std::numeric_limits<double>::lowest(), std::numeric_limits<double>::max(),
						static_cast<double>(0), 1.f, "", GUIDrawer::GetFormat<double>()
					);
					auto& d = w.template AddPlugin<OvUI::Plugins::DataDispatcher<double>>();
					d.RegisterGatherer(getter);
					d.RegisterProvider(setter);
					inputPtr = &w;
				}
				else if constexpr (std::is_same_v<T, OvCore::Scripting::AssetRef>)
				{
					const auto fileType = getter().fileType;

					auto pathGatherer = [getter]() { return getter().path; };
					auto pathProvider = [setter, getter](std::string newPath) {
						auto ref = getter();
						ref.path = std::move(newPath);
						setter(std::move(ref));
					};

					// Create the AssetField directly (without a GUIDrawer title, since the
					// label row above already serves that purpose).
					auto& w = p_root.CreateWidget<OvUI::Widgets::InputFields::AssetField>(pathGatherer());
					w.iconTextureID = OvCore::Helpers::GUIHelpers::GetIconForFileType(fileType);
					w.displayFormatter = &OvCore::Helpers::GUIDrawer::GetAssetDisplayName;
					w.tooltipFormatter = &OvCore::Helpers::GUIDrawer::GetAssetTooltip;

					auto widgetPtr = std::shared_ptr<OvUI::Widgets::InputFields::AssetField>(&w, [](void*) {});
					w.template AddPlugin<OvUI::Plugins::DataDispatcher<std::string>>().RegisterGatherer(pathGatherer);

					w.template AddPlugin<OvUI::Plugins::DDTarget<std::pair<std::string, OvUI::Widgets::Layout::Group*>>>("File").DataReceivedEvent +=
						[widgetPtr, pathProvider, fileType](auto p_receivedData)
					{
						const bool typeMatch = fileType == OvTools::Utils::PathParser::EFileType::UNKNOWN
							|| OvTools::Utils::PathParser::GetFileType(p_receivedData.first) == fileType;
						if (typeMatch)
						{
							widgetPtr->content = p_receivedData.first;
							pathProvider(p_receivedData.first);
						}
					};

					auto token = std::make_shared<bool>(true);
					w.ClickedEvent += [widgetPtr, pathProvider, fileType, token]()
					{
						std::weak_ptr<bool> weak = token;
						OvCore::Helpers::GUIHelpers::OpenAssetPicker(fileType,
							[widgetPtr, pathProvider, weak](const std::string& p_path)
							{
								if (!weak.expired())
								{
									widgetPtr->content = p_path;
									pathProvider(p_path);
								}
							}, true, true);
					};

					w.DoubleClickedEvent += [widgetPtr] { OvCore::Helpers::GUIHelpers::Open(widgetPtr->content); };

					inputPtr = &w;
				}
				else if constexpr (std::is_same_v<T, OvCore::Scripting::ActorRef>)
				{
					// Determine display name and tooltip from the scene.
					auto resolveDisplayName = [](uint64_t guid) -> std::string {
						if (guid == 0) return "";
						auto* scene = OVSERVICE(OvCore::SceneSystem::SceneManager).GetCurrentScene();
						auto* actor = scene ? scene->FindActorByGUID(guid) : nullptr;
						return actor ? actor->GetName() : "(Missing)";
					};

					const OvCore::Scripting::ActorRef initial = getter();
					auto& w = p_root.CreateWidget<OvUI::Widgets::InputFields::ActorField>(initial.guid);
					w.iconTextureID = OvCore::Helpers::GUIHelpers::GetActorIconID();
					w.displayFormatter = [resolveDisplayName](uint64_t guid) { return resolveDisplayName(guid); };
					w.tooltipFormatter = [](uint64_t guid) -> std::string {
						return guid != 0 ? std::format("GUID: {:016X}", guid) : "";
					};

					auto widgetPtr = std::shared_ptr<OvUI::Widgets::InputFields::ActorField>(&w, [](void*) {});

					w.template AddPlugin<OvUI::Plugins::DataDispatcher<uint64_t>>().RegisterGatherer(
						[getter]() -> uint64_t { return getter().guid; }
					);

					// Drag-drop: accept an actor dropped from the hierarchy.
					using ActorPayload = std::pair<OvCore::ECS::Actor*, OvUI::Widgets::Layout::TreeNode*>;
					w.template AddPlugin<OvUI::Plugins::DDTarget<ActorPayload>>("Actor").DataReceivedEvent +=
						[widgetPtr, setter](ActorPayload p_payload)
					{
						if (p_payload.first)
						{
							widgetPtr->content = p_payload.first->GetGUID();
							setter(OvCore::Scripting::ActorRef{widgetPtr->content});
						}
					};

					// Double-click: select the referenced actor in the hierarchy.
					w.DoubleClickedEvent += [widgetPtr]()
					{
						OvCore::Helpers::GUIHelpers::SelectActor(widgetPtr->content);
					};

					// Picker button: list all scene actors.
					auto token = std::make_shared<bool>(true);
					w.ClickedEvent += [widgetPtr, setter, token]()
					{
						auto* scene = OVSERVICE(OvCore::SceneSystem::SceneManager).GetCurrentScene();
						if (!scene) return;

						std::weak_ptr<bool> weak = token;
						OvCore::Helpers::GUIHelpers::PickerItemList items;
						items.Add({ "__none__", "None", "Clear the current selection", 0u,
							[widgetPtr, setter, weak]()
							{
								if (weak.expired()) return;
								widgetPtr->content = 0;
								setter(OvCore::Scripting::ActorRef{0});
							}
						});
						for (auto& actor : scene->GetActors())
						{
							const uint64_t guid = actor->GetGUID();
							items.Add({
								std::to_string(guid),
								actor->GetName(),
								"GUID: " + std::format("{:016X}", guid),
								OvCore::Helpers::GUIHelpers::GetActorIconID(),
								[widgetPtr, setter, guid, weak]()
								{
									if (!weak.expired())
									{
										widgetPtr->content = guid;
										setter(OvCore::Scripting::ActorRef{guid});
									}
								}
							});
						}

						OvCore::Helpers::GUIHelpers::OpenPicker(std::move(items), "Select Actor");
					};

					inputPtr = &w;
				}
				else if constexpr (std::is_same_v<T, OvMaths::FVector2> || std::is_same_v<T, OvMaths::FVector3> || std::is_same_v<T, OvMaths::FVector4>)
				{
					constexpr size_t kSize = sizeof(T) / sizeof(float);
					auto& w = p_root.CreateWidget<OvUI::Widgets::Drags::DragMultipleScalars<float, kSize>>(
						GUIDrawer::GetDataType<float>(), GUIDrawer::_MIN_FLOAT, GUIDrawer::_MAX_FLOAT, 0.0f, 0.1f, "", GUIDrawer::GetFormat<float>()
					);
					auto& d = w.template AddPlugin<OvUI::Plugins::DataDispatcher<std::array<float, kSize>>>();
					d.RegisterGatherer([getter]() { const T v = getter(); return reinterpret_cast<const std::array<float, kSize>&>(v); });
					d.RegisterProvider([setter](std::array<float, kSize> v) { setter(reinterpret_cast<const T&>(v)); });
					inputPtr = &w;
				}
				else if constexpr (std::is_same_v<T, OvCore::Scripting::ColorValue>)
				{
					auto& w = p_root.CreateWidget<OvUI::Widgets::Selection::ColorEdit>(true);
					auto& d = w.template AddPlugin<OvUI::Plugins::DataDispatcher<OvUI::Types::Color>>();
					d.RegisterGatherer([getter]() {
						const auto rgba = getter().rgba;
						return OvUI::Types::Color{ rgba.x, rgba.y, rgba.z, rgba.w };
					});
					d.RegisterProvider([setter](OvUI::Types::Color c) { setter(OvCore::Scripting::ColorValue{ { c.r, c.g, c.b, c.a } }); });
					inputPtr = &w;
				}
				else if constexpr (std::is_same_v<T, OvCore::Scripting::EnumValue>)
				{
					// The choices are the script's: an index stands for each
					const auto choices = getter().choices;
					const auto indexOf = [choices](const std::string& p_value) {
						const auto found = std::find(choices.begin(), choices.end(), p_value);
						return found != choices.end() ? static_cast<int>(found - choices.begin()) : 0;
					};

					auto& w = p_root.CreateWidget<OvUI::Widgets::Selection::ComboBox>(indexOf(getter().value));
					for (size_t index = 0; index < choices.size(); ++index)
						w.choices.emplace(static_cast<int>(index), choices[index]);

					auto& d = w.template AddPlugin<OvUI::Plugins::DataDispatcher<int>>();
					d.RegisterGatherer([getter, indexOf]() { return indexOf(getter().value); });
					d.RegisterProvider([setter, choices](int p_index) {
						if (p_index >= 0 && static_cast<size_t>(p_index) < choices.size())
							setter(OvCore::Scripting::EnumValue{ choices, choices[p_index] });
					});
					inputPtr = &w;
				}
				else if constexpr (std::is_same_v<T, OvCore::Scripting::ListValue>)
				{
					// Edited as one line and read when Enter is pressed, so that a half-typed item is not dropped
					auto& w = p_root.CreateWidget<OvUI::Widgets::InputFields::InputText>(getter().Join());
					auto* widget = &w;
					w.EnterPressedEvent += [getter, setter, widget](std::string p_text) {
						auto list = getter();
						list.Split(p_text);
						widget->content = list.Join();
						setter(std::move(list));
					};
					inputPtr = &w;
				}
				else
				{
					auto& w = p_root.CreateWidget<OvUI::Widgets::InputFields::InputText>("");
					auto& d = w.template AddPlugin<OvUI::Plugins::DataDispatcher<std::string>>();
					d.RegisterGatherer(getter);
					d.RegisterProvider(setter);
					inputPtr = &w;
				}

				auto& inputWidget = *inputPtr;
				inputWidget.disabled = !unlocked;

				unlockBox.ValueChangedEvent += [this, key, &inputWidget](bool checked) {
					if (checked)
					{
						m_unlockedProperties.insert(key);
					}
					else
					{
						m_unlockedProperties.erase(key);
						if (auto it = m_scriptDefaults.find(key); it != m_scriptDefaults.end())
						{
							m_scriptProperties[key] = it->second;
							if (m_script) m_script->SetProperty(key, it->second);
						}
					}
					inputWidget.disabled = !checked;
				};
			}, fieldValue);
		}
	}
	else
	{
		p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>("Invalid Script", OVUI_STYLE(Danger));
		p_root.CreateWidget<OvUI::Widgets::Texts::TextColored>("Check the console for more information.", OVUI_STYLE(TextDisabled));
	}
}
