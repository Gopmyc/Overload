/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <memory>
#include <typeindex>
#include <utility>
#include <vector>

#include <OvRendering/Entities/Camera.h>

#include <baregl/Framebuffer.h>

namespace OvRendering::Data
{
	/**
	* An object that can be described using additional data structures (descriptors)
	*/
	class Describable
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
		// Descriptors are immutable once added (they can only be replaced), so their storage
		// is shared between copies: copying a Describable (e.g. a Drawable) doesn't deep copy them.
		// The heap storage also keeps references returned by GetDescriptor() valid when adding descriptors.
		using DescriptorEntry = std::pair<std::type_index, std::shared_ptr<const void>>;

		std::vector<DescriptorEntry>::iterator FindDescriptor(std::type_index p_type);
		std::vector<DescriptorEntry>::const_iterator FindDescriptor(std::type_index p_type) const;

		template<typename T>
		void EmplaceDescriptor(T&& p_descriptor);

	private:
		// Objects only hold a handful of descriptors: a flat vector avoids hashing type names
		// on every lookup, and the node allocations of a hash map (Drawables are created every frame).
		std::vector<DescriptorEntry> m_descriptors;
	};
}

#include "OvRendering/Data/Describable.inl"
