#pragma once
#include "../define.h"

#include "_pimpls.h"

#include "../dimention.h"

namespace vsl::helper::dimention {
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
		return vk::ImageViewType::___VSL_JOIN3(e, VSL_DEFAULT_DIMENTION, D);
	}
}