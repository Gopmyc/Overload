/**
* @project: Overload
* @author: Overload Tech.
* @licence: MIT
*/

#pragma once

#include <cstdint>
#include <optional>

#include <OvRendering/Data/Describable.h>
#include <OvRendering/Data/Material.h>
#include <OvRendering/Resources/IMesh.h>
#include <OvTools/Utils/OptRef.h>

namespace OvRendering::Entities
{
	/**
	* Drawable entity
	*/
	struct Drawable : public Data::Describable
	{
		OvTools::Utils::OptRef<OvRendering::Resources::IMesh> mesh;
		OvTools::Utils::OptRef<OvRendering::Data::Material> material;
		Data::StateMask stateMask;
		baregl::types::EPrimitiveMode primitiveMode = baregl::types::EPrimitiveMode::TRIANGLES;
		std::optional<std::string> pass = std::nullopt;
		std::optional<Data::FeatureSet> featureSetOverride = std::nullopt;
		std::optional<uint32_t> gpuInstancesOverride = std::nullopt; // Instances to draw instead of the material's
	};
}
