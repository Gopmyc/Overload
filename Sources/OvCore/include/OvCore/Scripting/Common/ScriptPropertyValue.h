/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <OvMaths/FVector2.h>
#include <OvMaths/FVector3.h>
#include <OvMaths/FVector4.h>
#include <OvTools/Utils/PathParser.h>
namespace OvCore::ECS { class Actor; } //forward declaration so ActorRef can return Actor&

namespace OvCore::Scripting
{
	/**
	* Represents a reference to an asset (file type + path).
	* Used as a script property so the inspector can display an asset picker.
	*/
	struct AssetRef
	{
		OvTools::Utils::PathParser::EFileType fileType = OvTools::Utils::PathParser::EFileType::UNKNOWN;
		std::string path;
	};

	/**
	* Represents a reference to a scene actor by its GUID (0 = none).
	* Serves two roles: as an editor script property so the inspector can
	* show an actor picker, and as the runtime Lua type for actors (methods
	* resolve the GUID through the current scene; a dead handle raises).
	*/
	struct ActorRef
	{
		uint64_t guid = 0;

		bool IsAlive() const;
		OvCore::ECS::Actor& Resolve() const;
	};

	/**
	* A colour property, declared with Color(r, g, b[, a]): the inspector shows a colour picker, and the script receives
	* a Vector4 (r, g, b, a, each from 0 to 1)
	*/
	struct ColorValue
	{
		OvMaths::FVector4 rgba = { 1.0f, 1.0f, 1.0f, 1.0f };
	};

	/**
	* An enumeration property, declared with Enum({ "A", "B" }, "A"): the inspector lists the choices, and the script
	* receives the chosen one
	*/
	struct EnumValue
	{
		std::vector<std::string> choices;
		std::string value;
	};

	/**
	* A list property, declared with List("number" | "string" | "bool", { ... }): the inspector edits its items as one
	* comma-separated line, and the script receives a plain table. The items are kept as text, already checked
	*/
	struct ListValue
	{
		enum class EItemType { NUMBER, STRING, BOOL };

		EItemType itemType = EItemType::NUMBER;
		std::vector<std::string> items;

		/**
		* The item type a name stands for ("number", "string" or "bool"), if any
		* @param p_name
		*/
		static std::optional<EItemType> ParseItemType(std::string_view p_name);

		/**
		* The name of an item type
		* @param p_itemType
		*/
		static std::string_view GetItemTypeName(EItemType p_itemType);

		/**
		* The item as the list keeps it ("1.5", "true"), or nothing when it is not one of the list's type. A string item
		* is trimmed and may not hold a comma
		* @param p_itemType
		* @param p_text
		*/
		static std::optional<std::string> NormalizeItem(EItemType p_itemType, std::string_view p_text);

		/**
		* The items joined by ", ", as the inspector shows them
		*/
		std::string Join() const;

		/**
		* Replaces the items by those of a comma-separated line, leaving out those that are not of the list's type
		* @param p_text
		*/
		void Split(std::string_view p_text);
	};

	/**
	* Represents a script property value: a primitive, an asset or actor reference, a vector, a colour, an enumeration
	* or a list. This type is backend-agnostic and used by Behaviour to store and expose script fields.
	*/
	using ScriptPropertyValue = std::variant<bool, double, std::string, AssetRef, ActorRef, OvMaths::FVector2,
		OvMaths::FVector3, OvMaths::FVector4, ColorValue, EnumValue, ListValue>;
}
