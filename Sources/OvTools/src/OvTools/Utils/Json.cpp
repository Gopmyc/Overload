/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <charconv>
#include <cmath>
#include <cstdint>
#include <format>

#include "OvTools/Utils/Json.h"

namespace
{
	using namespace OvTools::Utils::Json;

	void AppendUtf8(std::string& p_out, uint32_t p_codePoint)
	{
		if (p_codePoint < 0x80)
		{
			p_out += static_cast<char>(p_codePoint);
		}
		else if (p_codePoint < 0x800)
		{
			p_out += static_cast<char>(0xC0 | (p_codePoint >> 6));
			p_out += static_cast<char>(0x80 | (p_codePoint & 0x3F));
		}
		else if (p_codePoint < 0x10000)
		{
			p_out += static_cast<char>(0xE0 | (p_codePoint >> 12));
			p_out += static_cast<char>(0x80 | ((p_codePoint >> 6) & 0x3F));
			p_out += static_cast<char>(0x80 | (p_codePoint & 0x3F));
		}
		else
		{
			p_out += static_cast<char>(0xF0 | (p_codePoint >> 18));
			p_out += static_cast<char>(0x80 | ((p_codePoint >> 12) & 0x3F));
			p_out += static_cast<char>(0x80 | ((p_codePoint >> 6) & 0x3F));
			p_out += static_cast<char>(0x80 | (p_codePoint & 0x3F));
		}
	}

	/**
	* Whether the text is well-formed UTF-8: no overlong form, no surrogate, nothing beyond U+10FFFF
	*/
	bool IsValidUtf8(std::string_view p_text)
	{
		size_t i = 0;

		while (i < p_text.size())
		{
			const auto byte = static_cast<unsigned char>(p_text[i]);
			size_t length = 0;
			uint32_t codePoint = 0;

			if (byte < 0x80) { ++i; continue; }
			else if ((byte & 0xE0) == 0xC0) { length = 2; codePoint = byte & 0x1F; }
			else if ((byte & 0xF0) == 0xE0) { length = 3; codePoint = byte & 0x0F; }
			else if ((byte & 0xF8) == 0xF0) { length = 4; codePoint = byte & 0x07; }
			else return false;

			if (i + length > p_text.size())
			{
				return false;
			}

			for (size_t j = 1; j < length; ++j)
			{
				const auto next = static_cast<unsigned char>(p_text[i + j]);
				if ((next & 0xC0) != 0x80) return false;
				codePoint = (codePoint << 6) | (next & 0x3F);
			}

			const uint32_t smallest[] = { 0, 0, 0x80, 0x800, 0x10000 };
			if (codePoint < smallest[length] || codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF))
			{
				return false;
			}

			i += length;
		}

