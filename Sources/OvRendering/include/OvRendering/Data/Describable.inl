/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <OvDebug/Assertion.h>
#include <OvRendering/Data/Describable.h>

namespace OvRendering::Data
{
	template<EDescriptorStorage Storage>
	template<typename T>
	inline void TDescribable<Storage>::AddDescriptor(T&& p_descriptor)
	{
		OVASSERT(!HasDescriptor<T>(), "Descriptor already added");

		if constexpr (Storage == EDescriptorStorage::STABLE)
		{
			m_descriptors.emplace(typeid(T), std::move(p_descriptor));
		}
		else
		{
			m_descriptors.emplace_back(typeid(T), std::move(p_descriptor));
		}
	}

	template<EDescriptorStorage Storage>
	template<typename T>
	inline void TDescribable<Storage>::SetDescriptor(T&& p_descriptor)
	{
		if (auto it = FindDescriptor(typeid(T)); it != m_descriptors.end())
		{
			it->second = std::move(p_descriptor);
		}
		else if constexpr (Storage == EDescriptorStorage::STABLE)
		{
			m_descriptors.emplace(typeid(T), std::move(p_descriptor));
		}
		else
		{
			m_descriptors.emplace_back(typeid(T), std::move(p_descriptor));
		}
	}

	template<EDescriptorStorage Storage>
	template<typename T>
	inline void TDescribable<Storage>::RemoveDescriptor()
	{
		OVASSERT(HasDescriptor<T>(), "Descriptor doesn't exist.");
		if (auto it = FindDescriptor(typeid(T)); it != m_descriptors.end())
		{
			m_descriptors.erase(it);
		}
	}

	template<EDescriptorStorage Storage>
	inline void TDescribable<Storage>::ClearDescriptors()
	{
		m_descriptors.clear();
	}

	template<EDescriptorStorage Storage>
	template<typename T>
	inline bool TDescribable<Storage>::HasDescriptor() const
	{
		return FindDescriptor(typeid(T)) != m_descriptors.end();
	}

	template<EDescriptorStorage Storage>
	template<typename T>
	inline const T& TDescribable<Storage>::GetDescriptor() const
	{
		auto it = FindDescriptor(typeid(T));
		OVASSERT(it != m_descriptors.end(), "Couldn't find a descriptor matching the given type T.");
		return std::any_cast<const T&>(it->second);
	}

	template<EDescriptorStorage Storage>
	template<typename T>
	inline bool TDescribable<Storage>::TryGetDescriptor(OvTools::Utils::OptRef<const T>& p_outDescriptor) const
	{
		if (auto it = FindDescriptor(typeid(T)); it != m_descriptors.end())
		{
			p_outDescriptor = std::any_cast<const T&>(it->second);
			return true;
		}

		return false;
	}

	template<EDescriptorStorage Storage>
	inline auto TDescribable<Storage>::FindDescriptor(const std::type_index& p_type)
	{
		if constexpr (Storage == EDescriptorStorage::STABLE)
		{
			return m_descriptors.find(p_type);
		}
		else
		{
			auto it = m_descriptors.begin();
			while (it != m_descriptors.end() && it->first != p_type) { ++it; }
			return it;
		}
	}

	template<EDescriptorStorage Storage>
	inline auto TDescribable<Storage>::FindDescriptor(const std::type_index& p_type) const
	{
		if constexpr (Storage == EDescriptorStorage::STABLE)
		{
			return m_descriptors.find(p_type);
		}
		else
		{
			auto it = m_descriptors.begin();
			while (it != m_descriptors.end() && it->first != p_type) { ++it; }
			return it;
		}
	}
}
