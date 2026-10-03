/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <any>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include <OvRendering/Entities/Camera.h>

#include <baregl/Framebuffer.h>

namespace OvRendering::Data
{
	/**
	* Defines how the descriptors of a describable are stored
	*/
	enum class EDescriptorStorage
	{
		/**
		* Hash map: references to descriptors stay valid when other descriptors are added
		*/
		STABLE,

		/**
		* Contiguous list: much cheaper to build, copy and search for a handful of descriptors,
		* but adding a descriptor can invalidate references to the other ones
		*/
		COMPACT
	};

	/**
	* An object that can be described using additional data structures (descriptors)
	*/
	template<EDescriptorStorage Storage>
	class TDescribable
	{
	public:
		/**
		* Add a descriptor
		* @param p_args (Parameter pack forwared to the extension constructor)
		*/
		template<typename T>
		void AddDescriptor(T&& p_descriptor);

		/**
		* Add or replace a descriptor (no-op if same type already exists with the same value)
		*/
		template<typename T>
		void SetDescriptor(T&& p_descriptor);

		/**
		* Remove a descriptor
		*/
		template<typename T>
		void RemoveDescriptor();

		/**
		* Remove all associated descriptors
		*/
		void ClearDescriptors();

		/**
		* Return true if the a descriptor matching the given type has been found
		*/
		template<typename T>
		bool HasDescriptor() const;

		/**
		* Retrieve the descriptor matching the given type
		* @note Fails if the descriptor doesn't exist
		*/
		template<typename T>
		const T& GetDescriptor() const;

		/**
		* Try retrieving the descriptor matching the given type
		* @param p_outDescriptor
		* @return true if the descriptor has been found
		*/
		template<typename T>
		bool TryGetDescriptor(OvTools::Utils::OptRef<const T>& p_outDescriptor) const;

	private:
		using DescriptorContainer = std::conditional_t<
			Storage == EDescriptorStorage::STABLE,
			std::unordered_map<std::type_index, std::any>,
			std::vector<std::pair<std::type_index, std::any>>
		>;

		auto FindDescriptor(const std::type_index& p_type);
		auto FindDescriptor(const std::type_index& p_type) const;

	private:
		DescriptorContainer m_descriptors;
	};

	/**
	* Describable keeping references to its descriptors valid (used by renderers, whose descriptors
	* are referenced while other descriptors are added)
	*/
	using Describable = TDescribable<EDescriptorStorage::STABLE>;

	/**
	* Describable optimized for objects that are created and copied in large numbers every frame (drawables)
	*/
	using CompactDescribable = TDescribable<EDescriptorStorage::COMPACT>;
}

#include "OvRendering/Data/Describable.inl"
