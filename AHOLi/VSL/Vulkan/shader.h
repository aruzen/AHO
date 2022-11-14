#pragma once
#include "../define.h"
#include "pv.h"

#include <filesystem>

#include "device.h"

namespace vsl {
	template <bool V>
	struct Shader {
		Shader(vsl::LogicalDeviceAcsessor device, std::filesystem::path path);

		std::shared_ptr<vsl::_impl::Shader_impl> _data;
	};
}