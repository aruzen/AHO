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

	struct ShaderAccessor {
		std::shared_ptr<vsl::_impl::Shader_impl> _data;
	};

	template<VSL_NAMESPACE::ShaderType _Type = ShaderType::Vertex>
	struct Shader : public ShaderAccessor {
		constexpr static ShaderType Type = _Type;

		Shader(vsl::LogicalDeviceAccessor device, std::filesystem::path path);
		Shader(vsl::LogicalDeviceAccessor device, std::filesystem::path path, std::string name);
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