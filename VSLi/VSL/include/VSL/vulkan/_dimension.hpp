#pragma once
#include "../define.hpp"

#include "_pimpls.hpp"

#include "../dimension.hpp"

namespace vsl::helper::dimension {
	template<typename D>
	consteval vk::ImageViewType toImageViewType() {
		if constexpr (std::same_as<vsl::D1, D>) {
			return vk::ImageViewType::e1D;
		}
		else if constexpr (std::same_as<vsl::D2, D>) {
			return vk::ImageViewType::e2D;
		}
		else if constexpr (std::same_as<vsl::D3, D>) {
			return vk::ImageViewType::e3D;
		}
		return vk::ImageViewType::___VSL_JOIN3(e, VSL_DEFAULT_dimension, D);
	}
}