		return true;
	}

	class Reader
	{
	public:
		Reader(std::string_view p_text, int p_maxDepth) : m_text(p_text), m_maxDepth(p_maxDepth) {}

		std::optional<Value> Read(std::string& p_error)
		{
			Value value;
			SkipSpace();

			if (!ReadValue(value, 0))
			{
				p_error = m_error;
				return std::nullopt;
			}

			SkipSpace();

			if (!AtEnd())
			{
				Fail("unexpected character after the value");
				p_error = m_error;
				return std::nullopt;
			}

			return value;
		}

	private:
		bool Fail(std::string_view p_message)
		{
			size_t line = 1;
			size_t column = 1;

			for (size_t i = 0; i < std::min(m_position, m_text.size()); ++i)
			{
				if (m_text[i] == '\n') { ++line; column = 1; }
				else { ++column; }
			}

			m_error = std::format("line {}, column {}: {}", line, column, p_message);
			return false;
		}

		bool AtEnd() const { return m_position >= m_text.size(); }
		char Peek() const { return m_text[m_position]; }

		void SkipSpace()
		{
			while (!AtEnd() && (Peek() == ' ' || Peek() == '\t' || Peek() == '\n' || Peek() == '\r'))
			{
				++m_position;
			}
		}

		bool Literal(std::string_view p_word)
		{
			if (m_text.substr(m_position, p_word.size()) != p_word)
			{
				return Fail("unexpected character");
			}

			m_position += p_word.size();
			return true;
		}

		bool ReadValue(Value& p_out, int p_depth)
		{
			if (AtEnd())
			{
				return Fail("unexpected end");
			}

			switch (Peek())
			{
			case '{': return ReadObject(p_out, p_depth);
			case '[': return ReadArray(p_out, p_depth);
			case '"':
			{
				std::string text;
				if (!ReadString(text)) return false;
				p_out = Value(std::move(text));
				return true;
			}
			case 't': if (!Literal("true")) return false; p_out = Value(true); return true;
			case 'f': if (!Literal("false")) return false; p_out = Value(false); return true;
			case 'n': if (!Literal("null")) return false; p_out = Value(nullptr); return true;
			default:
				if (Peek() == '-' || (Peek() >= '0' && Peek() <= '9'))
				{
					return ReadNumber(p_out);
				}

				return Fail("unexpected character");
			}
		}

		bool Enter(int p_depth)
		{
			if (p_depth >= m_maxDepth)
			{
				return Fail("nested too deep");
			}

			++m_position;
			SkipSpace();
			return true;
		}

		bool ReadArray(Value& p_out, int p_depth)
		{
			if (!Enter(p_depth))
			{
				return false;
			}

			Array array;

			if (!AtEnd() && Peek() == ']')
			{
				++m_position;
				p_out = Value(std::move(array));
				return true;
			}

			while (true)
			{
				Value element;
				if (!ReadValue(element, p_depth + 1)) return false;
				array.push_back(std::move(element));

				SkipSpace();
				if (AtEnd()) return Fail("unexpected end");

				if (Peek() == ',')
				{
					++m_position;
					SkipSpace();
				}
				else if (Peek() == ']')
				{
					++m_position;
					break;
				}
				else
				{
					return Fail("expected ',' or ']'");
				}
			}

			p_out = Value(std::move(array));
			return true;
		}

		bool ReadObject(Value& p_out, int p_depth)
		{
			if (!Enter(p_depth))
			{
				return false;
			}

			Object object;

			if (!AtEnd() && Peek() == '}')
			{
				++m_position;
				p_out = Value(std::move(object));
				return true;
			}

			while (true)
			{
				if (AtEnd() || Peek() != '"') return Fail("expected a member name");

				Member member;
				if (!ReadString(member.name)) return false;

				SkipSpace();
				if (AtEnd() || Peek() != ':') return Fail("expected ':'");
				++m_position;
				SkipSpace();

				if (!ReadValue(member.value, p_depth + 1)) return false;
				object.push_back(std::move(member));

				SkipSpace();
				if (AtEnd()) return Fail("unexpected end");

				if (Peek() == ',')
				{
					++m_position;
					SkipSpace();
				}
				else if (Peek() == '}')
				{
					++m_position;
					break;
				}
				else
				{
					return Fail("expected ',' or '}'");
				}
			}

			p_out = Value(std::move(object));
			return true;
		}

		bool Hex(uint32_t& p_value)
		{
			if (m_position + 4 > m_text.size())
			{
				return Fail("unexpected end");
			}

			p_value = 0;

			for (int i = 0; i < 4; ++i)
			{
				const char digit = m_text[m_position++];
				p_value <<= 4;

				if (digit >= '0' && digit <= '9') p_value |= static_cast<uint32_t>(digit - '0');
				else if (digit >= 'a' && digit <= 'f') p_value |= static_cast<uint32_t>(digit - 'a' + 10);
				else if (digit >= 'A' && digit <= 'F') p_value |= static_cast<uint32_t>(digit - 'A' + 10);
				else return Fail("expected a hexadecimal digit");
			}

			return true;
		}

		bool ReadString(std::string& p_out)
		{
			const size_t start = m_position;
			++m_position;

			while (true)
			{
				if (AtEnd())
				{
					return Fail("unexpected end");
				}

				const char character = m_text[m_position++];

				if (character == '"')
				{
					break;
				}

				if (static_cast<unsigned char>(character) < 0x20)
				{
					--m_position;
					return Fail("a control character in a string");
				}

				if (character != '\\')
				{
					p_out += character;
					continue;
				}

				if (AtEnd())
				{
					return Fail("unexpected end");
				}

				switch (m_text[m_position++])
				{
				case '"': p_out += '"'; break;
				case '\\': p_out += '\\'; break;
				case '/': p_out += '/'; break;
				case 'b': p_out += '\b'; break;
				case 'f': p_out += '\f'; break;
				case 'n': p_out += '\n'; break;
				case 'r': p_out += '\r'; break;
				case 't': p_out += '\t'; break;
				case 'u':
				{
					uint32_t codePoint = 0;

					if (!Hex(codePoint))
					{
						return false;
					}

					// A character beyond the first plane comes as two escapes, high then low surrogate
					if (codePoint >= 0xD800 && codePoint <= 0xDBFF)
					{
						uint32_t low = 0;

						if (m_text.substr(m_position, 2) != "\\u" || (m_position += 2, !Hex(low)) || low < 0xDC00 || low > 0xDFFF)
						{
							return Fail("a lone surrogate");
						}

						codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
					}
					else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF)
					{
						return Fail("a lone surrogate");
					}

					AppendUtf8(p_out, codePoint);
					break;
				}
				default:
					--m_position;
					return Fail("an unknown escape");
				}
			}

			if (!IsValidUtf8(p_out))
			{
				m_position = start;
				return Fail("a string is not valid UTF-8");
			}

			return true;
		}

		bool Digits()
		{
			const size_t start = m_position;

			while (!AtEnd() && Peek() >= '0' && Peek() <= '9')
			{
				++m_position;
			}

			return m_position > start;
		}

		bool ReadNumber(Value& p_out)
		{
			const size_t start = m_position;

			if (Peek() == '-')
			{
				++m_position;
			}

			// No leading zero but a lone one, no "+", no bare "." nor "1."
			if (!AtEnd() && Peek() == '0')
			{
				++m_position;
			}
			else if (!Digits())
			{
				return Fail("expected a digit");
			}

			if (!AtEnd() && Peek() == '.')
			{
				++m_position;

				if (!Digits())
				{
					return Fail("expected a digit");
				}
			}

			if (!AtEnd() && (Peek() == 'e' || Peek() == 'E'))
			{
				++m_position;

				if (!AtEnd() && (Peek() == '+' || Peek() == '-'))
				{
					++m_position;
				}

				if (!Digits())
				{
					return Fail("expected a digit");
				}
			}

			// from_chars reads it whatever the locale ('.' only, no thousands)
			double value = 0.0;
			const auto result = std::from_chars(m_text.data() + start, m_text.data() + m_position, value);

			if (result.ec != std::errc{} || !std::isfinite(value))
			{
				m_position = start;
				return Fail("a number out of range");
			}

			p_out = Value(value);
			return true;
		}

	private:
		std::string_view m_text;
		int m_maxDepth;
		size_t m_position = 0;
		std::string m_error;
	};

	void WriteString(std::string& p_out, std::string_view p_text)
	{
		p_out += '"';

		for (const char character : p_text)
		{
			switch (character)
			{
			case '"': p_out += "\\\""; break;
			case '\\': p_out += "\\\\"; break;
			case '\b': p_out += "\\b"; break;
			case '\f': p_out += "\\f"; break;
			case '\n': p_out += "\\n"; break;
			case '\r': p_out += "\\r"; break;
			case '\t': p_out += "\\t"; break;
			default:
				if (static_cast<unsigned char>(character) < 0x20)
				{
					p_out += std::format("\\u{:04x}", static_cast<unsigned int>(static_cast<unsigned char>(character)));
				}
				else
				{
					p_out += character;
				}
			}
		}

		p_out += '"';
	}

	void WriteNumber(std::string& p_out, double p_value)
	{
		if (!std::isfinite(p_value))
		{
			p_out += "null";
			return;
		}

		// Integers as integers, the others with as many digits as it takes to read them back exactly
		if (p_value == std::floor(p_value) && std::abs(p_value) < 1e15)
		{
			p_out += std::format("{}", static_cast<long long>(p_value));
			return;
		}

		char buffer[32];
		const auto result = std::to_chars(buffer, buffer + sizeof(buffer), p_value);
		p_out.append(buffer, result.ptr);
	}

	void WriteValue(std::string& p_out, const Value& p_value, int p_depth)
	{
		const auto newline = [&p_out](int p_indent)
		{
			p_out += '\n';
			p_out.append(static_cast<size_t>(p_indent), '\t');
		};

		if (p_value.IsNull()) p_out += "null";
		else if (const bool* boolean = p_value.AsBool()) p_out += *boolean ? "true" : "false";
		else if (const double* number = p_value.AsNumber()) WriteNumber(p_out, *number);
		else if (const std::string* text = p_value.AsString()) WriteString(p_out, *text);
		else if (const Array* array = p_value.AsArray())
		{
			if (array->empty())
			{
				p_out += "[]";
				return;
			}

			p_out += '[';
			for (size_t i = 0; i < array->size(); ++i)
			{
				if (i > 0) p_out += ',';
				newline(p_depth + 1);
				WriteValue(p_out, (*array)[i], p_depth + 1);
			}
			newline(p_depth);
			p_out += ']';
		}
		else if (const Object* object = p_value.AsObject())
		{
			if (object->empty())
			{
				p_out += "{}";
				return;
			}

			p_out += '{';
			for (size_t i = 0; i < object->size(); ++i)
			{
				if (i > 0) p_out += ',';
				newline(p_depth + 1);
				WriteString(p_out, (*object)[i].name);
				p_out += ": ";
				WriteValue(p_out, (*object)[i].value, p_depth + 1);
			}
			newline(p_depth);
			p_out += '}';
		}
	}
}

const OvTools::Utils::Json::Value* OvTools::Utils::Json::Value::Find(std::string_view p_name) const
{
	if (const Object* object = AsObject())
	{
		for (const Member& member : *object)
		{
			if (member.name == p_name)
			{
				return &member.value;
			}
		}
	}

	return nullptr;
}

bool OvTools::Utils::Json::Value::operator==(const Value& p_other) const
{
	return m_data == p_other.m_data;
}

std::optional<OvTools::Utils::Json::Value> OvTools::Utils::Json::Parse(std::string_view p_text, std::string& p_error, int p_maxDepth)
{
	return Reader(p_text, p_maxDepth).Read(p_error);
}

std::string OvTools::Utils::Json::Write(const Value& p_value)
{
	std::string text;
	WriteValue(text, p_value, 0);
	text += '\n';
	return text;
}
