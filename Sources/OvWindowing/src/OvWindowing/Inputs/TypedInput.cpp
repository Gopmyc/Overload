/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#include <OvWindowing/Inputs/TypedInput.h>

void OvWindowing::Inputs::TypedInput::AddCharacter(uint32_t p_codePoint)
{
	const bool control = p_codePoint < 0x20 || (p_codePoint >= 0x7F && p_codePoint < 0xA0);
	const bool surrogate = p_codePoint >= 0xD800 && p_codePoint <= 0xDFFF;

	if (control || surrogate || p_codePoint > 0x10FFFF)
	{
		return;
	}

	if (p_codePoint < 0x80)
	{
		m_text += static_cast<char>(p_codePoint);
	}
	else if (p_codePoint < 0x800)
	{
		m_text += static_cast<char>(0xC0 | (p_codePoint >> 6));
		m_text += static_cast<char>(0x80 | (p_codePoint & 0x3F));
	}
	else if (p_codePoint < 0x10000)
	{
		m_text += static_cast<char>(0xE0 | (p_codePoint >> 12));
		m_text += static_cast<char>(0x80 | ((p_codePoint >> 6) & 0x3F));
		m_text += static_cast<char>(0x80 | (p_codePoint & 0x3F));
	}
	else
	{
		m_text += static_cast<char>(0xF0 | (p_codePoint >> 18));
		m_text += static_cast<char>(0x80 | ((p_codePoint >> 12) & 0x3F));
		m_text += static_cast<char>(0x80 | ((p_codePoint >> 6) & 0x3F));
		m_text += static_cast<char>(0x80 | (p_codePoint & 0x3F));
	}
}

void OvWindowing::Inputs::TypedInput::AddKeyRepeat(EKey p_key)
{
	++m_repeats[p_key];
}

const std::string& OvWindowing::Inputs::TypedInput::GetText() const
{
	return m_text;
}

int OvWindowing::Inputs::TypedInput::GetRepeatCount(EKey p_key) const
{
	const auto found = m_repeats.find(p_key);
	return found != m_repeats.end() ? found->second : 0;
}

void OvWindowing::Inputs::TypedInput::Clear()
{
	m_text.clear();
	m_repeats.clear();
}
