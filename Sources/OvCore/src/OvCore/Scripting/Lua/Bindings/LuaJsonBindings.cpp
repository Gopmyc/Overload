/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

#include <sol/sol.hpp>

namespace
{
	// The same ceiling as a saved file, and a depth no data worth saving reaches but a cycle always does
	constexpr size_t kMaxSize = 4 * 1024 * 1024;
	constexpr int kMaxDepth = 64;

	bool IsValidUtf8(std::string_view p_text)
	{
		size_t i = 0;

		while (i < p_text.size())
		{
			const auto lead = static_cast<unsigned char>(p_text[i]);

			if (lead < 0x80)
			{
				++i;
				continue;
			}

			size_t length = 0;
			uint32_t codePoint = 0;

			if ((lead & 0xE0) == 0xC0) { length = 2; codePoint = lead & 0x1F; }
			else if ((lead & 0xF0) == 0xE0) { length = 3; codePoint = lead & 0x0F; }
			else if ((lead & 0xF8) == 0xF0) { length = 4; codePoint = lead & 0x07; }
			else return false;

			if (i + length > p_text.size())
			{
				return false;
			}

			for (size_t k = 1; k < length; ++k)
			{
				const auto next = static_cast<unsigned char>(p_text[i + k]);

				if ((next & 0xC0) != 0x80)
				{
					return false;
				}

				codePoint = (codePoint << 6) | (next & 0x3F);
			}

			// Overlong forms, surrogates and what lies beyond Unicode are not UTF-8
			const bool overlong =
				(length == 2 && codePoint < 0x80) ||
				(length == 3 && codePoint < 0x800) ||
				(length == 4 && codePoint < 0x10000);

			if (overlong || codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF))
			{
				return false;
			}

			i += length;
		}

