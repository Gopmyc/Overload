/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace OvTools::Utils::Json
{
	class Value;
	struct Member;

	using Array = std::vector<Value>;
	using Object = std::vector<Member>;

	/**
	* A JSON value: null, a boolean, a number, a string, an array or an object, whose members keep their order
	*/
	class Value
	{
	public:
		Value();
		Value(std::nullptr_t);
		Value(bool p_value);
		Value(double p_value);
		Value(int p_value);
		Value(std::string p_value);
		Value(const char* p_value);
		Value(Array p_value);
		Value(Object p_value);

		bool IsNull() const;
		bool IsBool() const;
		bool IsNumber() const;
		bool IsString() const;
		bool IsArray() const;
		bool IsObject() const;

		/**
		* The value as its type, or nullptr when it is of another type
		*/
		const bool* AsBool() const;
		const double* AsNumber() const;
		const std::string* AsString() const;
		const Array* AsArray() const;
		const Object* AsObject() const;
		Array* AsArray();
		Object* AsObject();

		/**
		* Returns the member of an object of the given name (the first, if several have it), or nullptr when the value
		* is no object or has no such member
		* @param p_name
		*/
		const Value* Find(std::string_view p_name) const;

		bool operator==(const Value& p_other) const;

	private:
		std::variant<std::nullptr_t, bool, double, std::string, Array, Object> m_data;
	};

	struct Member
	{
		std::string name;
		Value value;

		bool operator==(const Member& p_other) const = default;
	};

	// Defined once Member is complete, which the arrays and objects of a value need
	inline Value::Value() = default;
	inline Value::Value(std::nullptr_t) {}
	inline Value::Value(bool p_value) : m_data(p_value) {}
	inline Value::Value(double p_value) : m_data(p_value) {}
	inline Value::Value(int p_value) : m_data(static_cast<double>(p_value)) {}
	inline Value::Value(std::string p_value) : m_data(std::move(p_value)) {}
	inline Value::Value(const char* p_value) : m_data(std::string(p_value)) {}
	inline Value::Value(Array p_value) : m_data(std::move(p_value)) {}
	inline Value::Value(Object p_value) : m_data(std::move(p_value)) {}

	inline bool Value::IsNull() const { return std::holds_alternative<std::nullptr_t>(m_data); }
	inline bool Value::IsBool() const { return std::holds_alternative<bool>(m_data); }
	inline bool Value::IsNumber() const { return std::holds_alternative<double>(m_data); }
	inline bool Value::IsString() const { return std::holds_alternative<std::string>(m_data); }
	inline bool Value::IsArray() const { return std::holds_alternative<Array>(m_data); }
	inline bool Value::IsObject() const { return std::holds_alternative<Object>(m_data); }

	inline const bool* Value::AsBool() const { return std::get_if<bool>(&m_data); }
	inline const double* Value::AsNumber() const { return std::get_if<double>(&m_data); }
	inline const std::string* Value::AsString() const { return std::get_if<std::string>(&m_data); }
	inline const Array* Value::AsArray() const { return std::get_if<Array>(&m_data); }
	inline const Object* Value::AsObject() const { return std::get_if<Object>(&m_data); }
	inline Array* Value::AsArray() { return std::get_if<Array>(&m_data); }
	inline Object* Value::AsObject() { return std::get_if<Object>(&m_data); }

	/**
	* Reads a JSON text (RFC 8259): one value, whitespace around it. On failure, returns nullopt and says where and why
	* in p_error ("line 3, column 7: ...")
	* @param p_text
	* @param p_error
	* @param p_maxDepth (arrays and objects nested deeper fail)
	*/
	std::optional<Value> Parse(std::string_view p_text, std::string& p_error, int p_maxDepth = 64);

	/**
	* Writes a value as JSON text, indented with tabs. A number that is not finite is written as null
	* @param p_value
	*/
	std::string Write(const Value& p_value);
}
