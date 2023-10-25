#pragma once
#include "../define.h"
#include "pv.h"

#include <filesystem>
#include <functional>

#include "device.h"

namespace vsl {
	enum class ShaderType : unsigned char {
		Error,
		Vertex,
		Fragment
	};

	template<VSL_NAMESPACE::ShaderType _Type = ShaderType::Vertex>
	struct Shader {
		constexpr static ShaderType Type = _Type;

		Shader(vsl::LogicalDeviceAcsessor device, std::filesystem::path path);

		std::shared_ptr<vsl::_impl::Shader_impl> _data;
	};

	consteval ShaderType ___Get_ShaderType(std::string name) {
		if (name.contains(".vert.")) {
			return ShaderType::Vertex;
		} else if (name.contains(".frag.")) {
			return ShaderType::Fragment;
		}
		return ShaderType::Error;
	}
}