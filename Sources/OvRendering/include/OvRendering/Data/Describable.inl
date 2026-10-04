/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <algorithm>

#include <OvDebug/Assertion.h>
#include <OvRendering/Data/Describable.h>

namespace OvRendering::Data
{
	inline std::vector<Describable::DescriptorEntry>::iterator Describable::FindDescriptor(std::type_index p_type)
	{
		return std::find_if(m_descriptors.begin(), m_descriptors.end(), [p_type](const DescriptorEntry& p_entry) {
			return p_entry.first == p_type;
		});
	}

	inline std::vector<Describable::DescriptorEntry>::const_iterator Describable::FindDescriptor(std::type_index p_type) const
	{
		return std::find_if(m_descriptors.begin(), m_descriptors.end(), [p_type](const DescriptorEntry& p_entry) {
			return p_entry.first == p_type;
		});
	}

	template<typename T>
	inline void Describable::EmplaceDescriptor(T&& p_descriptor)
	{
		// Most describables hold a few descriptors: avoid growing the vector one element at a time
		if (m_descriptors.capacity() == 0)
		{
			m_descriptors.reserve(4);
		}

		m_descriptors.emplace_back(typeid(T), std::make_shared<const std::decay_t<T>>(std::forward<T>(p_descriptor)));
	}

	template<typename T>
	inline void Describable::AddDescriptor(T&& p_descriptor)
	{
		OVASSERT(!HasDescriptor<T>(), "Descriptor already added");
		EmplaceDescriptor(std::forward<T>(p_descriptor));
	}

	template<typename T>
	inline void Describable::SetDescriptor(T&& p_descriptor)
	{
		if (auto it = FindDescriptor(typeid(T)); it != m_descriptors.end())
		{
			it->second = std::make_shared<const std::decay_t<T>>(std::forward<T>(p_descriptor));
		}
		else
		{
			EmplaceDescriptor(std::forward<T>(p_descriptor));
		}
	}

	template<typename T>
	inline void Describable::RemoveDescriptor()
	{
		OVASSERT(HasDescriptor<T>(), "Descriptor doesn't exist.");
		if (auto it = FindDescriptor(typeid(T)); it != m_descriptors.end())
		{
			m_descriptors.erase(it);
		}
	}

	template<typename T>
	inline bool Describable::HasDescriptor() const
	{
		return FindDescriptor(typeid(T)) != m_descriptors.end();
	}

	template<typename T>
	inline const T& Describable::GetDescriptor() const
	{
		auto it = FindDescriptor(typeid(T));
		OVASSERT(it != m_descriptors.end(), "Couldn't find a descriptor matching the given type T.");
		return *static_cast<const T*>(it->second.get());
	}

	template<typename T>
	inline bool Describable::TryGetDescriptor(OvTools::Utils::OptRef<const T>& p_outDescriptor) const
	{
		if (auto it = FindDescriptor(typeid(T)); it != m_descriptors.end())
		{
			p_outDescriptor = *static_cast<const T*>(it->second.get());
			return true;
		}

		return false;
	}
}