		return true;
	}

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
	* A Lua value as JSON. Tables are read raw (no metamethod runs); a table is a list when its keys are exactly 1 to n,
	* an object when they are all strings, written in key order so that a file only changes where its data does.
	*/
	class Encoder
	{
	public:
		Encoder(lua_State* p_state, bool p_pretty) : m_state(p_state), m_pretty(p_pretty) {}

		bool Encode(int p_index) { return Value(p_index, 0); }
		const std::string& GetText() const { return m_text; }
		const std::string& GetError() const { return m_error; }

	private:
		bool Fail(std::string_view p_message)
		{
			std::string where;

			for (const auto& step : m_path)
			{
				where += step;
			}

			m_error = where.empty() ? std::format("json: {}", p_message) : std::format("json: {}: {}", where, p_message);
			return false;
		}

		void Newline(int p_depth)
		{
			if (m_pretty)
			{
				m_text += '\n';
				m_text.append(static_cast<size_t>(p_depth), '\t');
			}
		}

		bool Value(int p_index, int p_depth)
		{
			if (m_text.size() > kMaxSize)
			{
				return Fail("too large");
			}

			switch (lua_type(m_state, p_index))
			{
			case LUA_TNIL:
				m_text += "null";
				return true;
			case LUA_TBOOLEAN:
				m_text += lua_toboolean(m_state, p_index) ? "true" : "false";
				return true;
			case LUA_TNUMBER:
				return Number(p_index);
			case LUA_TSTRING:
			{
				size_t length = 0;
				const char* text = lua_tolstring(m_state, p_index, &length);
				return String({ text, length });
			}
			case LUA_TTABLE:
				return Table(p_index, p_depth);
			default:
				return Fail(std::format("cannot encode a {}", lua_typename(m_state, lua_type(m_state, p_index))));
			}
		}

		bool Number(int p_index)
		{
			char buffer[64];

			if (lua_isinteger(m_state, p_index))
			{
				const auto result = std::to_chars(buffer, buffer + sizeof(buffer), static_cast<long long>(lua_tointeger(m_state, p_index)));
				m_text.append(buffer, result.ptr);
				return true;
			}

			const double value = lua_tonumber(m_state, p_index);

			if (!std::isfinite(value))
			{
				return Fail("a number is not finite");
			}

			// The shortest text that reads back as the same number, and still a float when read back
			const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
			const std::string_view text(buffer, static_cast<size_t>(result.ptr - buffer));

			m_text += text;

			if (text.find_first_of(".eE") == std::string_view::npos)
			{
				m_text += ".0";
			}

			return true;
		}

		bool String(std::string_view p_text)
		{
			if (!IsValidUtf8(p_text))
			{
				return Fail("a string is not valid UTF-8");
			}

			m_text += '"';

			for (const char character : p_text)
			{
				switch (character)
				{
				case '"': m_text += "\\\""; break;
				case '\\': m_text += "\\\\"; break;
				case '\b': m_text += "\\b"; break;
				case '\f': m_text += "\\f"; break;
				case '\n': m_text += "\\n"; break;
				case '\r': m_text += "\\r"; break;
				case '\t': m_text += "\\t"; break;
				default:
					if (static_cast<unsigned char>(character) < 0x20)
					{
						m_text += std::format("\\u{:04x}", static_cast<unsigned>(character));
					}
					else
					{
						m_text += character;
					}
				}
			}

			m_text += '"';
			return true;
		}

		bool Table(int p_index, int p_depth)
		{
			if (p_depth >= kMaxDepth || !lua_checkstack(m_state, 4))
			{
				return Fail("nested too deep");
			}

			const int table = lua_absindex(m_state, p_index);
			std::vector<std::string> keys;
			lua_Integer count = 0;
			lua_Integer highest = 0;
			bool indexed = false;

			lua_pushnil(m_state);

			while (lua_next(m_state, table) != 0)
			{
				lua_pop(m_state, 1);

				if (lua_type(m_state, -1) == LUA_TSTRING)
				{
					size_t length = 0;
					const char* key = lua_tolstring(m_state, -1, &length);
					keys.emplace_back(key, length);
				}
				else if (lua_isinteger(m_state, -1) && lua_tointeger(m_state, -1) >= 1)
				{
					indexed = true;
					highest = std::max(highest, lua_tointeger(m_state, -1));
				}
				else
				{
					lua_pop(m_state, 1);
					return Fail("a key must be a string or an index from 1");
				}

				++count;
			}

			if (indexed && !keys.empty())
			{
				return Fail("a table mixes string keys and indices");
			}

			if (indexed && highest != count)
			{
				return Fail("a list has holes");
			}

			if (count == 0)
			{
				m_text += "[]";
				return true;
			}

			return indexed ? List(table, count, p_depth) : Object(table, keys, p_depth);
		}

		bool List(int p_table, lua_Integer p_count, int p_depth)
		{
			m_text += '[';

			for (lua_Integer i = 1; i <= p_count; ++i)
			{
				if (i > 1)
				{
					m_text += ',';
				}

				Newline(p_depth + 1);
				lua_rawgeti(m_state, p_table, i);
				m_path.push_back(std::format("[{}]", i));

				if (!Value(-1, p_depth + 1))
				{
					return false;
				}

				m_path.pop_back();
				lua_pop(m_state, 1);
			}

			Newline(p_depth);
			m_text += ']';
			return true;
		}

		bool Object(int p_table, std::vector<std::string>& p_keys, int p_depth)
		{
			std::sort(p_keys.begin(), p_keys.end());
			m_text += '{';

			for (size_t i = 0; i < p_keys.size(); ++i)
			{
				if (i > 0)
				{
					m_text += ',';
				}

				Newline(p_depth + 1);
				m_path.push_back(m_path.empty() ? p_keys[i] : "." + p_keys[i]);

				if (!String(p_keys[i]))
				{
					return false;
				}

				m_text += m_pretty ? ": " : ":";
				lua_pushlstring(m_state, p_keys[i].data(), p_keys[i].size());
				lua_rawget(m_state, p_table);

				if (!Value(-1, p_depth + 1))
				{
					return false;
				}

				m_path.pop_back();
				lua_pop(m_state, 1);
			}

			Newline(p_depth);
			m_text += '}';
			return true;
		}

	private:
		lua_State* m_state;
		bool m_pretty;
		std::string m_text;
		std::string m_error;
		std::vector<std::string> m_path;
	};

	/**
	* JSON as defined by RFC 8259, strictly, onto the Lua stack: nothing but data comes out of it. An object's null
	* member is left out (a Lua table cannot hold nil); a null in a list, a repeated key, a byte order mark or anything
	* after the value is refused.
	*/
	class Decoder
	{
	public:
		Decoder(lua_State* p_state, std::string_view p_text) : m_state(p_state), m_text(p_text) {}

		bool Decode()
		{
			if (m_text.size() > kMaxSize)
			{
				return Fail("too large");
			}

			SkipSpace();

			if (!Value(0))
			{
				return false;
			}

			SkipSpace();

			return m_position == m_text.size() || Fail("unexpected character after the value");
		}

		const std::string& GetError() const { return m_error; }

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

			m_error = std::format("json: line {}, column {}: {}", line, column, p_message);
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

		bool Value(int p_depth)
		{
			if (AtEnd())
			{
				return Fail("unexpected end");
			}

			switch (Peek())
			{
			case '{': return Object(p_depth);
			case '[': return List(p_depth);
			case '"':
			{
				std::string text;
				if (!String(text)) return false;
				lua_pushlstring(m_state, text.data(), text.size());
				return true;
			}
			case 't': if (!Literal("true")) return false; lua_pushboolean(m_state, 1); return true;
			case 'f': if (!Literal("false")) return false; lua_pushboolean(m_state, 0); return true;
			case 'n': if (!Literal("null")) return false; lua_pushnil(m_state); return true;
			default:
				if (Peek() == '-' || (Peek() >= '0' && Peek() <= '9'))
				{
					return Number();
				}

				return Fail("unexpected character");
			}
		}

		bool Enter(int p_depth)
		{
			if (p_depth >= kMaxDepth || !lua_checkstack(m_state, 4))
			{
				return Fail("nested too deep");
			}

			++m_position;
			SkipSpace();
			return true;
		}

		bool Object(int p_depth)
		{
			if (!Enter(p_depth))
			{
				return false;
			}

			lua_newtable(m_state);
			const int table = lua_gettop(m_state);

			if (!AtEnd() && Peek() == '}')
			{
				++m_position;
				return true;
			}

			while (true)
			{
				if (AtEnd() || Peek() != '"')
				{
					return Fail("expected a key");
				}

				const size_t keyPosition = m_position;
				std::string key;

				if (!String(key))
				{
					return false;
				}

				lua_pushlstring(m_state, key.data(), key.size());
				lua_rawget(m_state, table);
				const bool repeated = !lua_isnil(m_state, -1);
				lua_pop(m_state, 1);

				if (repeated)
				{
					m_position = keyPosition;
					return Fail("a key is repeated");
				}

				SkipSpace();

				if (AtEnd() || Peek() != ':')
				{
					return Fail("expected ':'");
				}

				++m_position;
				SkipSpace();

				lua_pushlstring(m_state, key.data(), key.size());

				if (!Value(p_depth + 1))
				{
					return false;
				}

				if (lua_isnil(m_state, -1))
				{
					lua_pop(m_state, 2);
				}
				else
				{
					lua_rawset(m_state, table);
				}

				SkipSpace();

				if (!AtEnd() && Peek() == ',')
				{
					++m_position;
					SkipSpace();
					continue;
				}

				if (!AtEnd() && Peek() == '}')
				{
					++m_position;
					return true;
				}

				return Fail("expected ',' or '}'");
			}
		}

		bool List(int p_depth)
		{
			if (!Enter(p_depth))
			{
				return false;
			}

			lua_newtable(m_state);
			const int table = lua_gettop(m_state);

			if (!AtEnd() && Peek() == ']')
			{
				++m_position;
				return true;
			}

			for (lua_Integer index = 1;; ++index)
			{
				const size_t valuePosition = m_position;

				if (!Value(p_depth + 1))
				{
					return false;
				}

				if (lua_isnil(m_state, -1))
				{
					m_position = valuePosition;
					return Fail("a list holds a null");
				}

				lua_rawseti(m_state, table, index);
				SkipSpace();

				if (!AtEnd() && Peek() == ',')
				{
					++m_position;
					SkipSpace();
					continue;
				}

				if (!AtEnd() && Peek() == ']')
				{
					++m_position;
					return true;
				}

				return Fail("expected ',' or ']'");
			}
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

		bool String(std::string& p_out)
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

		bool Number()
		{
			const size_t start = m_position;
			bool integral = true;

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
				integral = false;
				++m_position;

				if (!Digits())
				{
					return Fail("expected a digit");
				}
			}

			if (!AtEnd() && (Peek() == 'e' || Peek() == 'E'))
			{
				integral = false;
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

			const char* first = m_text.data() + start;
			const char* last = m_text.data() + m_position;

			if (integral)
			{
				long long value = 0;
				const auto result = std::from_chars(first, last, value);

				if (result.ec == std::errc{} && result.ptr == last)
				{
					lua_pushinteger(m_state, static_cast<lua_Integer>(value));
					return true;
				}
			}

			double value = 0.0;
			const auto result = std::from_chars(first, last, value);

			if (result.ec != std::errc{} || result.ptr != last || !std::isfinite(value))
			{
				m_position = start;
				return Fail("a number is out of range");
			}

			lua_pushnumber(m_state, value);
			return true;
		}

	private:
		lua_State* m_state;
		std::string_view m_text;
		size_t m_position = 0;
		std::string m_error;
	};

	int Failure(lua_State* p_state, int p_top, const std::string& p_error)
	{
		lua_settop(p_state, p_top);
		lua_pushnil(p_state);
		lua_pushlstring(p_state, p_error.data(), p_error.size());
		return 2;
	}

	int Encode(lua_State* p_state)
	{
		const int top = lua_gettop(p_state);
		Encoder encoder(p_state, lua_toboolean(p_state, 2) != 0);

		if (!encoder.Encode(1))
		{
			return Failure(p_state, top, encoder.GetError());
		}

		if (encoder.GetText().size() > kMaxSize)
		{
			return Failure(p_state, top, "json: too large");
		}

		lua_settop(p_state, top);
		lua_pushlstring(p_state, encoder.GetText().data(), encoder.GetText().size());
		return 1;
	}

	int Decode(lua_State* p_state)
	{
		const int top = lua_gettop(p_state);

		if (lua_type(p_state, 1) != LUA_TSTRING)
		{
			return Failure(p_state, top, "json: not a string");
		}

		size_t length = 0;
		const char* text = lua_tolstring(p_state, 1, &length);
		Decoder decoder(p_state, { text, length });

		if (!decoder.Decode())
		{
			return Failure(p_state, top, decoder.GetError());
		}

		return 1;
	}
}

void BindLuaJson(sol::state& p_luaState)
{
	p_luaState.create_named_table("Json",
		"Encode", &Encode,
		"Decode", &Decode
	);
}
